#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace ur::text {

using FontId = std::uint32_t;
inline constexpr FontId kInvalidFontId = 0U;

struct AtlasConfig {
    std::uint32_t width = 1024U;
    std::uint32_t height = 1024U;
    std::uint32_t padding = 1U;
};

struct FontDescriptor {
    std::filesystem::path filePath;
    std::uint32_t pixelSize = 16U;
};

struct TextBounds {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

struct TextMetrics {
    float advanceWidth = 0.0F;
    float ascender = 0.0F;
    float descender = 0.0F;
    float lineHeight = 0.0F;
    TextBounds inkBounds;
};

struct AtlasRect {
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

struct PositionedGlyph {
    std::uint32_t glyphIndex = 0U;
    float originX = 0.0F;
    float originY = 0.0F;
    float advanceX = 0.0F;
    float advanceY = 0.0F;
    TextBounds bitmapBounds;
    AtlasRect atlasRect;
};

struct TextLayout {
    FontId font = kInvalidFontId;
    std::vector<PositionedGlyph> glyphs;
    TextMetrics metrics;
};

struct AtlasView {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::span<const std::uint8_t> pixels;
    std::uint64_t revision = 0U;
};

class AtlasFullError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class TextSystem {
public:
    explicit TextSystem(AtlasConfig config = {});
    ~TextSystem();

    TextSystem(const TextSystem&) = delete;
    TextSystem& operator=(const TextSystem&) = delete;
    TextSystem(TextSystem&&) noexcept;
    TextSystem& operator=(TextSystem&&) noexcept;

    [[nodiscard]] FontId loadFont(const FontDescriptor& descriptor);
    [[nodiscard]] TextLayout shape(FontId font, std::string_view utf8);
    [[nodiscard]] TextMetrics measure(FontId font, std::string_view utf8);
    [[nodiscard]] TextLayout prepare(FontId font, std::string_view utf8);
    [[nodiscard]] AtlasView atlas() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ur::text
