# ADR-009: Qt Generic Unix Services 的 LSan 进程边界

- 状态: 已采纳
- 日期: 2026-07-21

## 背景

UltraRenderStudio 的 Debian 13 GCC/ASan 门禁中，`ur_gfx_tests` 的五个功能测试
全部通过，但进程退出时 LSan 报告 344 字节：24 字节分配来自
`QGenericUnixServices::QGenericUnixServices()`，其余 320 字节均为 QtCore 管理的
间接分配。调用栈没有 UltraRenderStudio 符号。

本地环境没有稳定复现，因此不能仅凭推断增加 suppression。为确定所有权，在与主 CI
相同的 Debian 13 容器和持久 SDK 中执行四组独立探针：

```text
qgui_only=1
qgui_vulkan=1
ur_gfx_without_suppression=1
ur_gfx_targeted_suppression=0
```

`qgui_only` 只构造并销毁 `QGuiApplication`，仍产生完全相同的 344 字节报告；增加
`QVulkanInstance` 不改变字节数或调用栈。对
`QGenericUnixServices::QGenericUnixServices` 做精确符号匹配后，完整
`ur_gfx_tests` 保持五项功能测试通过且进程返回 0。LSan 报告只显示一条 24 字节根
suppression，其 QtCore 间接子分配随根一同归类。

## 决策

1. 将 `leak:QGenericUnixServices::QGenericUnixServices` 加入 Qt GUI 测试使用的
   suppression 文件。
2. 只给实际创建 `QGuiApplication` 的测试/编辑器进程设置这些文件；纯逻辑测试不设置。
3. 真实 Vulkan 窗口进程继续使用同时包含 Qt services、Mesa worker 和 Vulkan loader
   精确边界的 `lsan-vulkan-loader.supp`。
4. 不使用 `detect_leaks=0`、字节阈值、`leak:Qt`、`leak:QObject` 或其他宽泛规则。
5. 若 Qt 升级后最小 `QGuiApplication` 探针不再泄漏，应删除该规则并重新执行完整门禁。

## 适用目标

- `ur_platform_tests`：Qt GUI suppression；
- `ur_gfx_tests`：Qt GUI + Mesa worker suppression；
- `ur_widgets_render_tests`：Qt GUI + Mesa worker suppression；
- `ur_integration_tests`：Qt GUI + Mesa worker suppression；
- `ur_window_present_tests`：Qt GUI + Mesa worker + Vulkan loader suppression；
- `ur_editor_smoke` 和独立 editor smoke：同真实窗口 suppression。

`ur_widgets_tests`、placeholder 模块测试和其他不创建 `QGuiApplication` 的进程不应用该
规则。

## 后果

该规则排除的是在最小第三方程序中已独立复现的 Qt xcb 平台进程级单例，不会匹配
UltraRenderStudio 自身函数。所有项目资源泄漏仍由 ASan/LSan 正常报告。诊断证据保留
在仓库 Automation status ledger 的 CI diagnostic issue 中，阶段完成后该临时 Issue
可以关闭，但 ADR 必须保留。
