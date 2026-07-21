# UltraRender Studio

UltraRenderStudio 是 UltraRender 离线渲染引擎的独立桌面编辑器前端。项目采用 C++20、
Qt 6 `QWindow`、QRhi 和自绘立即模式 UI；UltraRender 引擎通过独立 C ABI 边界接入。

当前完成 **Production Bootstrap** 与 **Minimal Text Rendering Closure**：

- `ur_platform` 提供后端无关窗口事件 FIFO；
- `ur_gfx` 提供 Vulkan 离屏回读、真实 swapchain/present、ordered `UiFrame`、R8 atlas 和
  alpha-masked quad；
- `ur_text` 使用 HarfBuzz + FreeType 完成显式字体加载、UTF-8 单行 shaping、测量、灰度
  raster 和确定性 glyph atlas；
- `ur_widgets` 生成有序 rectangle/text 命令并按 metrics 居中按钮 label；
- `ur_editor` 在真实窗口中呈现可交互、带文字的自绘按钮；
- 双编译器、ASan/UBSan、glyph shader 复现、像素回读和 Xvfb/Lavapipe 门禁已建立。

这仍不是完整编辑器。下一阶段是 Dock 前置 UI Foundation，而不是直接实现 Dock。

## 模块结构

```text
libs/
├── ur_platform      QWindow 生命周期与后端无关事件 FIFO
├── ur_gfx           Vulkan QRhi、ordered UiFrame、R8 atlas、离屏/窗口呈现
├── ur_text          字体、UTF-8 shaping、测量、灰度 raster、CPU glyph atlas
├── ur_widgets       立即模式 Context、有序 DrawList、按钮 label 与 UiFrame 翻译
├── ur_dock          占位；被 UI Foundation 阻塞
├── ur_nodegraph     占位
├── ur_viewport      占位
└── ur_scene_bridge  UltraRender C ABI 会话包装
```

## 权威开发环境

支持 Debian 13 amd64。恢复不可变 SDK：

```bash
git clone https://github.com/valurius38027/toolchain.git
cd toolchain
sudo bash profiles/ultrarender/scripts/restore.sh latest
```

确定性回滚版本：

```text
ultrarender-sdk-debian13-v2026.07.21.1
```

该 SDK 锁定 Qt 6.8/QRhi、Vulkan/Lavapipe、GCC、Clang、CMake、Ninja、GTest、FreeType、
HarfBuzz、DejaVu Sans、Qt qsb、FlatBuffers 和 shader 工具链。默认字体路径：

```text
/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
```

## 严格构建与测试

### GCC + ASan/UBSan

```bash
cmake -S . -B build/strict-gcc -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DUR_BUILD_TESTS=ON \
  -DUR_WARNINGS_AS_ERRORS=ON \
  -DUR_ENABLE_SANITIZERS=ON
cmake --build build/strict-gcc --parallel 2

VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ctest --test-dir build/strict-gcc --output-on-failure --no-tests=error
```

Leak detection remains enabled. Narrow third-party process-lifetime boundaries are assigned per CTest target
and documented by ADR-006/008/009.

### Clang

```bash
cmake -S . -B build/strict-clang -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DUR_BUILD_TESTS=ON \
  -DUR_WARNINGS_AS_ERRORS=ON \
  -DUR_ENABLE_SANITIZERS=OFF
cmake --build build/strict-clang --parallel 2

VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ctest --test-dir build/strict-clang --output-on-failure --no-tests=error
```

### Editor smoke

```bash
LSAN_OPTIONS=suppressions="$PWD/cmake/lsan-vulkan-loader.supp" \
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ./build/strict-gcc/apps/ur_editor/ur_editor --frames 5
```

成功摘要必须含非零：

```text
presented_frames=...
text_glyphs=...
atlas_revision=...
```

## 文字 v1 边界

- 一个 `FontId` 对应一个显式字体文件和 pixel size；
- 支持合法 UTF-8 的单行、单 run shaping，包括常见 ligature/kerning、combining mark 和单一
  RTL run；
- 缺字使用 `.notdef`；
- 不支持字体 fallback、Fontconfig 发现、混合方向段落算法、换行/截断、color emoji、富文本、
  MSDF、高 DPI atlas policy、分页、回收或 repack；
- atlas 耗尽是显式错误，不覆盖旧 glyph。

## 图形边界

`RenderDevice::createOffscreen()` 用于确定性像素回归，`createForWindow()` 接受
`ur::platform::Window&`。`UiFrame` 同步借用 CPU atlas pixels；设备不会在调用后持有 span。
窗口设备只在 atlas extent/revision 改变或 GPU 资源重建时上传 R8 texture。

`Backend` 仍只实现 Vulkan。D3D11、D3D12、Metal 只是 API 预留。

## UltraRender 引擎接入

未设置 `UR_ULTRARENDER_LIB_DIR` 时，前端、文字、UI、窗口和图形门禁独立运行。真实引擎
阶段必须提供上游 C ABI provenance、能力协商和兼容测试。

## 阶段持久化

正式阶段必须在 `main` 全绿后创建不可变 `phase/*` tag，并发布完整 Git bundle、SHA-256
sidecar 和验证报告。发布工作流会执行 `git bundle verify`、fresh clone、`git fsck --full`
以及远端下载后的 checksum round-trip。
