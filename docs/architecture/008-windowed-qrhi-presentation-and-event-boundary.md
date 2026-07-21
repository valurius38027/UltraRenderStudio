# ADR-008: QWindow、QRhi swapchain 与平台事件边界

- 状态: 已采纳
- 日期: 2026-07-20

## 背景

早期 `Window::nativeSurfaceHandle()` 把 `QWindow::winId()` 转成 `void*` 交给
`ur_gfx`，但图形设备完全忽略该值。它既不是可移植 surface 抽象，也无法满足
QRhi swapchain 需要的 `QWindow*` 生命周期；测试实际只覆盖离屏渲染。

同时，Qt/Vulkan 在窗口尚未形成有效平台 surface 时选择 present queue，会导致
QRhi 初始化失败。窗口输入若直接向上泄漏 Qt event 类型，也会破坏模块边界。

## 决策

### 所有权

- `ur::platform::Window` 独占一个私有 `QWindow` 子类和事件 FIFO。
- `ur::gfx::RenderDevice::createForWindow()` 接收 `Window&`；Window 必须比设备活得久。
- 只有 `ur_platform/detail/qt_window_access.hpp` 能在实现层取出 `QWindow*`。
- 公共 API 禁止 `WId`、`HWND`、`NSView*` 或整数式 native handle。
- 窗口设备独占 `QVulkanInstance`、QRhi、swapchain、深度附件、render-pass descriptor、
  pipeline、SRB 和缓冲区；析构时按依赖逆序释放，并在最后解除 QWindow 的 Vulkan
  instance。

`EditorApp` 通过成员声明顺序保证 RenderDevice 先析构、Window 后析构。

### 初始化时序

1. Window 构造时声明 `VulkanSurface`，尚不强制创建平台窗口。
2. Window RenderDevice 构造时创建带 QRhi 推荐扩展的 `QVulkanInstance`，并在
   `show()` 前设置到 QWindow。
3. QRhi 和 swapchain 不在构造时创建；第一次窗口 exposed 后由
   `resizeSwapChain()`/`presentRects()` 惰性初始化。
4. 离屏设备不需要平台 surface，仍在构造时立即初始化 QRhi。
5. swapchain 使用 `surfacePixelSize()`，而 UI 坐标保持 Qt logical pixels；投影矩阵
   由逻辑尺寸构造，viewport 使用实际像素尺寸。

### 帧结果

`presentRects()` 返回：

- `Presented`：帧已提交并呈现；
- `SkippedNotExposed`：窗口隐藏、未暴露或尺寸无效，未开始 frame；
- `Resized`：检测到 swapchain out-of-date，已触发恢复，调用方下一帧重试；
- `DeviceLost`：不可在当前对象内透明恢复，应用停止渲染并给出明确诊断。

`beginFrame` 或 `endFrame` 的其他错误作为异常传播到应用边界，不能静默吞掉。

### 平台事件

私有 QWindow 子类把 Qt 事件翻译为后端无关 `WindowEvent` FIFO：

- Exposed；
- Resized；
- PointerMoved；
- PointerButtonChanged；
- FocusLost；
- CloseRequested。

`EditorApp` 每帧先排空 FIFO，再更新 Context。FocusLost 必须同时重置鼠标按下状态并
调用 `cancelPointerCapture()`；resize/expose 只设置待重建标志，实际 swapchain 操作
由渲染帧执行。

## 测试要求

- 离屏 pixel-readback 测试继续使用 `createOffscreen()`；
- 窗口集成测试必须在 Xvfb/Lavapipe 下真实 `show -> expose -> present -> resize ->
  present`；
- 编辑器 smoke 必须支持 `--frames N`，输出非零 `presented_frames` 后退出；
- 仅创建对象或传递未使用句柄不构成窗口渲染测试。

## Sanitizer 边界

在 GCC/ASan 下，真实 swapchain 测试的功能断言全部通过，但进程退出时
LSan 会报告一个 72 字节分配，其唯一分配栈位于 `libvulkan.so.1`。显式释放
所有项目持有的 QRhi 资源、从 `QWindow` 解除 Vulkan instance，并显式调用
`QVulkanInstance::destroy()` 后，该报告仍然存在；离屏 QRhi 路径则保持无泄漏。
因此，仅对真实 Vulkan 窗口测试应用
`cmake/lsan-vulkan-loader.supp` 中的定向规则，其他目标继续执行完整的 LSan
检查。

## 后果

当前实现只支持 Vulkan。D3D11、D3D12 和 Metal 需要各自的 surface type、初始化参数
和平台测试，不能复用 Vulkan 的硬编码窗口类型，也不能因为 Backend 枚举存在就宣称
多后端完成。
