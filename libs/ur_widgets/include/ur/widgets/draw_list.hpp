#pragma once

#include "ur/text/text.hpp"
#include "ur/widgets/geometry.hpp"

#include <cstddef>
#include <stdexcept>
#include <variant>
#include <vector>

namespace ur::widgets {

struct RectCommand {
    Rect rect;
    Color color;
};

struct TextCommand {
    ur::text::TextLayout layout;
    float originX = 0.0F;
    float baselineY = 0.0F;
    Color color;
};

using DrawCommand = std::variant<RectCommand, TextCommand>;

/// Ordered frame command stream. Submission order is Z order.
class DrawList {
public:
    std::size_t addRect(Rect rect, Color color) {
        commands_.emplace_back(RectCommand{rect, color});
        return commands_.size() - 1U;
    }

    std::size_t addText(ur::text::TextLayout layout, float originX, float baselineY, Color color) {
        commands_.emplace_back(TextCommand{std::move(layout), originX, baselineY, color});
        return commands_.size() - 1U;
    }

    void clear() { commands_.clear(); }

    void setRectColor(std::size_t index, Color color) {
        auto* command = std::get_if<RectCommand>(&commands_.at(index));
        if (command == nullptr) {
            throw std::logic_error("ur_widgets: command is not a rectangle");
        }
        command->color = color;
    }

    [[nodiscard]] const std::vector<DrawCommand>& commands() const { return commands_; }

private:
    std::vector<DrawCommand> commands_;
};

}  // namespace ur::widgets
