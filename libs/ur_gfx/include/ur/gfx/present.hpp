#pragma once

namespace ur::gfx {

/// Result of attempting to render and present one window frame.
enum class PresentResult {
    Presented,
    SkippedNotExposed,
    Resized,
    DeviceLost,
};

}  // namespace ur::gfx
