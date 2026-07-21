#include "ur/widgets/render.hpp"

#include <variant>

namespace ur::widgets {

ur::gfx::UiFrame buildUiFrame(const DrawList& drawList, ur::text::AtlasView atlas) {
    ur::gfx::UiFrame frame;
    frame.alphaAtlas = ur::gfx::AlphaAtlasView{
        {atlas.width, atlas.height}, atlas.pixels, atlas.revision,
    };

    for (const DrawCommand& command : drawList.commands()) {
        if (const auto* rect = std::get_if<RectCommand>(&command)) {
            frame.primitives.emplace_back(ur::gfx::RectPrimitive{
                rect->rect.x, rect->rect.y, rect->rect.width, rect->rect.height,
                rect->color.r, rect->color.g, rect->color.b, rect->color.a,
            });
            continue;
        }

        const TextCommand& text = std::get<TextCommand>(command);
        for (const ur::text::PositionedGlyph& glyph : text.layout.glyphs) {
            if (glyph.atlasRect.width == 0U || glyph.atlasRect.height == 0U) {
                continue;
            }
            frame.primitives.emplace_back(ur::gfx::MaskedQuadPrimitive{
                text.originX + glyph.bitmapBounds.x,
                text.baselineY + glyph.bitmapBounds.y,
                glyph.bitmapBounds.width,
                glyph.bitmapBounds.height,
                glyph.atlasRect.x,
                glyph.atlasRect.y,
                glyph.atlasRect.width,
                glyph.atlasRect.height,
                text.color.r,
                text.color.g,
                text.color.b,
                text.color.a,
            });
        }
    }
    return frame;
}

ur::gfx::FrameReadback renderDrawList(const DrawList& drawList, ur::text::AtlasView atlas,
                                       ur::gfx::RenderDevice& device,
                                       ur::gfx::Extent2D targetSize) {
    return device.renderUiFrame(buildUiFrame(drawList, atlas), targetSize);
}

}  // namespace ur::widgets
