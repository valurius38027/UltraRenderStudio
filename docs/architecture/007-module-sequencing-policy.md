# ADR-007: 模块完成度与开发排期规则

- 状态: 已采纳（2026-07-20 修订）
- 初始日期: 2026-07-18
- 修订日期: 2026-07-20

## 背景

排期不能由单次对话或“哪个模块看起来有趣”决定。旧版本还把
“DrawList 尚未接入 ur_gfx”列为当前事实，并据此把 Dock 排到下一位；该陈述已被
真实像素测试和窗口 present 链推翻。继续沿用旧排期会让 Dock 建立在缺少文字测量、
作用域 ID 和 clipping 的底座上。

## 完成度规则

一个模块或阶段只有同时满足以下条件才能标记完成：

1. 核心职责不存在 placeholder/TODO 级缺口；
2. 行为由能失败的单元或集成测试覆盖，而不是只测试“能编译”；
3. 至少一个真实下游消费者产生可观察效果；
4. GCC 和 Clang 严格警告构建通过；
5. 涉及窗口或 GPU 的路径在 Xvfb/Lavapipe 或真实平台后端运行；
6. ADR/README 与代码现实一致；
7. 阶段提交、tag 和经过恢复验证的完整 git bundle 已生成。

## 已完成阶段：Production Bootstrap

截至 2026-07-20，以下底座已经闭合：

- Qt 私有 QRhi 头不再污染项目 `-Werror`；
- `ur_platform` 提供 resize、expose、pointer、focus 和 close 的 FIFO 事件边界；
- `ur_gfx` 分离离屏与窗口工厂，并有真实 Vulkan swapchain/present/resize；
- `ur_widgets` 使用 topmost 两阶段命中与 stale capture 清理；
- `ur_editor` 真实消费平台、图形和部件模块，能自动呈现有限帧并退出；
- 离屏像素回读路径保持可用。

这只说明最小运行底座完成，不说明 `ur_widgets`、编辑器或产品整体完成。

## 机械可推导的后续顺序

### 1. 最小文字闭环（下一里程碑）

实现 `ur_text` 的字体加载、UTF-8 shaping、字形 atlas、测量和 DrawText 输出，并让
`ur_widgets` 的按钮标签在真实窗口中显示。文字必须有确定性测试和至少一条像素或
布局集成测试。

原因：专业编辑器的标签、菜单、标签页、属性面板和节点名称全部依赖文字测量；
在没有文字闭环时实现 Dock 只能制造临时尺寸常量和返工。

### 2. Dock 前置 UI 基础设施

在进入 `ur_dock` 前，`ur_widgets` 至少补齐：

- 作用域化 ID；
- clipping/scissor；
- overlay 层级；
- 基础布局与文字尺寸消费；
- 指针 capture 与 focus-loss 的应用级集成测试。

这些能力必须被一个真实编辑器面板消费。

### 3. `ur_dock`

实现 split tree、tab strip、拖拽预览、最小尺寸约束、持久化和恢复。Dock 的完成
判定必须包含真实编辑器多面板操作，不接受 placeholder 测试。

### 4. `ur_nodegraph` 与 `ur_viewport`

Dock 完成后，根据实际风险重新排序。两者都不能仅靠 mock 数据声明生产闭环。

### 5. `ur_scene_bridge`

前端开发可在缺少引擎二进制时继续，但引擎集成阶段必须记录 C ABI 来源版本、能力
协商、错误传播和 ABI 兼容门禁，并通过真实 UltraRender 库测试。

## 变更规则

只有出现新的可验证架构风险、依赖关系或实验结果时，才能修改上述顺序。变更必须
同步更新 ADR，不能以临时偏好替代工程证据。
