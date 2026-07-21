#include "ur/widgets/render.hpp"

namespace ur::widgets {

ur::gfx::FrameReadback renderDrawList(const DrawList& drawList, ur::gfx::RenderDevice& device,
                                       ur::gfx::Extent2D targetSize) {
    std::vector<ur::gfx::RectPrimitive> primitives;
    primitives.reserve(drawList.commands().size());

    for (const RectCommand& cmd : drawList.commands()) {
        primitives.push_back(ur::gfx::RectPrimitive{
            cmd.rect.x, cmd.rect.y, cmd.rect.width, cmd.rect.height,
            cmd.color.r, cmd.color.g, cmd.color.b, cmd.color.a,
        });
    }

    return device.renderRects(primitives, targetSize);
}

}  // namespace ur::widgets
