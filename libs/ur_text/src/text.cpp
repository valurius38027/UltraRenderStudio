#include "ur/text/text.hpp"

#include <hb-ft.h>
#include <hb.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

namespace ur::text {
namespace {

constexpr FT_Int32 kLoadFlags = FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_BITMAP;
constexpr float kFixedScale = 1.0F / 64.0F;

[[nodiscard]] float fixedToFloat(FT_Pos value) {
    return static_cast<float>(value) * kFixedScale;
}

[[nodiscard]] std::size_t checkedArea(std::uint32_t width, std::uint32_t height) {
    if (height != 0U && static_cast<std::size_t>(width) >
                            std::numeric_limits<std::size_t>::max() /
                                static_cast<std::size_t>(height)) {
        throw std::invalid_argument("ur_text: atlas dimensions overflow addressable memory");
    }
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

void validateUtf8AndSingleLine(std::string_view text) {
    const auto fail = [] { throw std::invalid_argument("ur_text: malformed UTF-8"); };
    std::size_t index = 0U;
    while (index < text.size()) {
        const auto lead = static_cast<std::uint8_t>(text[index]);
        std::uint32_t codepoint = 0U;
        std::size_t length = 0U;
        if (lead <= 0x7FU) {
            codepoint = lead;
            length = 1U;
        } else if (lead >= 0xC2U && lead <= 0xDFU) {
            codepoint = lead & 0x1FU;
            length = 2U;
        } else if (lead >= 0xE0U && lead <= 0xEFU) {
            codepoint = lead & 0x0FU;
            length = 3U;
        } else if (lead >= 0xF0U && lead <= 0xF4U) {
            codepoint = lead & 0x07U;
            length = 4U;
        } else {
            fail();
        }
        if (index + length > text.size()) {
            fail();
        }
        for (std::size_t offset = 1U; offset < length; ++offset) {
            const auto byte = static_cast<std::uint8_t>(text[index + offset]);
            if ((byte & 0xC0U) != 0x80U) {
                fail();
            }
            codepoint = (codepoint << 6U) | (byte & 0x3FU);
        }
        if ((length == 3U && codepoint < 0x800U) ||
            (length == 4U && codepoint < 0x10000U) ||
            (codepoint >= 0xD800U && codepoint <= 0xDFFFU) || codepoint > 0x10FFFFU) {
            fail();
        }
        if (codepoint == 0x0AU || codepoint == 0x0DU || codepoint == 0x85U ||
            codepoint == 0x2028U || codepoint == 0x2029U) {
            throw std::invalid_argument("ur_text: v1 accepts single-line text only");
        }
        index += length;
    }
}

[[nodiscard]] TextBounds unionBounds(const std::optional<TextBounds>& existing,
                                     const TextBounds& candidate) {
    if (!existing.has_value()) {
        return candidate;
    }
    const float left = std::min(existing->x, candidate.x);
    const float top = std::min(existing->y, candidate.y);
    const float right = std::max(existing->x + existing->width,
                                 candidate.x + candidate.width);
    const float bottom = std::max(existing->y + existing->height,
                                  candidate.y + candidate.height);
    return {left, top, right - left, bottom - top};
}

struct GlyphKey {
    FontId font = kInvalidFontId;
    std::uint32_t glyphIndex = 0U;

    bool operator==(const GlyphKey&) const = default;
};

struct GlyphKeyHash {
    std::size_t operator()(const GlyphKey& key) const noexcept {
        const std::uint64_t packed = (static_cast<std::uint64_t>(key.font) << 32U) |
                                     static_cast<std::uint64_t>(key.glyphIndex);
        return std::hash<std::uint64_t>{}(packed);
    }
};

}  // namespace

struct TextSystem::Impl {
    struct Font {
        FT_Face face = nullptr;
        hb_font_t* harfbuzz = nullptr;
        std::filesystem::path path;
        std::uint32_t pixelSize = 0U;

