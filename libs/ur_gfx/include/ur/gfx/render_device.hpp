#pragma once

#include "ur/gfx/present.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
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
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;

    bool operator==(const Extent2D&) const = default;
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

struct AlphaAtlasView {
    Extent2D size;
    std::span<const std::uint8_t> pixels;
    std::uint64_t revision = 0U;
};

struct MaskedQuadPrimitive {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
    std::uint32_t atlasX = 0U;
    std::uint32_t atlasY = 0U;
    std::uint32_t atlasWidth = 0U;
    std::uint32_t atlasHeight = 0U;
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

using UiPrimitive = std::variant<RectPrimitive, MaskedQuadPrimitive>;

struct UiFrame {
    std::vector<UiPrimitive> primitives;
    std::optional<AlphaAtlasView> alphaAtlas;
};

struct RenderStatistics {
    std::uint64_t alphaAtlasUploadCount = 0U;
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
    [[nodiscard]] virtual RenderStatistics statistics() const = 0;

    /// Synchronously consumes the frame and any borrowed atlas pixels.
    virtual FrameReadback renderUiFrame(const UiFrame& frame, Extent2D targetSize) = 0;

    /// Compatibility wrapper retained while rectangle-only callers migrate.
    FrameReadback renderRects(const std::vector<RectPrimitive>& rects, Extent2D targetSize);

    virtual FrameReadback renderDebugTriangleToTexture(Extent2D size) = 0;

    /// Recreates or resizes the swapchain for a windowed device. Calling this
    /// on an offscreen device is a logic error.
    virtual void resizeSwapChain() = 0;

    /// Synchronously consumes and presents one ordered UI frame.
    virtual PresentResult presentUiFrame(const UiFrame& frame) = 0;

    /// Compatibility wrapper retained while rectangle-only callers migrate.
    PresentResult presentRects(const std::vector<RectPrimitive>& rects);
};

}  // namespace ur::gfx
