# ADR-003: QRhi 私有 API 风险,及跨后端裁剪空间校正的强制要求

- 状态: 已采纳
- 日期: 2026-07-18

## 背景

`ur_gfx` 选择 QRhi 作为后端无关渲染抽象(支持 Vulkan/D3D11/D3D12/Metal)。
实测确认 Qt 6.4.2(当前沙盒环境版本)里 QRhi 仍是私有 API,没有独立的
`Qt6GuiPrivate` CMake 包,需要手动挂 `.../QtGui/<version>/QtGui/private`
头文件搜索路径。Qt 6.6+ 起 QRhi 才会有更正式的公开路径(`<rhi/qrhi.h>`)。

实测还发现:一个不应用 `rhi->clipSpaceCorrMatrix()` 的简单顶点着色器,
在 Vulkan 后端下渲染结果相对于 D3D/GL 的直觉预期是垂直翻转的(Vulkan
NDC 的 Y 轴方向和其他后端不一致)。

## 决策

1. `ur_gfx` 内部使用 QRhi 私有头,通过 `cmake/CompilerWarnings.cmake`
   同级的手动 include 路径处理(见 `libs/ur_gfx/CMakeLists.txt` 里的
   TODO 注释)。这是已知的技术债,Qt 版本升级到 6.6+ 后需要重新评估。
2. 团队规范: 任何在 `ur_gfx` 之上编写的顶点变换代码,必须通过
   `RenderDevice` 提供的裁剪空间校正机制处理坐标系差异,不允许在
   `ur_widgets`/`ur_nodegraph`/`ur_viewport` 里直接假设某个后端的
   NDC 方向。代码审查时需要显式检查这一项。

## 考虑过的替代方案

- 只支持单一后端(比如只用 Vulkan,不支持 D3D): 和"Windows/Linux 从
  第一天就同等优先级"的平台决策冲突(Windows 上 D3D11/D3D12 的驱动
  成熟度普遍优于 Vulkan)。否决。

## 后果

- 需要在 CI 里跑跨后端的渲染回归测试(见 `tools/golden_image_diff/`),
  尽早捕获"某个后端下几何/文本方向错误"这类问题,而不是等到 Windows
  平台正式接入时才发现。
- QRhi 私有 API 的手动 include 路径 hack 是脆弱的,不同 Linux 发行版/
  Qt 打包方式的路径结构可能不同,需要在 CI 矩阵覆盖多个目标环境时
  重新验证。
