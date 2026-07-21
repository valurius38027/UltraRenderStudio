#include "ur/text/text.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace {
constexpr const char* kFont = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

ur::text::FontId loadDefault(ur::text::TextSystem& system, std::uint32_t size = 16U) {
    return system.loadFont({kFont, size});
}

template <typename Callable>
void invokeForException(Callable&& callable) {
    static_cast<void>(callable());
}

std::size_t nonZeroPixels(const ur::text::AtlasView& atlas) {
    return static_cast<std::size_t>(std::count_if(atlas.pixels.begin(), atlas.pixels.end(),
                                                  [](std::uint8_t value) { return value != 0U; }));
}
}  // namespace

TEST(TextSystemTest, RejectsInvalidAtlasAndFontInputs) {
    EXPECT_THROW(ur::text::TextSystem({0U, 64U, 1U}), std::invalid_argument);
    ur::text::TextSystem system;
    EXPECT_THROW(invokeForException([&] { return system.loadFont({kFont, 0U}); }),
                 std::invalid_argument);
    EXPECT_THROW(invokeForException([&] {
                     return system.loadFont({"/definitely/missing/font.ttf", 16U});
                 }),
                 std::runtime_error);
    EXPECT_THROW(invokeForException([&] {
                     return system.shape(ur::text::kInvalidFontId, "A");
                 }),
                 std::invalid_argument);
}

TEST(TextSystemTest, RejectsMalformedUtf8AndLineSeparators) {
    ur::text::TextSystem system;
    const auto font = loadDefault(system);
    const std::string malformed{"\xF0\x28\x8C\x28", 4};
    EXPECT_THROW(invokeForException([&] { return system.shape(font, malformed); }),
                 std::invalid_argument);
    EXPECT_THROW(invokeForException([&] { return system.shape(font, "a\nb"); }),
                 std::invalid_argument);
    EXPECT_THROW(invokeForException([&] { return system.shape(font, "a\r\nb"); }),
                 std::invalid_argument);
    EXPECT_THROW(invokeForException([&] { return system.shape(font, "a\xE2\x80\xA8"); }),
                 std::invalid_argument);
}

TEST(TextSystemTest, EmptyTextKeepsLineMetricsWithoutAtlasMutation) {
    ur::text::TextSystem system;
    const auto font = loadDefault(system);
    const auto before = system.atlas().revision;
    const auto layout = system.shape(font, "");
    EXPECT_TRUE(layout.glyphs.empty());
    EXPECT_FLOAT_EQ(layout.metrics.advanceWidth, 0.0F);
    EXPECT_GT(layout.metrics.lineHeight, 0.0F);
    EXPECT_GT(layout.metrics.ascender, 0.0F);
    EXPECT_LT(layout.metrics.descender, 0.0F);
    EXPECT_EQ(system.atlas().revision, before);
}

TEST(TextSystemTest, ShapesAsciiUtf8CombiningAndRtlRuns) {
    ur::text::TextSystem system;
    const auto font = loadDefault(system);
    const auto ascii = system.shape(font, "UltraRender");
    const auto utf8 = system.shape(font, "R\xC3\xA9sum\xC3\xA9");
    const auto combining = system.shape(font, "e\xCC\x81");
    const auto rtl = system.shape(font, "\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D");
    EXPECT_FALSE(ascii.glyphs.empty());
    EXPECT_FALSE(utf8.glyphs.empty());
    EXPECT_FALSE(combining.glyphs.empty());
    EXPECT_FALSE(rtl.glyphs.empty());
    EXPECT_GT(ascii.metrics.advanceWidth, 0.0F);
    EXPECT_GT(utf8.metrics.advanceWidth, 0.0F);
    EXPECT_GT(rtl.metrics.advanceWidth, 0.0F);
    EXPECT_LE(combining.glyphs.size(), 2U);
}

TEST(TextSystemTest, MeasureMatchesShapeAndDoesNotGrowAtlas) {
    ur::text::TextSystem system;
    const auto font = loadDefault(system);
    const auto before = system.atlas().revision;
    const auto shaped = system.shape(font, "AV office");
    const auto measured = system.measure(font, "AV office");
    EXPECT_NEAR(shaped.metrics.advanceWidth, measured.advanceWidth, 0.001F);
    EXPECT_EQ(system.atlas().revision, before);
    EXPECT_TRUE(std::all_of(shaped.glyphs.begin(), shaped.glyphs.end(), [](const auto& glyph) {
        return glyph.atlasRect.width == 0U && glyph.atlasRect.height == 0U;
    }));
}

