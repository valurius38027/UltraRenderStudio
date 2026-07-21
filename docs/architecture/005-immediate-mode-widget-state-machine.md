# ADR-005: 立即模式部件的两阶段交互状态机

- 状态: 已采纳（2026-07-20 修订）
- 初始日期: 2026-07-18
- 修订日期: 2026-07-20

## 背景

立即模式调用顺序同时承担绘制 Z 序。旧实现虽然让最后提交的部件获得 hover，
却在每次 `button()` 调用时立即分配 active，导致重叠区域中的底层部件先抢到
按下；如果 active 部件在释放帧不再提交，active 也永远无法清除。

因此，命中测试不能在尚未看到完整提交序列时立即作最终决定。

## 决策

采用**提交阶段 + 帧末解析阶段**：

1. `beginFrame()` 保存指针状态并清空本帧提交列表和 DrawList。
2. 每次 `button()` 按从底到顶的顺序登记 `{id, rect, commandIndex}`，先产生矩形
   命令，但不立即决定本帧 hover/active/click。
3. `endFrame()` 从提交列表尾部向前查找第一个命中的矩形，作为唯一 topmost hover。
4. 按下边沿只把该 topmost hover 设为 active；同一时刻最多一个 active。
5. 释放边沿只有在 active 仍被提交且释放位置仍命中同一 topmost 部件时产生 click；
   无论成功与否都清除 active。
6. active ID 如果本帧不再提交，也立即视为失效并清除，不能阻塞后续部件。
7. 平台焦点或指针捕获丢失时，调用 `cancelPointerCapture()` 清除 active。
8. 命中和状态解析完成后，`endFrame()` 回写最终 normal/hover/active 颜色，使 DrawList
   的视觉 Z 序与输入 Z 序一致。

## 点击报告时序

完整 click 只能在 `endFrame()` 看到所有提交后确认。因此 `button()` 返回的是**上一
次 `endFrame()` 已解析的 click**，有效期为下一提交帧；`wasClicked(id)` 可在同一
时段显式查询。调用方必须按持续帧循环使用 Context，而不能假设释放事件发生在
`button()` 调用前就已经知道 topmost 归属。

这个一帧交接是当前最小实现的明确契约，不应被误认为输入延迟缺陷。未来如引入
独立布局/命中预通道，可以在保持 topmost 语义的前提下重新评估报告时序。

## 取消语义

- 按下后拖出并释放：不点击，active 清零；
- active 部件在任何帧消失：active 清零；
- focus/capture loss：active 清零，应用层同时应把按键状态重置为未按下；
- 一个部件不能窃取另一个仍有效的 active capture。

## 已知限制

- ID 仍是 label 的全局扁平哈希；同帧重复 label 会碰撞。Dock 之前必须建立作用域
  ID 栈或等价机制。
- 当前只有单指针和鼠标左键按钮语义；键盘导航、IME、触摸、多按钮和拖放尚未实现。
- clipping、overlay/popup 优先级和多窗口 hit-test 尚未进入本 ADR 的范围。
