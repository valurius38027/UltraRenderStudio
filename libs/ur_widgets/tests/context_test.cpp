#include "ur/widgets/context.hpp"

#include <gtest/gtest.h>

using ur::widgets::Context;
using ur::widgets::Rect;
using ur::widgets::hashLabel;

namespace {
constexpr Rect kButtonRect{10.0F, 10.0F, 80.0F, 30.0F};
constexpr Rect kOverlappingRect{20.0F, 20.0F, 80.0F, 30.0F};
}

TEST(ContextButtonTest, CompletedClickIsReportedOnNextSubmissionFrame) {
    Context ctx;

    ctx.beginFrame(50.0F, 20.0F, true);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    (void)ctx.endFrame();
    EXPECT_EQ(ctx.activeId(), hashLabel("Save"));

    ctx.beginFrame(50.0F, 20.0F, false);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    (void)ctx.endFrame();
    EXPECT_EQ(ctx.activeId(), 0U);
    EXPECT_TRUE(ctx.wasClicked(hashLabel("Save")));

    ctx.beginFrame(50.0F, 20.0F, false);
    EXPECT_TRUE(ctx.button("Save", kButtonRect));
    (void)ctx.endFrame();

    ctx.beginFrame(50.0F, 20.0F, false);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    (void)ctx.endFrame();
}

TEST(ContextButtonTest, HoverWithoutPressNeverClicks) {
    Context ctx;
    for (int i = 0; i < 3; ++i) {
        ctx.beginFrame(50.0F, 20.0F, false);
        EXPECT_FALSE(ctx.button("Save", kButtonRect));
        (void)ctx.endFrame();
    }
}

TEST(ContextButtonTest, PressInsideReleaseOutsideCancelsClick) {
    Context ctx;

    ctx.beginFrame(50.0F, 20.0F, true);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    (void)ctx.endFrame();
    ASSERT_EQ(ctx.activeId(), hashLabel("Save"));

    ctx.beginFrame(200.0F, 200.0F, false);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    (void)ctx.endFrame();
    EXPECT_EQ(ctx.activeId(), 0U);
    EXPECT_FALSE(ctx.wasClicked(hashLabel("Save")));
}

TEST(ContextButtonTest, OverlappingWidgetsLastCallWinsHoverAndPressOwnership) {
    Context ctx;

    ctx.beginFrame(50.0F, 20.0F, true);
    EXPECT_FALSE(ctx.button("Bottom", kButtonRect));
    EXPECT_FALSE(ctx.button("Top", kOverlappingRect));
    (void)ctx.endFrame();

    EXPECT_EQ(ctx.hoveredId(), hashLabel("Top"));
    EXPECT_EQ(ctx.activeId(), hashLabel("Top"));
}

TEST(ContextButtonTest, ActiveWidgetDisappearingClearsCapture) {
    Context ctx;

    ctx.beginFrame(50.0F, 20.0F, true);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    (void)ctx.endFrame();
    ASSERT_EQ(ctx.activeId(), hashLabel("Save"));

    ctx.beginFrame(50.0F, 20.0F, false);
    (void)ctx.endFrame();

    EXPECT_EQ(ctx.activeId(), 0U);
    EXPECT_FALSE(ctx.wasClicked(hashLabel("Save")));
}

TEST(ContextButtonTest, CancelPointerCaptureClearsActiveWidget) {
    Context ctx;

    ctx.beginFrame(50.0F, 20.0F, true);
    EXPECT_FALSE(ctx.button("Save", kButtonRect));
    (void)ctx.endFrame();
    ASSERT_NE(ctx.activeId(), 0U);

    ctx.cancelPointerCapture();
    EXPECT_EQ(ctx.activeId(), 0U);
}

TEST(ContextDrawListTest, ButtonAlwaysEmitsExactlyOneRectPerFrame) {
    Context ctx;
    ctx.beginFrame(0.0F, 0.0F, false);
    EXPECT_FALSE(ctx.button("A", kButtonRect));
    EXPECT_FALSE(ctx.button("B", Rect{100.0F, 100.0F, 50.0F, 20.0F}));
    const auto& drawList = ctx.endFrame();
    EXPECT_EQ(drawList.commands().size(), 2U);
}