TEST(TextSystemTest, MissingCharacterUsesNotdefGlyph) {
    ur::text::TextSystem system;
    const auto font = loadDefault(system);
    const auto layout = system.shape(font, "\xF4\x8F\xBF\xBF");
    ASSERT_EQ(layout.glyphs.size(), 1U);
    EXPECT_EQ(layout.glyphs.front().glyphIndex, 0U);
}

TEST(TextSystemTest, PrepareRasterizesCachesAndAdvancesRevisionPerGlyph) {
    ur::text::TextSystem system;
    const auto font = loadDefault(system);
    const auto first = system.prepare(font, "ABA");
    const auto atlasAfterFirst = system.atlas();
    ASSERT_EQ(first.glyphs.size(), 3U);
    EXPECT_EQ(atlasAfterFirst.revision, 2U);
    EXPECT_GT(nonZeroPixels(atlasAfterFirst), 0U);
    for (const auto& glyph : first.glyphs) {
        if (glyph.atlasRect.width == 0U || glyph.atlasRect.height == 0U) {
            continue;
        }
        EXPECT_FLOAT_EQ(glyph.bitmapBounds.width,
                        static_cast<float>(glyph.atlasRect.width));
        EXPECT_FLOAT_EQ(glyph.bitmapBounds.height,
                        static_cast<float>(glyph.atlasRect.height));
        EXPECT_FLOAT_EQ(glyph.bitmapBounds.x - glyph.originX,
                        std::trunc(glyph.bitmapBounds.x - glyph.originX));
        EXPECT_FLOAT_EQ(glyph.bitmapBounds.y - glyph.originY,
                        std::trunc(glyph.bitmapBounds.y - glyph.originY));
    }
    EXPECT_EQ(first.glyphs[0].atlasRect.x, first.glyphs[2].atlasRect.x);
    EXPECT_EQ(first.glyphs[0].atlasRect.y, first.glyphs[2].atlasRect.y);

    const auto second = system.prepare(font, "BA");
    EXPECT_EQ(system.atlas().revision, atlasAfterFirst.revision);
    EXPECT_EQ(second.glyphs[1].atlasRect.x, first.glyphs[0].atlasRect.x);
}

TEST(TextSystemTest, SpaceIsMeasurableWithoutAtlasAllocation) {
    ur::text::TextSystem system;
    const auto font = loadDefault(system);
    const auto layout = system.prepare(font, " ");
    ASSERT_EQ(layout.glyphs.size(), 1U);
    EXPECT_GT(layout.metrics.advanceWidth, 0.0F);
    EXPECT_EQ(layout.glyphs.front().atlasRect.width, 0U);
    EXPECT_EQ(system.atlas().revision, 0U);
}

TEST(TextSystemTest, ShelfPlacementIsDeterministic) {
    ur::text::TextSystem first({128U, 128U, 1U});
    ur::text::TextSystem second({128U, 128U, 1U});
    const auto firstFont = loadDefault(first);
    const auto secondFont = loadDefault(second);
    const auto firstLayout = first.prepare(firstFont, "ABCxyz");
    const auto secondLayout = second.prepare(secondFont, "ABCxyz");
    ASSERT_EQ(firstLayout.glyphs.size(), secondLayout.glyphs.size());
    for (std::size_t index = 0U; index < firstLayout.glyphs.size(); ++index) {
        EXPECT_EQ(firstLayout.glyphs[index].atlasRect.x, secondLayout.glyphs[index].atlasRect.x);
        EXPECT_EQ(firstLayout.glyphs[index].atlasRect.y, secondLayout.glyphs[index].atlasRect.y);
        EXPECT_EQ(firstLayout.glyphs[index].atlasRect.width,
                  secondLayout.glyphs[index].atlasRect.width);
        EXPECT_EQ(firstLayout.glyphs[index].atlasRect.height,
                  secondLayout.glyphs[index].atlasRect.height);
    }
    EXPECT_EQ(first.atlas().pixels.size(), second.atlas().pixels.size());
    EXPECT_TRUE(std::equal(first.atlas().pixels.begin(), first.atlas().pixels.end(),
                           second.atlas().pixels.begin()));
}

TEST(TextSystemTest, SmallAtlasReportsExhaustionWithoutMovingExistingGlyphs) {
    ur::text::TextSystem system({20U, 20U, 1U});
    const auto font = loadDefault(system, 16U);
    const auto first = system.prepare(font, "A");
    ASSERT_FALSE(first.glyphs.empty());
    const auto original = first.glyphs.front().atlasRect;
    EXPECT_THROW(invokeForException([&] { return system.prepare(font, "WXYZ"); }),
                 ur::text::AtlasFullError);
    const auto repeated = system.prepare(font, "A");
    EXPECT_EQ(repeated.glyphs.front().atlasRect.x, original.x);
    EXPECT_EQ(repeated.glyphs.front().atlasRect.y, original.y);
}
