#pragma once

#include "ur/widgets/geometry.hpp"

#include <cstddef>
#include <vector>

namespace ur::widgets {

/// 单条绘制命令。当前只有矩形一种(文字命令等 ur_text 有真实实现后再加)。
/// 这是纯数据结构,DrawList 本身不知道也不关心 QRhi/Vulkan —— 这层解耦
/// 保证 ur_widgets 的单元测试完全不需要真实渲染设备,详见 ADR-001 里
/// "立即模式部件库和渲染后端解耦"的分层要求。
struct RectCommand {
    Rect rect;
    Color color;
};

/// 一帧内所有部件产生的绘制命令,按提交顺序排列(顺序即 Z 序,后提交的在上层,
/// 和 Context 的 hover 判定"后调用者优先"是同一个假设,必须保持一致)。
class DrawList {
public:
    void addRect(Rect rect, Color color) { commands_.push_back(RectCommand{rect, color}); }

    void clear() { commands_.clear(); }

    void setRectColor(std::size_t index, Color color) { commands_.at(index).color = color; }

    [[nodiscard]] const std::vector<RectCommand>& commands() const { return commands_; }

private:
    std::vector<RectCommand> commands_;
};

}  // namespace ur::widgets
