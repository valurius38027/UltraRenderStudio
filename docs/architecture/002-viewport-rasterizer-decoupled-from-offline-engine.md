# ADR-002: 视口用前端自研光栅化,UltraRender 离线引擎仅显式触发

- 状态: 已采纳
- 日期: 2026-07-18

## 背景

UltraRender 后端目前是离线 CUDA 路径追踪器(`RenderSession` 渐进式采样,
`render_pass()` 循环调用),不提供交互式实时视口。最初担心这会迫使编辑
态视口做成收敛式预览交互(类似 Arnold IPR),体验上和 Blender/Houdini 的
实时视口手感有本质差别。

## 决策

编辑态视口(相机操纵、gizmo、物体拾取)完全由前端自己的 `ur_gfx`
(QRhi + Vulkan/D3D 光栅化)实现,不依赖 UltraRender。UltraRender 的
`RenderSession` 只在用户显式点击"渲染"时启动,类似 Blender 的
solid/material preview viewport(EEVEE/OpenGL) vs F12 最终渲染(Cycles)
的解耦方式。

节点图编辑期间的实时反馈(改参数看到网格变化)通过 CPU 侧
`build_procedural_scene()` 重新编译图 -> 前端光栅化器读取新
`SceneIR` 更新视口几何,不经过 CUDA 引擎。

## 考虑过的替代方案

- 视口直接用 RenderSession 的渐进式采样做实时预览: 需要设计"拖动时降
  采样、松手后收敛"这类复杂交互,且和 Blender/Houdini 的用户肌肉记忆
  不符。否决。

## 后果

- 好处: 编辑态交互设计大幅简化,不需要处理渲染会话取消/重启、AOV
  分层显示等复杂度。
- 代价: `ur_gfx`/`ur_viewport` 需要自己实现相机控制器、拾取、gizmo ——
  这些不能从 UltraRender 白拿。
- 已知风险: `build_procedural_scene()` 目前是整图重编,没有节点级增量
  求值。图规模大时(数千 Scatter 实例)可能有明显延迟,需要前端做防抖,
  或未来推动后端补充增量求值 API。此风险已知但暂不阻塞 MVP。
