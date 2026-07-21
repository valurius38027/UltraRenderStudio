# UltraRenderStudio Production Bootstrap Design

## Status

Approved scope: **Approach A — Production Bootstrap**.

This design converts the current offscreen feasibility skeleton into the first real editor loop without starting Dock, NodeGraph, or production text work prematurely.

## Objective

Deliver a Vulkan-backed UltraRenderStudio window that:

1. builds under the repository's strict warnings-as-errors policy;
2. presents QRhi-rendered rectangles through a real `QWindow` swapchain;
3. processes resize, exposure, pointer movement, left-button press/release, and close events through backend-neutral platform events;
4. renders a self-drawn interactive button whose hover, active, and click semantics remain correct under overlap and disappearing-widget edge cases;
5. retains the existing offscreen readback path for deterministic render tests;
6. is recoverable after host cleanup through committed Git history and a verified phase bundle.

## Non-goals

- Dock layout or tab implementation.
- Text shaping, glyph atlases, or label rendering.
- NodeGraph or viewport integration.
- D3D11, D3D12, Metal, or OpenGL implementation.
- UltraRender engine session integration beyond preserving the existing bridge.
- General-purpose application framework or retained widget tree.

## Architecture

### Platform boundary

`ur_platform::Window` remains the public owner of window lifetime. Its public header continues to avoid Qt types.

A private integration header under `ur/platform/detail/` exposes the underlying `QWindow*` only to implementation-layer consumers such as `ur_gfx`. The old integer-like `nativeSurfaceHandle()` API is removed because QRhi swapchain creation requires the `QWindow` object, not a native integer handle.

The window contains a FIFO event queue. Qt events are translated into backend-neutral `WindowEvent` records:

- `Exposed`
- `Resized`
- `PointerMoved`
- `PointerButtonChanged`
- `CloseRequested`

The queue is drained by the editor shell. Qt callbacks never invoke widget or renderer code directly.

### Graphics boundary

`RenderDevice` gains explicit factories:

```cpp
static std::unique_ptr<RenderDevice> createOffscreen(Backend backend);
static std::unique_ptr<RenderDevice> createForWindow(
    Backend backend,
    ur::platform::Window& window);
```

The Vulkan implementation owns one `QVulkanInstance` and one `QRhi`. Windowed mode additionally owns:

- `QRhiSwapChain`
- compatible render-pass descriptor
- persistent rectangle pipeline state
- persistent or reusable vertex/uniform resources where appropriate

The device provides:

```cpp
virtual void resizeSwapChain() = 0;
virtual PresentResult presentRects(
    const std::vector<RectPrimitive>& rects) = 0;
```

`PresentResult` distinguishes `Presented`, `SkippedNotExposed`, `Resized`, and `DeviceLost`. Recoverable swapchain-out-of-date conditions trigger resize/retry; unrecoverable device loss is returned to the editor rather than silently ignored.

The existing `renderRects()` and `renderDebugTriangleToTexture()` offscreen APIs remain available for deterministic tests.

### Widget interaction model

The current immediate mutation model cannot correctly resolve topmost activation because an early widget can claim `activeId_` before later overlapping widgets are known.

The bootstrap uses two-stage per-frame arbitration:

1. widget calls register hit candidates and drawing commands in draw order;
2. `endFrame()` resolves the topmost hovered widget, press ownership, release/click, stale active state, and final visual states;
3. click results are consumed by widget ID on the next editor tick or through a resolved result table.

The implementation must satisfy:

- the final/topmost overlapping widget owns hover and a new press;
- an active widget retains capture while held, even outside its bounds;
- release outside cancels click and clears active;
- release when the active widget was not submitted clears active;
- focus/capture loss clears active;
- one physical click produces at most one logical click.

Because the editor button has no text yet, it is identified visually by shape and state only. Text is the next milestone.

### Editor loop

`ur_editor` constructs the window, creates the windowed Vulkan device before showing the window, then runs a small `QTimer`-driven pump.

Each tick:

1. drain platform events;
2. update pointer state and process resize/exposure changes;
3. begin the widget frame;
4. submit the bootstrap button;
5. resolve the widget frame;
6. translate `DrawList` into graphics primitives;
7. call `presentRects()` only when the window is exposed and has non-zero pixel size.

A click toggles a deterministic application state, changing the button's base color so interaction is visually observable without text.

## Build-system corrections

- Link `ur_gfx` privately to `Qt6::GuiPrivate` instead of manually adding Qt private include paths.
- Treat imported/private Qt headers as system headers so repository `-Werror` applies only to project code.
- Remove stale Qt 6.4-specific comments and fallback include construction.
- Correct all size conversions with checked bounds before passing Qt integer sizes.
- Keep GCC and Clang strict builds as required gates.

## Testing

### Unit tests

- Platform event translation and FIFO order.
- Resize and pointer state payloads.
- Widget overlap press ownership.
- Active widget disappearance on release.
- Capture-loss cleanup.
- Existing click, drag-cancel, and draw-list tests.

### Integration tests

- Windowed `RenderDevice` creation receives the real `QWindow` through the private bridge.
- Swapchain creates/resizes under Xvfb with Lavapipe.
- At least one frame presents successfully after exposure.
- Resize followed by present does not produce device loss.

### Regression gates

- Dependency-layer lint.
- GCC Debug + ASan/UBSan + `UR_WARNINGS_AS_ERRORS=ON`.
- Clang RelWithDebInfo + `UR_WARNINGS_AS_ERRORS=ON`.
- All CTest targets pass.
- Headless editor smoke process starts, presents frames, and exits through a test-only frame limit.

## Persistence and repository policy

The imported archive is committed as baseline `c65fcf8` on `main`.

Implementation occurs on `feat/production-bootstrap` in an isolated worktree. Each independently accepted task is committed. At the completion of each phase:

1. run the full phase verification matrix;
2. create an annotated phase tag;
3. create a complete `git bundle` containing all refs required to reconstruct the repository;
4. write a SHA-256 sidecar;
5. store a persistent copy in the toolchain repository's dedicated UltraRenderStudio bundle-vault branch until a canonical standalone remote is available;
6. provide the local downloadable bundle as an additional convenience, not as the authority.

## Completion criteria

Production Bootstrap is complete only when:

- strict GCC and Clang builds pass;
- all unit/integration tests pass;
- Xvfb/Lavapipe demonstrates real swapchain presentation;
- the editor displays and interacts with a self-drawn button;
- stale ADR and README claims are corrected;
- the phase tag and verified bundle are published to persistent storage.
