#include "ur/widgets/context.hpp"
#include "ur/widgets/render.hpp"

#include <gtest/gtest.h>

#include <variant>

using ur::widgets::Context;
using ur::widgets::Rect;
using ur::widgets::hashLabel;

namespace {

constexpr const char* kFontPath = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
constexpr Rect kButtonRect{10.0F, 10.0F, 80.0F, 30.0F};
constexpr Rect kOverlappingRect{20.0F, 20.0F, 80.0F, 30.0F};

struct TextFixture {
    ur::text::TextSystem text;
    ur::text::FontId font = text.loadFont({kFontPath, 16U});
};

}  // namespace

TEST(ContextButtonTest, CompletedClickIsReportedOnNextSubmissionFrame) {
    TextFixture fixture;
    Context ctx(fixture.text, fixture.font);

    ctx.beginFrame(50.0F, 20.0F, true);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    static_cast<void>(ctx.endFrame());
    EXPECT_EQ(ctx.activeId(), hashLabel("Save"));

    ctx.beginFrame(50.0F, 20.0F, false);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    static_cast<void>(ctx.endFrame());
    EXPECT_EQ(ctx.activeId(), 0U);
    EXPECT_TRUE(ctx.wasClicked(hashLabel("Save")));

    ctx.beginFrame(50.0F, 20.0F, false);
    EXPECT_TRUE(ctx.button("Save", kButtonRect));
    static_cast<void>(ctx.endFrame());

    ctx.beginFrame(50.0F, 20.0F, false);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    static_cast<void>(ctx.endFrame());
}

TEST(ContextButtonTest, HoverWithoutPressNeverClicks) {
    TextFixture fixture;
    Context ctx(fixture.text, fixture.font);
    for (int iteration = 0; iteration < 3; ++iteration) {
        ctx.beginFrame(50.0F, 20.0F, false);
        EXPECT_FALSE(ctx.button("Save", kButtonRect));
        static_cast<void>(ctx.endFrame());
    }
}

TEST(ContextButtonTest, PressInsideReleaseOutsideCancelsClick) {
    TextFixture fixture;
    Context ctx(fixture.text, fixture.font);

    ctx.beginFrame(50.0F, 20.0F, true);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    static_cast<void>(ctx.endFrame());
    ASSERT_EQ(ctx.activeId(), hashLabel("Save"));

    ctx.beginFrame(200.0F, 200.0F, false);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    static_cast<void>(ctx.endFrame());
    EXPECT_EQ(ctx.activeId(), 0U);
    EXPECT_FALSE(ctx.wasClicked(hashLabel("Save")));
}

TEST(ContextButtonTest, OverlappingWidgetsLastCallWinsHoverAndPressOwnership) {
    TextFixture fixture;
    Context ctx(fixture.text, fixture.font);

    ctx.beginFrame(50.0F, 20.0F, true);
    EXPECT_FALSE(ctx.button("Bottom", kButtonRect));
    EXPECT_FALSE(ctx.button("Top", kOverlappingRect));
    static_cast<void>(ctx.endFrame());

    EXPECT_EQ(ctx.hoveredId(), hashLabel("Top"));
    EXPECT_EQ(ctx.activeId(), hashLabel("Top"));
}

TEST(ContextButtonTest, ActiveWidgetDisappearingClearsCapture) {
    TextFixture fixture;
    Context ctx(fixture.text, fixture.font);

    ctx.beginFrame(50.0F, 20.0F, true);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    static_cast<void>(ctx.endFrame());
    ASSERT_EQ(ctx.activeId(), hashLabel("Save"));

    ctx.beginFrame(50.0F, 20.0F, false);
    static_cast<void>(ctx.endFrame());

    EXPECT_EQ(ctx.activeId(), 0U);
    EXPECT_FALSE(ctx.wasClicked(hashLabel("Save")));
}

TEST(ContextButtonTest, CancelPointerCaptureClearsActiveWidget) {
    TextFixture fixture;
    Context ctx(fixture.text, fixture.font);

    ctx.beginFrame(50.0F, 20.0F, true);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    static_cast<void>(ctx.endFrame());
    ASSERT_NE(ctx.activeId(), 0U);

    ctx.cancelPointerCapture();
    EXPECT_EQ(ctx.activeId(), 0U);
}

TEST(ContextDrawListTest, ButtonEmitsOrderedBackgroundAndTextPerFrame) {
    TextFixture fixture;
    Context ctx(fixture.text, fixture.font);
    ctx.beginFrame(0.0F, 0.0F, false);
    EXPECT_FALSE(ctx.button("A", kButtonRect));
    EXPECT_FALSE(ctx.button("B", Rect{100.0F, 100.0F, 50.0F, 20.0F}));
    const auto& drawList = ctx.endFrame();
    ASSERT_EQ(drawList.commands().size(), 4U);
    EXPECT_TRUE(std::holds_alternative<ur::widgets::RectCommand>(drawList.commands()[0]));
    EXPECT_TRUE(std::holds_alternative<ur::widgets::TextCommand>(drawList.commands()[1]));
    EXPECT_TRUE(std::holds_alternative<ur::widgets::RectCommand>(drawList.commands()[2]));
    EXPECT_TRUE(std::holds_alternative<ur::widgets::TextCommand>(drawList.commands()[3]));
}

TEST(ContextDrawListTest, ButtonCentersPreparedLabelUsingLineMetrics) {
    TextFixture fixture;
    Context ctx(fixture.text, fixture.font);
    ctx.beginFrame(0.0F, 0.0F, false);
    static_cast<void>(ctx.button("Save", kButtonRect));
    const auto& drawList = ctx.endFrame();
    ASSERT_EQ(drawList.commands().size(), 2U);
    const auto& text = std::get<ur::widgets::TextCommand>(drawList.commands()[1]);
    EXPECT_NEAR(text.originX,
                kButtonRect.x + (kButtonRect.width - text.layout.metrics.advanceWidth) * 0.5F,
                0.001F);
    EXPECT_NEAR(text.baselineY,
                kButtonRect.y + (kButtonRect.height - text.layout.metrics.lineHeight) * 0.5F +
                    text.layout.metrics.ascender,
                0.001F);
    EXPECT_FALSE(text.layout.glyphs.empty());
    EXPECT_GT(fixture.text.atlas().revision, 0U);
}

TEST(ContextDrawListTest, UiFrameExpansionPreservesRectThenGlyphOrder) {
    TextFixture fixture;
    Context ctx(fixture.text, fixture.font);
    ctx.beginFrame(0.0F, 0.0F, false);
    static_cast<void>(ctx.button("A", kButtonRect));
    const auto& drawList = ctx.endFrame();
    const auto frame = ur::widgets::buildUiFrame(drawList, fixture.text.atlas());
    ASSERT_FALSE(frame.primitives.empty());
    EXPECT_TRUE(std::holds_alternative<ur::gfx::RectPrimitive>(frame.primitives.front()));
    ASSERT_GT(frame.primitives.size(), 1U);
    EXPECT_TRUE(std::holds_alternative<ur::gfx::MaskedQuadPrimitive>(frame.primitives[1]));
    ASSERT_TRUE(frame.alphaAtlas.has_value());
    EXPECT_EQ(frame.alphaAtlas->revision, fixture.text.atlas().revision);
}
