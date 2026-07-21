# ADR-007: 模块完成度与开发排期规则

- 状态: 已采纳（2026-07-21 修订）
- 初始日期: 2026-07-18
- 最新修订: 2026-07-21

## 完成度规则

模块或阶段只有同时满足以下条件才能标记完成：

1. 目标职责不存在 placeholder/TODO 级缺口；
2. 行为由能失败的单元或集成测试覆盖；
3. 至少一个真实下游消费者产生可观察效果；
4. GCC/Clang 严格构建和适用 sanitizer 通过；
5. 窗口/GPU 路径在 Xvfb/Lavapipe 或真实支持后端运行；
6. README、INDEX、ADR、spec 和 plan 与代码现实一致；
7. 阶段 tag、完整 bundle、fresh clone、`git fsck` 和远端 checksum round-trip 完成。

## 已完成阶段

### Production Bootstrap

闭合平台事件、私有 QWindow 边界、Vulkan QRhi 离屏/窗口 present、resize、顶层命中仲裁、
stale capture 清理和真实 editor frame loop。

### Minimal Text Rendering Closure

闭合显式字体加载、UTF-8 HarfBuzz shaping、FreeType grayscale raster、测量、确定性 R8
atlas、通用 ordered UiFrame、masked-quad QRhi pipeline、按钮 label、像素回读和真实窗口呈现。
文字所有权和边界见 ADR-010。

## 机械可推导的后续顺序

### 1. Dock 前置 UI Foundation（下一里程碑）

`ur_widgets` 必须补齐：

- scoped ID；
- clipping/scissor 与嵌套交集；
- overlay 层级；
- 消费文字尺寸的基础布局；
- 指针 capture/focus-loss 的应用级回归；
- 一个真实 editor panel 消费上述能力。

### 2. `ur_dock`

实现 split tree、tab strip、拖拽预览、最小尺寸、持久化和恢复，并由真实编辑器多面板操作
验证。禁止 placeholder 完成声明。

### 3. `ur_nodegraph` 与 `ur_viewport`

Dock 闭合后根据实际风险重排；二者都必须有真实下游效果。

### 4. `ur_scene_bridge`

集成阶段必须记录 C ABI 上游版本、能力协商、错误传播和 ABI 兼容门禁，并链接真实
UltraRender 库验证。

## 变更规则

只有新的可验证架构风险、依赖关系或实验结果才能改变顺序。效率优化可以压缩流程，不能
替代工程证据。
