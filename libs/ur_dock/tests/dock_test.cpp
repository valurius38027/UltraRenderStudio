#include "ur/dock/dock.hpp"

#include <gtest/gtest.h>

// TODO(ur_dock): 占位测试,模块有真实实现后替换成有意义的用例。
// 这个测试存在的目的是确保 CMake target 图和 CTest 注册链路是通的。
TEST(DockPlaceholderTest, ModuleLinksAndHeaderIsIncludable) {
    EXPECT_TRUE(ur::dock::kModulePlaceholder);
}
