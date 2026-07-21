#include "ur/widgets/context.hpp"

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace ur::widgets {
namespace {

constexpr Color kNormalColor{0.3F, 0.3F, 0.35F, 1.0F};
constexpr Color kHoverColor{0.25F, 0.25F, 0.32F, 1.0F};
constexpr Color kActiveColor{0.15F, 0.45F, 0.8F, 1.0F};
constexpr Color kLabelColor{0.92F, 0.92F, 0.95F, 1.0F};

}  // namespace

WidgetId hashLabel(std::string_view label) {
    return static_cast<WidgetId>(std::hash<std::string_view>{}(label));
}

Context::Context(ur::text::TextSystem& textSystem, ur::text::FontId defaultFont)
    : textSystem_(&textSystem), defaultFont_(defaultFont) {
    if (defaultFont_ == ur::text::kInvalidFontId) {
        throw std::invalid_argument("ur_widgets: default font must be valid");
    }
    static_cast<void>(textSystem_->measure(defaultFont_, ""));
}

void Context::beginFrame(float mouseX, float mouseY, bool mouseDown) {
    mouseX_ = mouseX;
    mouseY_ = mouseY;
    mouseDown_ = mouseDown;
    hoveredId_ = 0U;
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

    const bool activeWasSubmitted = activeId_ != 0U && wasSubmitted(activeId_);
    if (activeId_ != 0U && !activeWasSubmitted) {
        activeId_ = 0U;
    }

    if (pressedEdge() && activeId_ == 0U) {
        activeId_ = hoveredId_;
    }

    WidgetId resolvedClick = 0U;
    if (releasedEdge()) {
        if (activeId_ != 0U && activeId_ == hoveredId_ && wasSubmitted(activeId_)) {
            resolvedClick = activeId_;
        }
        activeId_ = 0U;
    }

    clickedId_ = resolvedClick;
    resolveColors();
    prevMouseDown_ = mouseDown_;
    return drawList_;
}

bool Context::button(std::string_view label, Rect rect) {
    const WidgetId id = hashLabel(label);
    const bool clicked = wasClicked(id);
    const std::size_t backgroundIndex = drawList_.addRect(rect, kNormalColor);

    ur::text::TextLayout layout = textSystem_->prepare(defaultFont_, label);
    const float originX = rect.x + (rect.width - layout.metrics.advanceWidth) * 0.5F;
    const float lineTop = rect.y + (rect.height - layout.metrics.lineHeight) * 0.5F;
    const float baselineY = lineTop + layout.metrics.ascender;
    static_cast<void>(drawList_.addText(std::move(layout), originX, baselineY, kLabelColor));

    submissions_.push_back(Submission{id, rect, backgroundIndex});
    return clicked;
}

void Context::cancelPointerCapture() {
    activeId_ = 0U;
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
        drawList_.setRectColor(submission.backgroundCommandIndex, color);
    }
}

}  // namespace ur::widgets