        ~Font() {
            if (harfbuzz != nullptr) {
                hb_font_destroy(harfbuzz);
            }
            if (face != nullptr) {
                FT_Done_Face(face);
            }
        }
    };

    struct CachedGlyph {
        AtlasRect rect;
        TextBounds rasterBounds;
    };

    explicit Impl(AtlasConfig atlasConfig) : config(atlasConfig) {
        if (config.width == 0U || config.height == 0U) {
            throw std::invalid_argument("ur_text: atlas dimensions must be non-zero");
        }
        atlasPixels.resize(checkedArea(config.width, config.height), 0U);
        if (FT_Init_FreeType(&freetype) != 0 || freetype == nullptr) {
            throw std::runtime_error("ur_text: FreeType initialization failed");
        }
    }

    ~Impl() {
        fonts.clear();
        if (freetype != nullptr) {
            FT_Done_FreeType(freetype);
        }
    }

    [[nodiscard]] Font& font(FontId id) {
        if (id == kInvalidFontId || id > fonts.size()) {
            throw std::invalid_argument("ur_text: invalid FontId");
        }
        return *fonts.at(static_cast<std::size_t>(id - 1U));
    }

    [[nodiscard]] TextLayout shape(FontId id, std::string_view utf8) {
        validateUtf8AndSingleLine(utf8);
        Font& selected = font(id);

        TextLayout layout;
        layout.font = id;
        layout.metrics.ascender = fixedToFloat(selected.face->size->metrics.ascender);
        layout.metrics.descender = fixedToFloat(selected.face->size->metrics.descender);
        layout.metrics.lineHeight = fixedToFloat(selected.face->size->metrics.height);
        if (utf8.empty()) {
            return layout;
        }

        if (utf8.size() > static_cast<std::size_t>(INT_MAX)) {
            throw std::invalid_argument("ur_text: UTF-8 run exceeds HarfBuzz input limits");
        }

        using BufferPtr = std::unique_ptr<hb_buffer_t, decltype(&hb_buffer_destroy)>;
        BufferPtr buffer(hb_buffer_create(), &hb_buffer_destroy);
        if (buffer == nullptr || !hb_buffer_allocation_successful(buffer.get())) {
            throw std::runtime_error("ur_text: HarfBuzz buffer allocation failed");
        }
        hb_buffer_add_utf8(buffer.get(), utf8.data(), static_cast<int>(utf8.size()), 0,
                           static_cast<int>(utf8.size()));
        hb_buffer_guess_segment_properties(buffer.get());
        hb_shape(selected.harfbuzz, buffer.get(), nullptr, 0U);

        unsigned int infoCount = 0U;
        unsigned int positionCount = 0U;
        const hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer.get(), &infoCount);
        const hb_glyph_position_t* positions =
            hb_buffer_get_glyph_positions(buffer.get(), &positionCount);
        if (infoCount != positionCount ||
            (infoCount > 0U && (infos == nullptr || positions == nullptr))) {
            throw std::runtime_error("ur_text: HarfBuzz returned incomplete glyph data");
        }
        const unsigned int glyphCount = infoCount;

        layout.glyphs.reserve(glyphCount);
        float penX = 0.0F;
        float penY = 0.0F;
        std::optional<TextBounds> ink;
        for (unsigned int index = 0U; index < glyphCount; ++index) {
            const std::uint32_t glyphIndex = infos[index].codepoint;
            if (FT_Load_Glyph(selected.face, glyphIndex, kLoadFlags) != 0) {
                throw std::runtime_error("ur_text: FreeType failed to load shaped glyph");
            }

            const float originX = penX + fixedToFloat(positions[index].x_offset);
            const float originY = penY - fixedToFloat(positions[index].y_offset);
            const FT_Glyph_Metrics& metrics = selected.face->glyph->metrics;
            TextBounds bounds{
                originX + fixedToFloat(metrics.horiBearingX),
                originY - fixedToFloat(metrics.horiBearingY),
                fixedToFloat(metrics.width),
                fixedToFloat(metrics.height),
            };
            if (bounds.width > 0.0F && bounds.height > 0.0F) {
                ink = unionBounds(ink, bounds);
            }

            const float advanceX = fixedToFloat(positions[index].x_advance);
            const float advanceY = -fixedToFloat(positions[index].y_advance);
            layout.glyphs.push_back(PositionedGlyph{
                glyphIndex, originX, originY, advanceX, advanceY, bounds, {},
            });
            penX += advanceX;
            penY += advanceY;
        }
        layout.metrics.advanceWidth = penX;
        if (ink.has_value()) {
            layout.metrics.inkBounds = *ink;
        }
        return layout;
    }

