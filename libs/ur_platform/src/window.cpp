#include "ur/platform/window.hpp"

#include "ur/platform/detail/qt_window_access.hpp"

#include <QCloseEvent>
#include <QExposeEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWindow>

#include <deque>
#include <limits>
#include <stdexcept>

namespace ur::platform {
namespace {

[[nodiscard]] std::uint32_t checkedDimension(int value) {
    if (value < 0) {
        throw std::runtime_error("ur_platform: negative window dimension");
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] int checkedQtDimension(std::uint32_t value) {
    constexpr auto kMax = static_cast<std::uint32_t>(std::numeric_limits<int>::max());
    if (value > kMax) {
        throw std::overflow_error("ur_platform: window dimension exceeds Qt int range");
    }
    return static_cast<int>(value);
}

[[nodiscard]] PointerButton translateButton(Qt::MouseButton button) noexcept {
    switch (button) {
        case Qt::LeftButton:
            return PointerButton::Left;
        case Qt::RightButton:
            return PointerButton::Right;
        case Qt::MiddleButton:
            return PointerButton::Middle;
        default:
            return PointerButton::Other;
    }
}

class PlatformQWindow final : public QWindow {
public:
    explicit PlatformQWindow(std::deque<WindowEvent>& events) : events_(events) {}

protected:
    void exposeEvent(QExposeEvent* event) override {
        const QSize currentSize = size();
        events_.push_back(WindowEvent{
            .type = WindowEventType::Exposed,
            .size = {checkedDimension(currentSize.width()), checkedDimension(currentSize.height())},
        });
        QWindow::exposeEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override {
        const QSize newSize = event->size();
        events_.push_back(WindowEvent{
            .type = WindowEventType::Resized,
            .size = {checkedDimension(newSize.width()), checkedDimension(newSize.height())},
        });
        QWindow::resizeEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        const QPointF position = event->position();
        events_.push_back(WindowEvent{
            .type = WindowEventType::PointerMoved,
            .x = static_cast<float>(position.x()),
            .y = static_cast<float>(position.y()),
        });
        QWindow::mouseMoveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        pushButtonEvent(*event, true);
        QWindow::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        pushButtonEvent(*event, false);
        QWindow::mouseReleaseEvent(event);
    }

    void focusOutEvent(QFocusEvent* event) override {
        events_.push_back(WindowEvent{.type = WindowEventType::FocusLost});
        QWindow::focusOutEvent(event);
    }

    void closeEvent(QCloseEvent* event) override {
        events_.push_back(WindowEvent{.type = WindowEventType::CloseRequested});
        QWindow::closeEvent(event);
    }

private:
    void pushButtonEvent(const QMouseEvent& event, bool pressed) {
        const QPointF position = event.position();
        events_.push_back(WindowEvent{
            .type = WindowEventType::PointerButtonChanged,
            .x = static_cast<float>(position.x()),
            .y = static_cast<float>(position.y()),
            .button = translateButton(event.button()),
            .pressed = pressed,
        });
    }

    std::deque<WindowEvent>& events_;
};

}  // namespace

struct Window::Impl {
    std::deque<WindowEvent> events;
    PlatformQWindow qwindow{events};
};

Window::Window() : impl_(std::make_unique<Impl>()) {
    impl_->qwindow.setSurfaceType(QSurface::VulkanSurface);
}

Window::~Window() = default;
Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

void Window::show() {
    impl_->qwindow.show();
}

void Window::resize(WindowSize size) {
    impl_->qwindow.resize(checkedQtDimension(size.width), checkedQtDimension(size.height));
}

WindowSize Window::size() const {
    const QSize currentSize = impl_->qwindow.size();
    return WindowSize{checkedDimension(currentSize.width()), checkedDimension(currentSize.height())};
}

bool Window::isExposed() const {
    return impl_->qwindow.isExposed();
}

bool Window::pollEvent(WindowEvent& event) {
    if (impl_->events.empty()) {
        return false;
    }
    event = impl_->events.front();
    impl_->events.pop_front();
    return true;
}

QWindow* detail::WindowAccess::qtWindow(Window& window) noexcept {
    return &window.impl_->qwindow;
}

}  // namespace ur::platform
