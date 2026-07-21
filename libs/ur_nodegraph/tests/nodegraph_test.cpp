#include "ur/nodegraph/nodegraph.hpp"

#include <gtest/gtest.h>

// TODO(ur_nodegraph): 占位测试,模块有真实实现后替换成有意义的用例。
// 这个测试存在的目的是确保 CMake target 图和 CTest 注册链路是通的。
TEST(NodegraphPlaceholderTest, ModuleLinksAndHeaderIsIncludable) {
    EXPECT_TRUE(ur::nodegraph::kModulePlaceholder);
}
