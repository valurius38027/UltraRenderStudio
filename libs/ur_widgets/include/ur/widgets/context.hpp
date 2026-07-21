#pragma once

#include "ur/widgets/draw_list.hpp"
#include "ur/widgets/geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ur::widgets {

using WidgetId = std::uint64_t;

/// Hashes a label into the current flat widget ID namespace. Scoped IDs are a
/// later concern; duplicate labels in one frame remain unsupported.
[[nodiscard]] WidgetId hashLabel(std::string_view label);

/// Immediate-mode widget context with two-stage input resolution. Widgets are
/// submitted in draw order during the frame; endFrame() resolves the topmost
/// hover, press ownership, release/click state, and final command colors.
class Context {
public:
    void beginFrame(float mouseX, float mouseY, bool mouseDown);
    [[nodiscard]] const DrawList& endFrame();

    /// Submits a rectangle button and reports a click resolved by the previous
    /// endFrame(). This one-frame handoff is required so topmost ownership can
    /// be decided after every overlapping widget has been submitted.
    bool button(std::string_view label, Rect rect);

    /// Cancels active pointer ownership, for example after focus or capture
    /// loss from the platform layer.
    void cancelPointerCapture();

    [[nodiscard]] bool wasClicked(WidgetId id) const { return clickedId_ == id; }
    [[nodiscard]] WidgetId hoveredId() const { return hoveredId_; }
    [[nodiscard]] WidgetId activeId() const { return activeId_; }

private:
    struct Submission {
        WidgetId id = 0;
        Rect rect;
        std::size_t commandIndex = 0U;
    };

    DrawList drawList_;
    std::vector<Submission> submissions_;

    float mouseX_ = 0.0F;
    float mouseY_ = 0.0F;
    bool mouseDown_ = false;
    bool prevMouseDown_ = false;

    WidgetId hoveredId_ = 0;
    WidgetId activeId_ = 0;
    WidgetId clickedId_ = 0;

    [[nodiscard]] bool pressedEdge() const { return mouseDown_ && !prevMouseDown_; }
    [[nodiscard]] bool releasedEdge() const { return !mouseDown_ && prevMouseDown_; }
    [[nodiscard]] bool wasSubmitted(WidgetId id) const;
    void resolveColors();
};

}  // namespace ur::widgets
