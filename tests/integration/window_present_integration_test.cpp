#include "ur/gfx/present.hpp"
#include "ur/gfx/render_device.hpp"
#include "ur/platform/window.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QThread>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <memory>
#include <vector>

class QtEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        static int argc = 1;
        static char arg0[] = "ur_window_present_tests";
        static char* argv[] = {arg0, nullptr};
        app_ = std::make_unique<QGuiApplication>(argc, argv);
    }
    void TearDown() override { app_.reset(); }

private:
    std::unique_ptr<QGuiApplication> app_;
};

namespace {

bool waitUntilExposed(ur::platform::Window& window) {
    QElapsedTimer timer;
    timer.start();
    constexpr qint64 kTimeoutMs = 3000;
    while (!window.isExposed() && timer.elapsed() < kTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    return window.isExposed();
}

void processResizeEvents() {
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
}

}  // namespace

TEST(WindowPresentIntegrationTest, PresentsAndResizesRealSwapChain) {
    ur::platform::Window window;
    window.resize({640, 480});

    auto device =
        ur::gfx::RenderDevice::createForWindow(ur::gfx::Backend::Vulkan, window);
    ASSERT_NE(device, nullptr);

    window.show();
    ASSERT_TRUE(waitUntilExposed(window));

    device->resizeSwapChain();
    const std::vector<ur::gfx::RectPrimitive> rects = {
        {20.0F, 20.0F, 100.0F, 60.0F, 0.2F, 0.6F, 0.9F, 1.0F},
    };
    EXPECT_EQ(device->presentRects(rects), ur::gfx::PresentResult::Presented);

    window.resize({800, 600});
    processResizeEvents();
    device->resizeSwapChain();
    EXPECT_EQ(device->presentRects(rects), ur::gfx::PresentResult::Presented);
}


TEST(WindowPresentIntegrationTest, ReusesAtlasUploadAcrossFramesAndResize) {
    ur::platform::Window window;
    window.resize({320, 240});

    auto device =
        ur::gfx::RenderDevice::createForWindow(ur::gfx::Backend::Vulkan, window);
    ASSERT_NE(device, nullptr);

    window.show();
    ASSERT_TRUE(waitUntilExposed(window));
    device->resizeSwapChain();

    const std::array<std::uint8_t, 1U> atlasPixels{255U};
    ur::gfx::UiFrame frame;
    frame.alphaAtlas = ur::gfx::AlphaAtlasView{{1U, 1U}, atlasPixels, 1U};
    frame.primitives.emplace_back(ur::gfx::MaskedQuadPrimitive{
        20.0F, 20.0F, 80.0F, 40.0F, 0U, 0U, 1U, 1U, 1.0F, 1.0F, 1.0F, 1.0F,
    });

    EXPECT_EQ(device->presentUiFrame(frame), ur::gfx::PresentResult::Presented);
    EXPECT_EQ(device->statistics().alphaAtlasUploadCount, 1U);
    EXPECT_EQ(device->presentUiFrame(frame), ur::gfx::PresentResult::Presented);
    EXPECT_EQ(device->statistics().alphaAtlasUploadCount, 1U);

    frame.alphaAtlas->revision = 2U;
    EXPECT_EQ(device->presentUiFrame(frame), ur::gfx::PresentResult::Presented);
    EXPECT_EQ(device->statistics().alphaAtlasUploadCount, 2U);

    window.resize({480, 320});
    processResizeEvents();
    device->resizeSwapChain();
    EXPECT_EQ(device->presentUiFrame(frame), ur::gfx::PresentResult::Presented);
    EXPECT_EQ(device->statistics().alphaAtlasUploadCount, 2U);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new QtEnvironment());
    return RUN_ALL_TESTS();
}