    [[nodiscard]] CachedGlyph cacheGlyph(FontId id, std::uint32_t glyphIndex) {
        const GlyphKey key{id, glyphIndex};
        if (const auto found = glyphCache.find(key); found != glyphCache.end()) {
            return found->second;
        }

        Font& selected = font(id);
        if (FT_Load_Glyph(selected.face, glyphIndex, kLoadFlags) != 0 ||
            FT_Render_Glyph(selected.face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
            throw std::runtime_error("ur_text: FreeType glyph rasterization failed");
        }
        const FT_Bitmap& bitmap = selected.face->glyph->bitmap;
        if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY && bitmap.width != 0U && bitmap.rows != 0U) {
            throw std::runtime_error("ur_text: FreeType did not return grayscale coverage");
        }
        const TextBounds rasterBounds{
            static_cast<float>(selected.face->glyph->bitmap_left),
            -static_cast<float>(selected.face->glyph->bitmap_top),
            static_cast<float>(bitmap.width),
            static_cast<float>(bitmap.rows),
        };
        if (bitmap.width == 0U || bitmap.rows == 0U) {
            const CachedGlyph cached{{}, rasterBounds};
            glyphCache.emplace(key, cached);
            return cached;
        }
        if (bitmap.buffer == nullptr) {
            throw std::runtime_error("ur_text: FreeType returned a null bitmap buffer");
        }

        const std::uint32_t width = bitmap.width;
        const std::uint32_t height = bitmap.rows;
        const std::uint64_t paddedWidth64 = static_cast<std::uint64_t>(width) + 2ULL * config.padding;
        const std::uint64_t paddedHeight64 = static_cast<std::uint64_t>(height) + 2ULL * config.padding;
        if (paddedWidth64 > config.width || paddedHeight64 > config.height) {
            throw AtlasFullError("ur_text: glyph is larger than the atlas");
        }
        std::uint64_t nextX = shelfX;
        std::uint64_t nextY = shelfY;
        std::uint64_t nextShelfHeight = shelfHeight;
        if (nextX + paddedWidth64 > config.width) {
            nextX = 0U;
            nextY += nextShelfHeight;
            nextShelfHeight = 0U;
        }
        if (nextY + paddedHeight64 > config.height) {
            throw AtlasFullError("ur_text: glyph atlas is full");
        }

        const AtlasRect rect{
            static_cast<std::uint32_t>(nextX) + config.padding,
            static_cast<std::uint32_t>(nextY) + config.padding,
            width,
            height,
        };
        std::vector<std::uint8_t> staged(checkedArea(width, height));
        const int pitch = bitmap.pitch;
        if (pitch == std::numeric_limits<int>::min()) {
            throw std::runtime_error("ur_text: invalid FreeType bitmap pitch");
        }
        const int absolutePitch = std::abs(pitch);
        if (static_cast<std::uint32_t>(absolutePitch) < width) {
            throw std::runtime_error("ur_text: FreeType bitmap pitch is smaller than its width");
        }
        for (std::uint32_t row = 0U; row < height; ++row) {
            const std::uint32_t sourceRow = pitch >= 0 ? row : (height - 1U - row);
            const auto* source = bitmap.buffer + static_cast<std::size_t>(sourceRow) *
                                                   static_cast<std::size_t>(absolutePitch);
            std::memcpy(staged.data() + static_cast<std::size_t>(row) * width, source, width);
        }

        for (std::uint32_t row = 0U; row < height; ++row) {
            auto* destination = atlasPixels.data() +
                static_cast<std::size_t>(rect.y + row) * config.width + rect.x;
            std::memcpy(destination, staged.data() + static_cast<std::size_t>(row) * width, width);
        }
        shelfX = static_cast<std::uint32_t>(nextX + paddedWidth64);
        shelfY = static_cast<std::uint32_t>(nextY);
        shelfHeight = static_cast<std::uint32_t>(
            std::max(nextShelfHeight, paddedHeight64));
        const CachedGlyph cached{rect, rasterBounds};
        glyphCache.emplace(key, cached);
        ++revision;
        return cached;
    }

