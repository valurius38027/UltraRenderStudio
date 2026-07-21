#pragma once

#include "ur/platform/window.hpp"

class QWindow;

namespace ur::platform::detail {

/// Private implementation bridge. Only platform integrations such as ur_gfx
/// should include this header; public application code uses Window directly.
struct WindowAccess {
    [[nodiscard]] static QWindow* qtWindow(Window& window) noexcept;
};

}  // namespace ur::platform::detail
