#pragma once

#include "ur/gfx/present.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ur::platform {
class Window;
}

namespace ur::gfx {

enum class Backend {
    Vulkan,
    D3D11,
    D3D12,
    Metal,
};

struct Extent2D {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct FrameReadback {
    std::vector<std::uint8_t> pixels;
    Extent2D size;
};

struct RectPrimitive {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
};

class RenderDevice {
public:
    /// Creates a QRhi device with no swapchain. This remains the deterministic
    /// path for pixel-readback and render-regression tests.
    static std::unique_ptr<RenderDevice> createOffscreen(Backend backend);

    /// Creates a QRhi device associated with a real platform window. The
    /// window must outlive the returned device.
    static std::unique_ptr<RenderDevice> createForWindow(Backend backend,
                                                          ur::platform::Window& window);

    virtual ~RenderDevice() = default;

    [[nodiscard]] virtual Backend backend() const = 0;
    [[nodiscard]] virtual std::string driverName() const = 0;

    virtual FrameReadback renderRects(const std::vector<RectPrimitive>& rects,
                                       Extent2D targetSize) = 0;
    virtual FrameReadback renderDebugTriangleToTexture(Extent2D size) = 0;

    /// Recreates or resizes the swapchain for a windowed device. Calling this
    /// on an offscreen device is a logic error.
    virtual void resizeSwapChain() = 0;

    /// Renders rectangles in logical window coordinates and presents them.
    virtual PresentResult presentRects(const std::vector<RectPrimitive>& rects) = 0;
};

}  // namespace ur::gfx
