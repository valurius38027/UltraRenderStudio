#include "ur/widgets/context.hpp"
#include "ur/widgets/render.hpp"

#include <QGuiApplication>
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>

class QtEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        static int argc = 1;
        static char arg0[] = "ur_widgets_render_tests";
        static char* argv[] = {arg0, nullptr};
        app_ = std::make_unique<QGuiApplication>(argc, argv);
    }
    void TearDown() override { app_.reset(); }

private:
    std::unique_ptr<QGuiApplication> app_;
};

namespace {
constexpr const char* kFontPath = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

std::uint8_t pixelAt(const ur::gfx::FrameReadback& frame, std::uint32_t x, std::uint32_t y,
                      std::uint32_t channel) {
    const std::size_t idx = (static_cast<std::size_t>(y) * frame.size.width + x) * 4 + channel;
    return frame.pixels.at(idx);
}

}  // namespace

// 端到端: button() 的 hover 配色(context.cpp 里定义)经过 DrawList ->
// RectPrimitive 翻译 -> 真实 QRhi/Vulkan 渲染 -> 像素回读,颜色要能对上。
// 这条测试贯穿 ur_widgets 的状态机逻辑和 ur_gfx 的渲染管线,是 ADR-007
// 规则一"必须被下游真正消费并产生可观察效果"的具体验证。
TEST(WidgetsRenderIntegrationTest, HoveredButtonRendersHoverColor) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    ASSERT_NE(device, nullptr);

    ur::text::TextSystem text;
    const auto font = text.loadFont({kFontPath, 16U});
    ur::widgets::Context ctx(text, font);
    ctx.beginFrame(/*mouseX=*/50.0F, /*mouseY=*/20.0F, /*mouseDown=*/false);
    ctx.button("Save", {10.0F, 10.0F, 80.0F, 30.0F});
    const auto& drawList = ctx.endFrame();

    const auto frame = ur::widgets::renderDrawList(drawList, text.atlas(), *device, {200, 200});

    // hover 但未按下的配色是 Color{0.25, 0.25, 0.32, 1.0}(见 context.cpp),
    // 换算成 0-255: (64, 64, 82) 左右(浮点转 8bit 有舍入,允许小误差)。
    const int r = pixelAt(frame, 15, 15, 0);
    const int g = pixelAt(frame, 15, 15, 1);
    const int b = pixelAt(frame, 15, 15, 2);
    EXPECT_NEAR(r, 64, 3);
    EXPECT_NEAR(g, 64, 3);
    EXPECT_NEAR(b, 82, 3);
}

TEST(WidgetsRenderIntegrationTest, UnhoveredButtonRendersDefaultColorNotHoverColor) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    ASSERT_NE(device, nullptr);

    ur::text::TextSystem text;
    const auto font = text.loadFont({kFontPath, 16U});
    ur::widgets::Context ctx(text, font);
    // 鼠标完全不在按钮范围内
    ctx.beginFrame(/*mouseX=*/500.0F, /*mouseY=*/500.0F, /*mouseDown=*/false);
    ctx.button("Save", {10.0F, 10.0F, 80.0F, 30.0F});
    const auto& drawList = ctx.endFrame();

    const auto frame = ur::widgets::renderDrawList(drawList, text.atlas(), *device, {200, 200});

    // 默认配色 Color{0.3, 0.3, 0.35, 1.0} -> 约 (77, 77, 89),
    // 明显不同于 hover 配色 (64, 64, 82),用来确认状态机真的影响了渲染结果,
    // 不是巧合碰到同一个颜色。
    const int r = pixelAt(frame, 15, 15, 0);
    EXPECT_NEAR(r, 77, 3);
}


TEST(WidgetsRenderIntegrationTest, Utf8LabelProducesForegroundPixelsThroughCompleteTextPath) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    ASSERT_NE(device, nullptr);

    ur::text::TextSystem text;
    const auto font = text.loadFont({kFontPath, 16U});
    ur::widgets::Context ctx(text, font);
    ctx.beginFrame(/*mouseX=*/500.0F, /*mouseY=*/500.0F, /*mouseDown=*/false);
    static_cast<void>(ctx.button("Text Ω", {10.0F, 10.0F, 160.0F, 40.0F}));
    const auto& drawList = ctx.endFrame();
    ASSERT_EQ(drawList.commands().size(), 2U);
    const auto& textCommand = std::get<ur::widgets::TextCommand>(drawList.commands()[1]);
    ASSERT_FALSE(textCommand.layout.glyphs.empty());
    ASSERT_GT(text.atlas().revision, 0U);

    const auto frame = ur::widgets::renderDrawList(drawList, text.atlas(), *device, {220U, 100U});
    std::size_t foregroundPixels = 0U;
    for (const ur::text::PositionedGlyph& glyph : textCommand.layout.glyphs) {
        if (glyph.bitmapBounds.width <= 0.0F || glyph.bitmapBounds.height <= 0.0F) {
            continue;
        }
        const int left = std::max(0, static_cast<int>(std::floor(
                                         textCommand.originX + glyph.bitmapBounds.x)));
        const int top = std::max(0, static_cast<int>(std::floor(
                                        textCommand.baselineY + glyph.bitmapBounds.y)));
        const int right = std::min(220, static_cast<int>(std::ceil(
                                           textCommand.originX + glyph.bitmapBounds.x +
                                           glyph.bitmapBounds.width)));
        const int bottom = std::min(100, static_cast<int>(std::ceil(
                                            textCommand.baselineY + glyph.bitmapBounds.y +
                                            glyph.bitmapBounds.height)));
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                const auto red = pixelAt(frame, static_cast<std::uint32_t>(x),
                                         static_cast<std::uint32_t>(y), 0U);
                const auto green = pixelAt(frame, static_cast<std::uint32_t>(x),
                                           static_cast<std::uint32_t>(y), 1U);
                const auto blue = pixelAt(frame, static_cast<std::uint32_t>(x),
                                          static_cast<std::uint32_t>(y), 2U);
                if (red > 130U && green > 130U && blue > 130U) {
                    ++foregroundPixels;
                }
            }
        }
    }
    EXPECT_GT(foregroundPixels, 10U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new QtEnvironment());
    return RUN_ALL_TESTS();
}
