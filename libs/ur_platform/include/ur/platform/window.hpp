#pragma once

#include "ur/platform/events.hpp"

#include <memory>

namespace ur::platform {

namespace detail {
struct WindowAccess;
}

/// Minimal QWindow owner. Qt types stay out of this public header; ur_gfx uses
/// the implementation-only detail::WindowAccess bridge when a real QRhi
/// swapchain is required.
class Window {
public:
    Window();
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    void show();
    void resize(WindowSize size);
    [[nodiscard]] WindowSize size() const;
    [[nodiscard]] bool isExposed() const;

    /// Pops one translated platform event in FIFO order.
    [[nodiscard]] bool pollEvent(WindowEvent& event);

private:
    friend struct detail::WindowAccess;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ur::platform
