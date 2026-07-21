#pragma once

#include <cstdint>

namespace ur::platform {

/// Window extent in Qt logical pixels. The renderer queries the QWindow's
/// device-pixel size when creating or resizing the swapchain.
struct WindowSize {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

enum class WindowEventType {
    Exposed,
    Resized,
    PointerMoved,
    PointerButtonChanged,
    CloseRequested,
    FocusLost,
};

enum class PointerButton {
    Left,
    Right,
    Middle,
    Other,
};

/// Backend-neutral event value emitted by ur_platform. Fields not used by a
/// given event type retain their default values.
struct WindowEvent {
    WindowEventType type = WindowEventType::Exposed;
    WindowSize size{};
    float x = 0.0F;
    float y = 0.0F;
    PointerButton button = PointerButton::Other;
    bool pressed = false;
};

}  // namespace ur::platform
