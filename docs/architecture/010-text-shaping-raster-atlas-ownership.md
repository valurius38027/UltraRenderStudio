# ADR-010: 文字 shaping、光栅化、atlas 与 GPU 传输所有权

- 状态: 已采纳
- 日期: 2026-07-21

## 背景

编辑器的菜单、标签页、属性面板、节点名称和 Dock 尺寸都依赖可靠文字测量。使用 Qt
文字绘制会把自绘 UI 绑定到平台字体发现和 Qt 绘制语义；把 FreeType/HarfBuzz 直接放进
`ur_widgets` 或 QRhi 实现则会破坏模块边界。

## 决策

1. `ur_text` 独立拥有 FreeType/HarfBuzz 生命周期、UTF-8/单行验证、shaping、测量、
   灰度 raster、glyph cache 和 CPU atlas；公开头不暴露第三方或图形类型。
2. 一个 `FontId` 对应一个显式文件和固定 pixel size。v1 使用 SDK 锁定的 DejaVu Sans，
   不进行 Fontconfig 查询或 fallback。
3. atlas 为 1024×1024 R8、1 像素 padding、确定性 shelf allocator、单页、单调追加，
   不移动、不回收、不 repack；耗尽时抛出 `AtlasFullError`。
4. `shape()`/`measure()` 不修改 atlas；`prepare()` 才保证非空 glyph 有 raster bitmap 和
   atlas rect。prepared bounds 使用实际 FreeType bitmap bearing/extent。
5. `ur_widgets` 只决定 label、颜色、origin 和基于 metrics 的居中，并维持 rectangle/text
   有序命令流；文字不产生独立 hit region。
6. `ur_gfx` 不认识 Unicode、字体或 glyph ID，只消费通用 R8 atlas 和 masked quad。
   atlas span 在调用内同步消费，GPU texture 由设备拥有。
7. atlas 仅在 extent/revision 改变或 GPU 资源重建时上传。矩形与文字只能合并相邻同类
   batch，不能跨命令重排；提交顺序就是 Z 序。
8. 新 glyph GLSL 和 checked-in `.qsb` 由 SDK 锁定的 Qt qsb 做字节级构建验证。旧 bootstrap
   shaders 暂不重写，作为单独技术债记录。

## 确定性环境

权威 SDK 为 `ultrarender-sdk-debian13-v2026.07.21.1`，显式锁定：

- `fonts-dejavu-core`；
- FreeType 和 HarfBuzz；
- `qt6-shader-baker`；
- Qt 6.8 QRhi、Vulkan 和 Lavapipe。

SDK 的 clean-container gate 会真实加载字体、shape UTF-8、生成 `.qsb`，并在移除外部 APT
源后完成全部验证。

## v1 边界

支持任意合法 UTF-8 的单个 shaped run，包括常见 ligature/kerning、combining mark 和单一
RTL run。缺字使用 glyph 0 `.notdef`。不支持 fallback、完整混合 BiDi 段落、换行、
富文本、color font、MSDF、高 DPI cache policy、atlas paging 或 eviction。

## 证据

- `ur_text` 行为测试覆盖输入错误、metrics、RTL/combining、`.notdef`、revision、复用、
  确定性 placement 和 exhaustion；
- `ur_gfx` 像素测试覆盖 mask coverage、tint/alpha 和 rect/text/rect Z 序；
- 窗口测试覆盖 revision upload suppression、revision change、resize 和 present；
- 端到端测试执行 UTF-8 → HarfBuzz → FreeType → atlas → DrawList → UiFrame → Vulkan → pixels；
- editor smoke 报告正数 `text_glyphs`、`atlas_revision` 和 `presented_frames`。

## 后果

文字基础可被后续布局和 Dock 使用，同时 GPU 后端仍与字体实现解耦。代价是 v1 atlas 有
容量上限且没有 fallback/多行能力；这些必须在出现真实产品需求和相应失效测试后扩展。
