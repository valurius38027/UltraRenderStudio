#include "ur/platform/detail/qt_window_access.hpp"
#include "ur/platform/window.hpp"

#include <QCoreApplication>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWindow>
#include <gtest/gtest.h>

// QGuiApplication 必须在任何 QWindow 构造之前存在,且整个测试进程只能有一个。
class QtEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        static int argc = 1;
        static char arg0[] = "ur_platform_tests";
        static char* argv[] = {arg0, nullptr};
        app_ = std::make_unique<QGuiApplication>(argc, argv);
    }
    void TearDown() override { app_.reset(); }

private:
    std::unique_ptr<QGuiApplication> app_;
};

namespace {

void drainEvents(ur::platform::Window& window) {
    ur::platform::WindowEvent event;
    while (window.pollEvent(event)) {
    }
}

}  // namespace

TEST(WindowTest, ConstructAndResizeDoesNotCrash) {
    ur::platform::Window window;
    window.resize({800, 600});
    const auto size = window.size();
    EXPECT_EQ(size.width, 800u);
    EXPECT_EQ(size.height, 600u);
}

TEST(WindowTest, PrivateQtWindowBridgeIsQueryable) {
    ur::platform::Window window;
    EXPECT_NE(ur::platform::detail::WindowAccess::qtWindow(window), nullptr);
}


TEST(WindowEventTest, TranslatesResizeMoveAndPressInFifoOrder) {
    ur::platform::Window window;
    drainEvents(window);
    QWindow* qwindow = ur::platform::detail::WindowAccess::qtWindow(window);
    ASSERT_NE(qwindow, nullptr);

    QResizeEvent resizeEvent(QSize(900, 700), QSize(640, 480));
    QCoreApplication::sendEvent(qwindow, &resizeEvent);

    const QPointF pointerPosition(31.5, 42.25);
    QMouseEvent moveEvent(QEvent::MouseMove, pointerPosition, pointerPosition, pointerPosition,
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(qwindow, &moveEvent);

    QMouseEvent pressEvent(QEvent::MouseButtonPress, pointerPosition, pointerPosition,
                           pointerPosition, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(qwindow, &pressEvent);

    ur::platform::WindowEvent event;
    ASSERT_TRUE(window.pollEvent(event));
    EXPECT_EQ(event.type, ur::platform::WindowEventType::Resized);
    EXPECT_EQ(event.size.width, 900u);
    EXPECT_EQ(event.size.height, 700u);

    ASSERT_TRUE(window.pollEvent(event));
    EXPECT_EQ(event.type, ur::platform::WindowEventType::PointerMoved);
    EXPECT_FLOAT_EQ(event.x, 31.5F);
    EXPECT_FLOAT_EQ(event.y, 42.25F);

    ASSERT_TRUE(window.pollEvent(event));
    EXPECT_EQ(event.type, ur::platform::WindowEventType::PointerButtonChanged);
    EXPECT_EQ(event.button, ur::platform::PointerButton::Left);
    EXPECT_TRUE(event.pressed);
    EXPECT_FALSE(window.pollEvent(event));
}

TEST(WindowEventTest, FocusLossIsQueuedAndClearsAtConsumerBoundary) {
    ur::platform::Window window;
    drainEvents(window);
    QWindow* qwindow = ur::platform::detail::WindowAccess::qtWindow(window);
    ASSERT_NE(qwindow, nullptr);

    QFocusEvent focusOut(QEvent::FocusOut, Qt::ActiveWindowFocusReason);
    QCoreApplication::sendEvent(qwindow, &focusOut);

    ur::platform::WindowEvent event;
    ASSERT_TRUE(window.pollEvent(event));
    EXPECT_EQ(event.type, ur::platform::WindowEventType::FocusLost);
    EXPECT_FALSE(window.pollEvent(event));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new QtEnvironment());
    return RUN_ALL_TESTS();
}
