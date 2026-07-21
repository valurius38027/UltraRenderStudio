#pragma once

// TODO(ur_nodegraph): 占位接口,尚未实现。参考 docs/architecture/ 下对应的 ADR
// 补充这个模块的真实公开接口设计后再填充。

namespace ur::nodegraph {

/// 占位符号,确保这个头文件被 include 时至少有一个可见的声明,
/// 避免"空头文件"在某些编译器/工具链下被当成异常情况处理。
constexpr bool kModulePlaceholder = true;

}  // namespace ur::nodegraph
