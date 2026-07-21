#include "ur/gfx/render_device.hpp"

#include <QGuiApplication>
#include <array>
#include <gtest/gtest.h>

class QtEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        static int argc = 1;
        static char arg0[] = "ur_gfx_tests";
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

TEST(RenderDeviceTest, VulkanBackendRendersClearColorAtCorners) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    ASSERT_NE(device, nullptr);

    const auto frame = device->renderDebugTriangleToTexture({400, 300});
    ASSERT_EQ(frame.size.width, 400u);
    ASSERT_EQ(frame.size.height, 300u);

    // 四角都在三角形轮廓外,应该是清屏色 (20, 20, 30)。
    EXPECT_EQ(pixelAt(frame, 5, 5, 0), 20);
    EXPECT_EQ(pixelAt(frame, 5, 5, 1), 20);
    EXPECT_EQ(pixelAt(frame, 5, 5, 2), 30);
}

TEST(RenderDeviceTest, VulkanBackendReportsDriverName) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    EXPECT_FALSE(device->driverName().empty());
}

TEST(RenderDeviceTest, RenderRectsPlacesColorAtExpectedPixels) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    ASSERT_NE(device, nullptr);

    // 一个红色矩形,占据画布左上角 [10,10] 到 [50,50)(逻辑像素,Y 向下)。
    const std::vector<ur::gfx::RectPrimitive> rects = {
        {10.0F, 10.0F, 40.0F, 40.0F, 1.0F, 0.0F, 0.0F, 1.0F},
    };
    const auto frame = device->renderRects(rects, {200, 200});
    ASSERT_EQ(frame.size.width, 200u);
    ASSERT_EQ(frame.size.height, 200u);

    // 矩形内部应该是红色
    EXPECT_EQ(pixelAt(frame, 30, 30, 0), 255);
    EXPECT_EQ(pixelAt(frame, 30, 30, 1), 0);
    EXPECT_EQ(pixelAt(frame, 30, 30, 2), 0);

    // 矩形外部(比如右下角)应该是清屏色 (20,20,30)
    EXPECT_EQ(pixelAt(frame, 190, 190, 0), 20);
    EXPECT_EQ(pixelAt(frame, 190, 190, 1), 20);
    EXPECT_EQ(pixelAt(frame, 190, 190, 2), 30);

    // 左上角原点验证: 矩形从 (10,10) 开始,(5,5) 应该还在矩形外面
    EXPECT_EQ(pixelAt(frame, 5, 5, 0), 20);
}

TEST(RenderDeviceTest, RenderRectsWithEmptyListStillProducesClearedFrame) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    const auto frame = device->renderRects({}, {100, 100});
    EXPECT_EQ(pixelAt(frame, 50, 50, 0), 20);
    EXPECT_EQ(pixelAt(frame, 50, 50, 2), 30);
}

TEST(RenderDeviceTest, RenderRectsDrawsMultipleNonOverlappingRects) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    const std::vector<ur::gfx::RectPrimitive> rects = {
        {0.0F, 0.0F, 20.0F, 20.0F, 1.0F, 0.0F, 0.0F, 1.0F},    // 红,左上
        {80.0F, 80.0F, 20.0F, 20.0F, 0.0F, 1.0F, 0.0F, 1.0F},  // 绿,右下
    };
    const auto frame = device->renderRects(rects, {100, 100});

    EXPECT_EQ(pixelAt(frame, 10, 10, 0), 255);  // 红色矩形内部
    EXPECT_EQ(pixelAt(frame, 10, 10, 1), 0);

    EXPECT_EQ(pixelAt(frame, 90, 90, 0), 0);  // 绿色矩形内部
    EXPECT_EQ(pixelAt(frame, 90, 90, 1), 255);
}

TEST(RenderDeviceTest, GenericUiFramePreservesRectanglePixels) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    ur::gfx::UiFrame ui;
    ui.primitives.emplace_back(
        ur::gfx::RectPrimitive{12.0F, 14.0F, 30.0F, 24.0F, 0.0F, 0.0F, 1.0F, 1.0F});

    const auto frame = device->renderUiFrame(ui, {80U, 80U});
    EXPECT_EQ(pixelAt(frame, 20U, 20U, 0U), 0U);
    EXPECT_EQ(pixelAt(frame, 20U, 20U, 1U), 0U);
    EXPECT_EQ(pixelAt(frame, 20U, 20U, 2U), 255U);
}

