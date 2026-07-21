#include "ur/gfx/render_device.hpp"
#include "ur/platform/window.hpp"

#include <QGuiApplication>
#include <gtest/gtest.h>

class QtEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        static int argc = 1;
        static char arg0[] = "ur_integration_tests";
        static char* argv[] = {arg0, nullptr};
        app_ = std::make_unique<QGuiApplication>(argc, argv);
    }
    void TearDown() override { app_.reset(); }

private:
    std::unique_ptr<QGuiApplication> app_;
};

// Windowed device creation must consume the ur_platform Window object, not an
// integer-like native handle. Real presentation is covered by the dedicated
// window_present integration executable.
TEST(PlatformGfxIntegrationTest, WindowCanCreateWindowedRenderDevice) {
    ur::platform::Window window;
    window.resize({640, 480});

    auto device =
        ur::gfx::RenderDevice::createForWindow(ur::gfx::Backend::Vulkan, window);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->backend(), ur::gfx::Backend::Vulkan);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new QtEnvironment());
    return RUN_ALL_TESTS();
}
