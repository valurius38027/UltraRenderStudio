#pragma once

namespace ur::widgets {

struct Rect {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;

    [[nodiscard]] bool contains(float px, float py) const {
        return px >= x && px < x + width && py >= y && py < y + height;
    }
};

struct Color {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 1.0F;
};

}  // namespace ur::widgets
