#include "ur/gfx/render_device.hpp"

#include <QGuiApplication>
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new QtEnvironment());
    return RUN_ALL_TESTS();
}