TEST(RenderDeviceTest, GenericUiFrameRejectsInvalidAlphaAtlasContracts) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);
    const ur::gfx::MaskedQuadPrimitive quad{
        0.0F, 0.0F, 10.0F, 10.0F, 0U, 0U, 2U, 2U, 1.0F, 1.0F, 1.0F, 1.0F,
    };

    ur::gfx::UiFrame missingAtlas;
    missingAtlas.primitives.emplace_back(quad);
    EXPECT_THROW(static_cast<void>(device->renderUiFrame(missingAtlas, {32U, 32U})),
                 std::invalid_argument);

    const std::array<std::uint8_t, 3U> wrongPixels{255U, 255U, 255U};
    ur::gfx::UiFrame wrongSize;
    wrongSize.alphaAtlas = ur::gfx::AlphaAtlasView{{2U, 2U}, wrongPixels, 1U};
    EXPECT_THROW(static_cast<void>(device->renderUiFrame(wrongSize, {32U, 32U})),
                 std::invalid_argument);

    const std::array<std::uint8_t, 4U> validPixels{255U, 255U, 255U, 255U};
    ur::gfx::UiFrame outOfBounds;
    outOfBounds.alphaAtlas = ur::gfx::AlphaAtlasView{{2U, 2U}, validPixels, 1U};
    auto outside = quad;
    outside.atlasX = 1U;
    outside.atlasWidth = 2U;
    outOfBounds.primitives.emplace_back(outside);
    EXPECT_THROW(static_cast<void>(device->renderUiFrame(outOfBounds, {32U, 32U})),
                 std::invalid_argument);
}


TEST(RenderDeviceTest, MaskedQuadSamplesOpaqueAndTransparentCoverage) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);

    const std::array<std::uint8_t, 2U> atlasPixels{255U, 0U};
    ur::gfx::UiFrame ui;
    ui.alphaAtlas = ur::gfx::AlphaAtlasView{{2U, 1U}, atlasPixels, 1U};
    ui.primitives.emplace_back(ur::gfx::MaskedQuadPrimitive{
        10.0F, 10.0F, 20.0F, 20.0F, 0U, 0U, 1U, 1U, 1.0F, 0.0F, 0.0F, 1.0F,
    });
    ui.primitives.emplace_back(ur::gfx::MaskedQuadPrimitive{
        40.0F, 10.0F, 20.0F, 20.0F, 1U, 0U, 1U, 1U, 0.0F, 1.0F, 0.0F, 1.0F,
    });

    const auto frame = device->renderUiFrame(ui, {80U, 50U});
    EXPECT_GT(pixelAt(frame, 20U, 20U, 0U), 245U);
    EXPECT_LT(pixelAt(frame, 20U, 20U, 1U), 10U);
    EXPECT_EQ(pixelAt(frame, 50U, 20U, 0U), 20U);
    EXPECT_EQ(pixelAt(frame, 50U, 20U, 1U), 20U);
    EXPECT_EQ(pixelAt(frame, 50U, 20U, 2U), 30U);
    EXPECT_EQ(device->statistics().alphaAtlasUploadCount, 1U);
}

TEST(RenderDeviceTest, MaskedQuadAppliesTintAndSourceAlpha) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);

    const std::array<std::uint8_t, 1U> atlasPixels{255U};
    ur::gfx::UiFrame ui;
    ui.alphaAtlas = ur::gfx::AlphaAtlasView{{1U, 1U}, atlasPixels, 7U};
    ui.primitives.emplace_back(ur::gfx::MaskedQuadPrimitive{
        8.0F, 8.0F, 24.0F, 24.0F, 0U, 0U, 1U, 1U, 0.0F, 0.0F, 1.0F, 0.5F,
    });

    const auto frame = device->renderUiFrame(ui, {48U, 48U});
    EXPECT_NEAR(pixelAt(frame, 16U, 16U, 0U), 10U, 2U);
    EXPECT_NEAR(pixelAt(frame, 16U, 16U, 1U), 10U, 2U);
    EXPECT_NEAR(pixelAt(frame, 16U, 16U, 2U), 143U, 3U);
}

TEST(RenderDeviceTest, GenericUiFramePreservesRectMaskedRectSubmissionOrder) {
    auto device = ur::gfx::RenderDevice::createOffscreen(ur::gfx::Backend::Vulkan);

    const std::array<std::uint8_t, 1U> atlasPixels{255U};
    ur::gfx::UiFrame ui;
    ui.alphaAtlas = ur::gfx::AlphaAtlasView{{1U, 1U}, atlasPixels, 1U};
    ui.primitives.emplace_back(
        ur::gfx::RectPrimitive{0.0F, 0.0F, 40.0F, 40.0F, 0.0F, 0.0F, 1.0F, 1.0F});
    ui.primitives.emplace_back(ur::gfx::MaskedQuadPrimitive{
        10.0F, 10.0F, 30.0F, 30.0F, 0U, 0U, 1U, 1U, 1.0F, 0.0F, 0.0F, 1.0F,
    });
    ui.primitives.emplace_back(
        ur::gfx::RectPrimitive{20.0F, 20.0F, 20.0F, 20.0F, 0.0F, 1.0F, 0.0F, 1.0F});

    const auto frame = device->renderUiFrame(ui, {50U, 50U});
    EXPECT_GT(pixelAt(frame, 5U, 5U, 2U), 245U);
    EXPECT_GT(pixelAt(frame, 15U, 15U, 0U), 245U);
    EXPECT_GT(pixelAt(frame, 30U, 30U, 1U), 245U);
    EXPECT_LT(pixelAt(frame, 30U, 30U, 0U), 10U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new QtEnvironment());
    return RUN_ALL_TESTS();
}
