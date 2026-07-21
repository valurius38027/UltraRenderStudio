#pragma once

#include "ur/gfx/render_device.hpp"
#include "ur/widgets/draw_list.hpp"

namespace ur::widgets {

/// 把 DrawList 翻译成 ur_gfx 的 RectPrimitive 词汇表并提交渲染。
/// 这是 ur_widgets 依赖 ur_gfx(而不是反过来)这条分层规则的具体体现 ——
/// 翻译逻辑放在 ur_widgets 这一侧,ur_gfx 完全不知道 DrawList 的存在。
[[nodiscard]] ur::gfx::FrameReadback renderDrawList(const DrawList& drawList,
                                                     ur::gfx::RenderDevice& device,
                                                     ur::gfx::Extent2D targetSize);

}  // namespace ur::widgets
