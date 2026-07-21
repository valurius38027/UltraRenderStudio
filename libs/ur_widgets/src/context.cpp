#include "ur/widgets/context.hpp"

#include <algorithm>
#include <functional>

namespace ur::widgets {
namespace {

constexpr Color kNormalColor{0.3F, 0.3F, 0.35F, 1.0F};
constexpr Color kHoverColor{0.25F, 0.25F, 0.32F, 1.0F};
constexpr Color kActiveColor{0.15F, 0.45F, 0.8F, 1.0F};

}  // namespace

WidgetId hashLabel(std::string_view label) {
    return static_cast<WidgetId>(std::hash<std::string_view>{}(label));
}

void Context::beginFrame(float mouseX, float mouseY, bool mouseDown) {
    mouseX_ = mouseX;
    mouseY_ = mouseY;
    mouseDown_ = mouseDown;
    hoveredId_ = 0;
    submissions_.clear();
    drawList_.clear();
}

const DrawList& Context::endFrame() {
    for (auto it = submissions_.rbegin(); it != submissions_.rend(); ++it) {
        if (it->rect.contains(mouseX_, mouseY_)) {
            hoveredId_ = it->id;
            break;
        }
    }

    const bool activeWasSubmitted = activeId_ != 0 && wasSubmitted(activeId_);
    if (activeId_ != 0 && !activeWasSubmitted) {
        activeId_ = 0;
    }

    if (pressedEdge() && activeId_ == 0) {
        activeId_ = hoveredId_;
    }

    WidgetId resolvedClick = 0;
    if (releasedEdge()) {
        if (activeId_ != 0 && activeId_ == hoveredId_ && wasSubmitted(activeId_)) {
            resolvedClick = activeId_;
        }
        activeId_ = 0;
    }

    clickedId_ = resolvedClick;
    resolveColors();
    prevMouseDown_ = mouseDown_;
    return drawList_;
}

bool Context::button(std::string_view label, Rect rect) {
    const WidgetId id = hashLabel(label);
    const bool clicked = wasClicked(id);
    const std::size_t commandIndex = drawList_.commands().size();
    drawList_.addRect(rect, kNormalColor);
    submissions_.push_back(Submission{id, rect, commandIndex});
    return clicked;
}

void Context::cancelPointerCapture() {
    activeId_ = 0;
}

bool Context::wasSubmitted(WidgetId id) const {
    return std::ranges::any_of(submissions_, [id](const Submission& submission) {
        return submission.id == id;
    });
}

void Context::resolveColors() {
    for (const Submission& submission : submissions_) {
        Color color = kNormalColor;
        if (submission.id == activeId_) {
            color = kActiveColor;
        } else if (submission.id == hoveredId_) {
            color = kHoverColor;
        }
        drawList_.setRectColor(submission.commandIndex, color);
    }
}

}  // namespace ur::widgets
