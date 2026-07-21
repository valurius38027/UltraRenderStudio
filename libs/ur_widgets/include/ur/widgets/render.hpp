#pragma once

#include "ur/gfx/render_device.hpp"
#include "ur/text/text.hpp"
#include "ur/widgets/draw_list.hpp"

namespace ur::widgets {

[[nodiscard]] ur::gfx::UiFrame buildUiFrame(const DrawList& drawList,
                                             ur::text::AtlasView atlas);

[[nodiscard]] ur::gfx::FrameReadback renderDrawList(const DrawList& drawList,
                                                     ur::text::AtlasView atlas,
                                                     ur::gfx::RenderDevice& device,
                                                     ur::gfx::Extent2D targetSize);

}  // namespace ur::widgets
