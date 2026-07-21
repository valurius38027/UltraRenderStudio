# UltraRender Studio

UltraRenderStudio 是 UltraRender 离线渲染引擎的独立桌面编辑器前端。项目采用
C++20、Qt 6 `QWindow`、QRhi 和自绘立即模式 UI；UltraRender 引擎通过稳定的
C ABI 边界接入。

当前仓库已完成 **Production Bootstrap**，不再只是“能打开空窗口”的架构骨架：

- `ur_platform` 将 Qt 事件翻译为后端无关 FIFO 事件流；
- `ur_gfx` 同时提供确定性的 Vulkan 离屏回读路径和真实 `QWindow` swapchain/present；
- `ur_widgets` 的 `DrawList` 已被真实 QRhi/Vulkan 消费，支持顶层命中仲裁和捕获清理；
- `ur_editor` 已有事件循环、交互按钮、resize/present 和可自动退出的 smoke 模式；
- GCC/Clang 严格警告门禁以及 Xvfb/Lavapipe 集成测试已闭合。

这仍不是完整编辑器。文字、Dock、节点图、3D 视口和完整引擎会话仍处于后续阶段。

## 模块结构

```text
libs/
├── ur_platform      QWindow 生命周期和后端无关事件 FIFO
├── ur_gfx           QRhi 抽象；当前实现 Vulkan 离屏与窗口 swapchain
├── ur_text          文字布局/字形缓存占位，下一正式里程碑
├── ur_widgets       立即模式 Context、DrawList、按钮与 ur_gfx 翻译层
├── ur_dock          停靠布局占位；被最小文字和 UI 基础设施阻塞
├── ur_nodegraph     节点图占位
├── ur_viewport      3D 视口占位
└── ur_scene_bridge  UltraRender C ABI 会话包装
```

依赖只允许按顶层 `CMakeLists.txt` 和
`cmake/module_dependency_lint.py` 声明的方向流动。构建会执行分层检查。

## 权威开发环境

当前受支持的生产开发宿主是 Debian 13 amd64。工具链由独立仓库的不可变 Release
维护，不应在宿主清理后重新手工拼装依赖：

```bash
git clone https://github.com/valurius38027/toolchain.git
cd toolchain
sudo bash profiles/ultrarender/scripts/restore.sh latest
```

已发布的确定性回滚版本：

```text
ultrarender-sdk-debian13-v2026.07.20.1
```

该 SDK 包含 Qt 6.8 `GuiPrivate`/QRhi、Vulkan/Lavapipe、GTest、FreeType、
HarfBuzz、FlatBuffers、GCC、Clang、CMake、Ninja 和着色器工具链。

## 严格构建与测试

### GCC + ASan/UBSan

```bash
cmake -S . -B build/strict-gcc -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DUR_WARNINGS_AS_ERRORS=ON \
  -DUR_ENABLE_SANITIZERS=ON
cmake --build build/strict-gcc --parallel 2

VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ctest --test-dir build/strict-gcc --output-on-failure
```

### Clang

```bash
cmake -S . -B build/strict-clang -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DUR_WARNINGS_AS_ERRORS=ON \
  -DUR_ENABLE_SANITIZERS=OFF
cmake --build build/strict-clang --parallel 2

VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ctest --test-dir build/strict-clang --output-on-failure
```

运行真实编辑器窗口的有限帧 smoke：

```bash
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ./build/strict-gcc/apps/ur_editor/ur_editor --frames 5
```

成功输出包含非零 `presented_frames=`。不传 `--frames` 时进入正常交互事件循环。

## 图形边界

`RenderDevice::createOffscreen()` 用于像素回读和回归测试；
`RenderDevice::createForWindow()` 接收 `ur::platform::Window&`，禁止向上层泄漏
`WId`、`HWND` 或其他整数式原生句柄。窗口设备在 `QWindow` exposed 后惰性创建
QRhi 和 swapchain，避免在不存在有效平台 surface 时选择 present queue。

`Backend` 枚举预留 D3D11、D3D12 和 Metal，但当前只有 Vulkan 实现。不要把 API
预留误述为多后端已经可用。

## UltraRender 引擎接入

`ur_scene_bridge` 默认只编译 C ABI 包装层。提供真实引擎库后重新配置：

```bash
export UR_ULTRARENDER_LIB_DIR=/path/to/ultrarender/lib
cmake -S . -B build/strict-gcc -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DUR_WARNINGS_AS_ERRORS=ON
```

没有真实引擎库时，前端、窗口、UI 和图形测试仍必须独立通过。

## 仓库与维护策略

权威源码仓库是 `https://github.com/valurius38027/UltraRenderStudio.git`，只维护
`main`。远端不保留功能、开发或 bundle-vault 分支；隔离开发可以使用本地临时
worktree，但完成后必须回到 `main` 并清理。仓库自动化规则见 `AGENTS.md`，当前
代码现实和下一里程碑见 `INDEX.md`。

## 架构与阶段持久化

架构决策位于 `docs/architecture/`。当前实现入口与后续顺序以 ADR-005、ADR-007
和 ADR-008 为准。

每个正式阶段结束后必须：

1. 运行双编译器严格门禁；
2. 创建带注释阶段 tag；
3. 生成包含所有 refs 的完整 `git bundle`；
4. 执行 `git bundle verify`；
5. 从 bundle 全新 clone 并运行 `git fsck --full`；
6. 将 bundle、SHA-256 sidecar 和验证报告发布为不可变 GitHub Release。

## 代码规范

- C++20；禁止关闭 `UR_WARNINGS_AS_ERRORS` 来绕过问题；
- `.clang-format` / `.clang-tidy` 作为提交前静态门禁；
- 新模块依赖必须同步更新 dependency lint；
- 改变模块边界、生命周期或输入语义时必须新增或更新 ADR；
- 不允许用 placeholder smoke 测试宣称模块功能完成。

`PHASE_VERSION` 保存最近完成的阶段名。`main` 上的严格验证通过且对应
`phase/<PHASE_VERSION>` 尚不存在时，GitHub Actions 会创建注释 tag、完整 Git
bundle、sidecar、验证报告，并在远端回下载后再次核对 SHA-256。
