#include "ur/widgets/context.hpp"
#include "ur/widgets/render.hpp"

#include <QGuiApplication>
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

    ur::widgets::Context ctx;
    ctx.beginFrame(/*mouseX=*/50.0F, /*mouseY=*/20.0F, /*mouseDown=*/false);
    ctx.button("Save", {10.0F, 10.0F, 80.0F, 30.0F});
    const auto& drawList = ctx.endFrame();

    const auto frame = ur::widgets::renderDrawList(drawList, *device, {200, 200});

    // hover 但未按下的配色是 Color{0.25, 0.25, 0.32, 1.0}(见 context.cpp),
    // 换算成 0-255: (64, 64, 82) 左右(浮点转 8bit 有舍入,允许小误差)。
    const int r = pixelAt(frame, 50, 20, 0);
    const int g = pixelAt(frame, 50, 20, 1);
    const int b = pixelAt(frame, 50, 20, 2);
    EXPECT_NEAR(r, 64, 3);
    EXPECT_NEAR(g, 64, 3);
    EXPECT_NEAR(b, 82, 3);
}

TEST(WidgetsRenderIntegrationTest, UnhoveredButtonRendersDefaultColorNotHoverColor) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    ASSERT_NE(device, nullptr);

    ur::widgets::Context ctx;
    // 鼠标完全不在按钮范围内
    ctx.beginFrame(/*mouseX=*/500.0F, /*mouseY=*/500.0F, /*mouseDown=*/false);
    ctx.button("Save", {10.0F, 10.0F, 80.0F, 30.0F});
    const auto& drawList = ctx.endFrame();

    const auto frame = ur::widgets::renderDrawList(drawList, *device, {200, 200});

    // 默认配色 Color{0.3, 0.3, 0.35, 1.0} -> 约 (77, 77, 89),
    // 明显不同于 hover 配色 (64, 64, 82),用来确认状态机真的影响了渲染结果,
    // 不是巧合碰到同一个颜色。
    const int r = pixelAt(frame, 50, 20, 0);
    EXPECT_NEAR(r, 77, 3);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new QtEnvironment());
    return RUN_ALL_TESTS();
}
