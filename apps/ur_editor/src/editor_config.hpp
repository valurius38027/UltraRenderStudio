#pragma once

#include <string_view>

namespace ur::editor::config {

inline constexpr std::string_view kDefaultFontPath =
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
inline constexpr unsigned int kDefaultFontPixelSize = 16U;

}  // namespace ur::editor::config
