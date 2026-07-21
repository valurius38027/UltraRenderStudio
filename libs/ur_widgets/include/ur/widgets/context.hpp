#pragma once

#include "ur/text/text.hpp"
#include "ur/widgets/draw_list.hpp"
#include "ur/widgets/geometry.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace ur::widgets {

using WidgetId = std::uint64_t;

[[nodiscard]] WidgetId hashLabel(std::string_view label);

class Context {
public:
    Context(ur::text::TextSystem& textSystem, ur::text::FontId defaultFont);

    void beginFrame(float mouseX, float mouseY, bool mouseDown);
    [[nodiscard]] const DrawList& endFrame();
    bool button(std::string_view label, Rect rect);
    void cancelPointerCapture();

    [[nodiscard]] bool wasClicked(WidgetId id) const { return clickedId_ == id; }
    [[nodiscard]] WidgetId hoveredId() const { return hoveredId_; }
    [[nodiscard]] WidgetId activeId() const { return activeId_; }

private:
    struct Submission {
        WidgetId id = 0U;
        Rect rect;
        std::size_t backgroundCommandIndex = 0U;
    };

    ur::text::TextSystem* textSystem_ = nullptr;
    ur::text::FontId defaultFont_ = ur::text::kInvalidFontId;
    DrawList drawList_;
    std::vector<Submission> submissions_;

    float mouseX_ = 0.0F;
    float mouseY_ = 0.0F;
    bool mouseDown_ = false;
    bool prevMouseDown_ = false;

    WidgetId hoveredId_ = 0U;
    WidgetId activeId_ = 0U;
    WidgetId clickedId_ = 0U;

    [[nodiscard]] bool pressedEdge() const { return mouseDown_ && !prevMouseDown_; }
    [[nodiscard]] bool releasedEdge() const { return !mouseDown_ && prevMouseDown_; }
    [[nodiscard]] bool wasSubmitted(WidgetId id) const;
    void resolveColors();
};

}  // namespace ur::widgets
