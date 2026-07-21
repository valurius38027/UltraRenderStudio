#include "ur/scene_bridge/session.hpp"

#include <gtest/gtest.h>

TEST(RenderSessionTest, CreateAndDestroyDoesNotCrash) {
    auto session = ur::scene_bridge::RenderSession::create();
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(session->isValid());
}