    AtlasConfig config;
    FT_Library freetype = nullptr;
    std::vector<std::unique_ptr<Font>> fonts;
    std::unordered_map<GlyphKey, CachedGlyph, GlyphKeyHash> glyphCache;
    std::vector<std::uint8_t> atlasPixels;
    std::uint32_t shelfX = 0U;
    std::uint32_t shelfY = 0U;
    std::uint32_t shelfHeight = 0U;
    std::uint64_t revision = 0U;
};

TextSystem::TextSystem(AtlasConfig config) : impl_(std::make_unique<Impl>(config)) {}
TextSystem::~TextSystem() = default;
TextSystem::TextSystem(TextSystem&&) noexcept = default;
TextSystem& TextSystem::operator=(TextSystem&&) noexcept = default;

FontId TextSystem::loadFont(const FontDescriptor& descriptor) {
    if (descriptor.pixelSize == 0U) {
        throw std::invalid_argument("ur_text: font pixel size must be non-zero");
    }
    auto loaded = std::make_unique<Impl::Font>();
    loaded->path = descriptor.filePath;
    loaded->pixelSize = descriptor.pixelSize;
    const std::string path = descriptor.filePath.string();
    if (FT_New_Face(impl_->freetype, path.c_str(), 0, &loaded->face) != 0 || loaded->face == nullptr) {
        throw std::runtime_error("ur_text: failed to load font: " + path);
    }
    if (FT_Set_Pixel_Sizes(loaded->face, 0U, descriptor.pixelSize) != 0) {
        throw std::runtime_error("ur_text: failed to set font pixel size: " + path);
    }
    loaded->harfbuzz = hb_ft_font_create_referenced(loaded->face);
    if (loaded->harfbuzz == nullptr) {
        throw std::runtime_error("ur_text: failed to create HarfBuzz font: " + path);
    }
    hb_ft_font_set_load_flags(loaded->harfbuzz, kLoadFlags);
    if (impl_->fonts.size() >= static_cast<std::size_t>(std::numeric_limits<FontId>::max())) {
        throw std::overflow_error("ur_text: FontId space exhausted");
    }
    impl_->fonts.push_back(std::move(loaded));
    return static_cast<FontId>(impl_->fonts.size());
}

TextLayout TextSystem::shape(FontId font, std::string_view utf8) {
    return impl_->shape(font, utf8);
}

TextMetrics TextSystem::measure(FontId font, std::string_view utf8) {
    return impl_->shape(font, utf8).metrics;
}

TextLayout TextSystem::prepare(FontId font, std::string_view utf8) {
    TextLayout layout = impl_->shape(font, utf8);
    for (PositionedGlyph& glyph : layout.glyphs) {
        const Impl::CachedGlyph cached = impl_->cacheGlyph(font, glyph.glyphIndex);
        glyph.atlasRect = cached.rect;
        glyph.bitmapBounds = {
            glyph.originX + cached.rasterBounds.x,
            glyph.originY + cached.rasterBounds.y,
            cached.rasterBounds.width,
            cached.rasterBounds.height,
        };
    }
    return layout;
}

AtlasView TextSystem::atlas() const {
    return {impl_->config.width, impl_->config.height, impl_->atlasPixels, impl_->revision};
}

}  // namespace ur::text
