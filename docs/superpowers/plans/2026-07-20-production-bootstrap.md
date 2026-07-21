# UltraRenderStudio Production Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert the offscreen architecture skeleton into a strict-build, real-window Vulkan editor loop with correct pointer interaction and a persistently recoverable phase bundle.

**Architecture:** Preserve `ur_platform -> ur_gfx -> ur_widgets -> ur_editor` layering. Replace the invalid native-handle abstraction with a private `QWindow` bridge, add a windowed QRhi swapchain beside the existing offscreen path, translate Qt events into a platform event queue, and resolve widget hit-testing in a two-stage frame finalization pass.

**Tech Stack:** C++20, CMake 3.25+, Qt 6.8 `Core`, `Gui`, `GuiPrivate`, QRhi Vulkan, Vulkan/Lavapipe, GoogleTest, Xvfb, GCC 14, Clang 17, Git bundles.

## Global Constraints

- Supported production-bootstrap host: Debian 13 amd64.
- Vulkan is the only implemented backend in this phase.
- Public `ur_platform/window.hpp` must not expose Qt types.
- Existing offscreen render/readback APIs remain supported.
- `UR_WARNINGS_AS_ERRORS=ON` is mandatory for completion.
- Dock, text, node graph, viewport, and scene-engine integration are out of scope.
- Every task ends in a focused commit and every phase ends in a verified complete Git bundle.

---

## File map

**New files**

- `libs/ur_platform/include/ur/platform/events.hpp` — backend-neutral event types and payloads.
- `libs/ur_platform/include/ur/platform/detail/qt_window_access.hpp` — private Qt bridge used by `ur_gfx` only.
- `libs/ur_gfx/include/ur/gfx/present.hpp` — window-presentation result contract.
- `apps/ur_editor/src/editor_app.hpp` — editor orchestration state.
- `apps/ur_editor/src/editor_app.cpp` — timer tick, event drain, widget build, and present loop.
- `tests/integration/window_present_integration_test.cpp` — Xvfb/Lavapipe real-swapchain test.
- `cmake/create_phase_bundle.sh` — deterministic phase tag/bundle/checksum helper.

**Modified files**

- `libs/ur_platform/include/ur/platform/window.hpp` — event polling, exposure state, and removal of integer native handle.
- `libs/ur_platform/src/window.cpp` — `QWindow` subclass and event translation.
- `libs/ur_platform/tests/window_test.cpp` — event queue and ordering tests.
- `libs/ur_gfx/include/ur/gfx/render_device.hpp` — explicit offscreen/windowed factories and present API.
- `libs/ur_gfx/src/vulkan_render_device.cpp` — strict-build cleanup, swapchain lifecycle, and shared rectangle rendering.
- `libs/ur_gfx/CMakeLists.txt` — `Qt6::GuiPrivate`, no manual private include fallback.
- `libs/ur_gfx/tests/render_device_test.cpp` — factory and invalid-size coverage.
- `libs/ur_widgets/include/ur/widgets/context.hpp` — two-stage interaction result contract.
- `libs/ur_widgets/src/context.cpp` — topmost arbitration and stale-active cleanup.
- `libs/ur_widgets/tests/context_test.cpp` — overlap/disappearance/capture-loss regressions.
- `libs/ur_widgets/include/ur/widgets/render.hpp` and `src/render.cpp` — primitive translation reusable by present and readback.
- `apps/ur_editor/src/main.cpp` and `apps/ur_editor/CMakeLists.txt` — real editor application assembly.
- `tests/integration/CMakeLists.txt` and `platform_gfx_integration_test.cpp` — replace native-handle smoke with real private bridge/device creation.
- `README.md` and `docs/architecture/007-module-sequencing-policy.md` — code-reality update.

---

### Task 1: Close the strict build gate

**Files:**
- Modify: `libs/ur_gfx/CMakeLists.txt`
- Modify: `libs/ur_gfx/src/vulkan_render_device.cpp`
- Modify: `README.md`

**Interfaces:**
- Consumes: existing `RenderDevice` API.
- Produces: unchanged runtime API with GCC/Clang warnings-as-errors clean.

- [ ] **Step 1: Add a strict configure/build regression command**

Run:

```bash
cmake -S . -B build/strict-gcc -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DUR_BUILD_TESTS=ON \
  -DUR_ENABLE_SANITIZERS=ON \
  -DUR_WARNINGS_AS_ERRORS=ON
cmake --build build/strict-gcc --parallel 2
```

Expected before implementation: FAIL in Qt private headers and at the project size conversion.

- [ ] **Step 2: Replace manual private include paths**

Change `libs/ur_gfx/CMakeLists.txt` to require and link the imported private target:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Core Gui)

target_link_libraries(ur_gfx
    PUBLIC Qt6::Core Qt6::Gui
    PUBLIC ur::platform
    PRIVATE Qt6::GuiPrivate Vulkan::Vulkan
)
```

Delete `Qt6Gui_PRIVATE_INCLUDE_DIRS` and fixed-path fallback logic.

- [ ] **Step 3: Add checked QRhi buffer-size conversion**

In `vulkan_render_device.cpp`, introduce:

```cpp
[[nodiscard]] int checkedByteSize(std::size_t elementCount, std::size_t elementSize) {
    constexpr auto kMax = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (elementSize != 0U && elementCount > kMax / elementSize) {
        throw std::overflow_error("ur_gfx: vertex buffer size exceeds QRhi int range");
    }
    return static_cast<int>(elementCount * elementSize);
}
```

Use it for dynamic vertex data. Include `<limits>` and `<stdexcept>`.

- [ ] **Step 4: Run strict GCC and Clang builds**

```bash
cmake --build build/strict-gcc --parallel 2
ctest --test-dir build/strict-gcc --output-on-failure
cmake -S . -B build/strict-clang -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DUR_BUILD_TESTS=ON \
  -DUR_WARNINGS_AS_ERRORS=ON
cmake --build build/strict-clang --parallel 2
ctest --test-dir build/strict-clang --output-on-failure
```

Expected: both builds succeed and 9/9 existing tests pass.

- [ ] **Step 5: Commit**

```bash
git add libs/ur_gfx/CMakeLists.txt libs/ur_gfx/src/vulkan_render_device.cpp README.md
git commit -m "build: close strict Qt QRhi warning gate"
```

---

### Task 2: Introduce platform events and the private Qt window bridge

**Files:**
- Create: `libs/ur_platform/include/ur/platform/events.hpp`
- Create: `libs/ur_platform/include/ur/platform/detail/qt_window_access.hpp`
- Modify: `libs/ur_platform/include/ur/platform/window.hpp`
- Modify: `libs/ur_platform/src/window.cpp`
- Modify: `libs/ur_platform/tests/window_test.cpp`

**Interfaces:**
- Produces:

```cpp
enum class WindowEventType { Exposed, Resized, PointerMoved, PointerButtonChanged, CloseRequested, FocusLost };
enum class PointerButton { Left, Right, Middle, Other };
struct WindowEvent { WindowEventType type; WindowSize size; float x; float y; PointerButton button; bool pressed; };
bool Window::pollEvent(WindowEvent& event);
bool Window::isExposed() const;
QWindow* ur::platform::detail::qtWindow(Window&) noexcept;
```

- [ ] **Step 1: Write failing FIFO and payload tests**

Add tests that send `QResizeEvent`, `QMouseEvent`, and `QFocusEvent` through `QCoreApplication::sendEvent`, then assert event type, payload, and ordering from `pollEvent()`.

- [ ] **Step 2: Verify the new tests fail to compile**

```bash
cmake --build build/strict-gcc --target ur_platform_tests
```

Expected: FAIL because event types and `pollEvent()` do not exist.

- [ ] **Step 3: Implement translated event queue**

Use a `QWindow` subclass inside `Window::Impl` and override `exposeEvent`, `resizeEvent`, `mouseMoveEvent`, `mousePressEvent`, `mouseReleaseEvent`, `focusOutEvent`, and `closeEvent`. Push value-type `WindowEvent` records into `std::deque<WindowEvent>`.

- [ ] **Step 4: Remove `nativeSurfaceHandle()` and add private bridge**

Declare only the detail accessor in `detail/qt_window_access.hpp`; keep Qt absent from `window.hpp`.

- [ ] **Step 5: Run platform tests and dependency lint**

```bash
cmake --build build/strict-gcc --target ur_platform_tests ur_lint_deps
ctest --test-dir build/strict-gcc -R ur_platform_tests --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/ur_platform
git commit -m "feat(platform): add backend-neutral window event queue"
```

---

### Task 3: Add explicit windowed rendering and QRhi swapchain presentation

**Files:**
- Create: `libs/ur_gfx/include/ur/gfx/present.hpp`
- Modify: `libs/ur_gfx/include/ur/gfx/render_device.hpp`
- Modify: `libs/ur_gfx/src/vulkan_render_device.cpp`
- Modify: `libs/ur_gfx/tests/render_device_test.cpp`
- Modify: `tests/integration/platform_gfx_integration_test.cpp`
- Create: `tests/integration/window_present_integration_test.cpp`
- Modify: `tests/integration/CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
enum class PresentResult { Presented, SkippedNotExposed, Resized, DeviceLost };
static std::unique_ptr<RenderDevice> createOffscreen(Backend backend);
static std::unique_ptr<RenderDevice> createForWindow(Backend backend, ur::platform::Window& window);
virtual void resizeSwapChain() = 0;
virtual PresentResult presentRects(const std::vector<RectPrimitive>& rects) = 0;
```

- [ ] **Step 1: Update offscreen tests to the explicit factory and write failing windowed tests**

Replace `create(..., nullptr)` with `createOffscreen()`. Add tests for windowed creation, real present, and resize/present under Xvfb/Lavapipe.

- [ ] **Step 2: Verify windowed tests fail**

```bash
cmake --build build/strict-gcc --target ur_gfx_tests ur_integration_tests
ctest --test-dir build/strict-gcc -R 'ur_gfx_tests|ur_integration_tests' --output-on-failure
```

Expected: compile failure for missing factories/present API.

- [ ] **Step 3: Implement windowed Vulkan initialization**

Use `detail::qtWindow(window)`, set the device-owned `QVulkanInstance` on the `QWindow` before native surface creation, create `QRhi`, `QRhiSwapChain`, render-pass descriptor, and reusable rectangle pipeline state.

- [ ] **Step 4: Implement resize and present result handling**

Handle zero-size/not-exposed windows without calling `beginFrame`. Retry once after `SwapChainOutOfDate`; return `DeviceLost` for unrecoverable device loss.

- [ ] **Step 5: Preserve offscreen readback behavior**

Run existing pixel tests unchanged except for factory naming. Pixel values must remain identical.

- [ ] **Step 6: Run integration tests under Xvfb/Lavapipe**

```bash
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ctest --test-dir build/strict-gcc \
  -R 'ur_gfx_tests|ur_integration_tests' --output-on-failure
```

Expected: all selected tests pass and at least one real frame reports `Presented`.

- [ ] **Step 7: Commit**

```bash
git add libs/ur_gfx tests/integration
git commit -m "feat(gfx): present rectangle frames through QRhi swapchain"
```

---

### Task 4: Correct widget topmost arbitration and stale active cleanup

**Files:**
- Modify: `libs/ur_widgets/include/ur/widgets/context.hpp`
- Modify: `libs/ur_widgets/src/context.cpp`
- Modify: `libs/ur_widgets/tests/context_test.cpp`

**Interfaces:**
- Produces:

```cpp
void Context::cancelPointerCapture();
bool Context::wasClicked(WidgetId id) const;
```

`button()` registers the widget and returns the previous resolved click state; `endFrame()` resolves current hit testing and click ownership.

- [ ] **Step 1: Replace the incorrect overlap expectation with topmost ownership**

Write a test where Bottom and Top overlap on a press edge. After `endFrame()`, `activeId()` must be Top.

- [ ] **Step 2: Add disappearing-active and capture-loss tests**

Press a widget, omit it on the release frame, call `endFrame()`, and assert `activeId() == 0`. Add `cancelPointerCapture()` coverage.

- [ ] **Step 3: Verify tests fail against the current implementation**

```bash
cmake --build build/strict-gcc --target ur_widgets_tests
ctest --test-dir build/strict-gcc -R ur_widgets_tests --output-on-failure
```

Expected: overlap and stale-active tests fail.

- [ ] **Step 4: Implement two-stage frame resolution**

Store submitted widget records in draw order, resolve topmost hover and new active ownership in `endFrame()`, track submitted IDs, and clear stale active state on release or capture loss.

- [ ] **Step 5: Run widget unit and render tests**

```bash
cmake --build build/strict-gcc --target ur_widgets_tests ur_widgets_render_tests
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ctest --test-dir build/strict-gcc \
  -R 'ur_widgets_tests|ur_widgets_render_tests' --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add libs/ur_widgets
git commit -m "fix(widgets): resolve topmost input and stale capture"
```

---

### Task 5: Assemble the first real editor frame loop

**Files:**
- Create: `apps/ur_editor/src/editor_app.hpp`
- Create: `apps/ur_editor/src/editor_app.cpp`
- Modify: `apps/ur_editor/src/main.cpp`
- Modify: `apps/ur_editor/CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
class EditorApp {
public:
    EditorApp();
    void show();
private:
    void tick();
};
```

- [ ] **Step 1: Add a test-only frame-limit CLI contract**

`ur_editor --frames 5` must show the window, attempt presentation, print a summary containing `presented_frames=`, and exit without user input.

- [ ] **Step 2: Implement event drain and pointer state**

Map platform events into `Context::beginFrame(mouseX, mouseY, leftDown)`. Call `cancelPointerCapture()` on `FocusLost`.

- [ ] **Step 3: Implement visible interactive state**

Submit one rectangle button. A resolved click toggles a boolean and changes the button's normal color on subsequent frames.

- [ ] **Step 4: Present only valid frames**

Skip presentation when hidden or zero-sized; call `resizeSwapChain()` after resize; abort with a clear diagnostic on `DeviceLost`.

- [ ] **Step 5: Run editor smoke**

```bash
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ./build/strict-gcc/apps/ur_editor/ur_editor --frames 5
```

Expected: exit code 0 and `presented_frames` greater than zero.

- [ ] **Step 6: Commit**

```bash
git add apps/ur_editor
git commit -m "feat(editor): run interactive QRhi window frame loop"
```

---

### Task 6: Update architecture reality and execute the full production-bootstrap gate

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture/005-immediate-mode-widget-state-machine.md`
- Modify: `docs/architecture/007-module-sequencing-policy.md`
- Create: `docs/architecture/008-windowed-qrhi-presentation-and-event-boundary.md`

**Interfaces:**
- Produces: accurate architecture and sequencing authority for the next text milestone.

- [ ] **Step 1: Record the new window/event/swapchain boundary**

Document ownership, lifetime order, recoverable frame results, and why native integer handles are prohibited.

- [ ] **Step 2: Correct widget and sequencing ADRs**

Replace the old first-widget-active behavior and stale claim that DrawList is disconnected. Set minimum text as the next milestone; Dock remains blocked.

- [ ] **Step 3: Run the full GCC gate**

```bash
cmake --build build/strict-gcc --parallel 2
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ctest --test-dir build/strict-gcc --output-on-failure
python3 cmake/module_dependency_lint.py --repo-root .
```

Expected: all targets and tests pass.

- [ ] **Step 4: Run the full Clang gate**

```bash
cmake --build build/strict-clang --parallel 2
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
  xvfb-run -a ctest --test-dir build/strict-clang --output-on-failure
```

Expected: all targets and tests pass.

- [ ] **Step 5: Commit**

```bash
git add README.md docs/architecture
git commit -m "docs: record production bootstrap architecture"
```

---

### Task 7: Tag and persist the completed phase bundle

**Files:**
- Create: `cmake/create_phase_bundle.sh`
- Create: `docs/releases/production-bootstrap.md`

**Interfaces:**
- Produces:

```text
phase/production-bootstrap-v1
UltraRenderStudio-production-bootstrap-v1.bundle
UltraRenderStudio-production-bootstrap-v1.bundle.sha256
```

- [ ] **Step 1: Implement bundle helper validation**

The script must reject a dirty worktree, missing tag, or failed `git bundle verify`. It creates the annotated tag, bundle, and SHA-256 sidecar under a caller-provided output directory.

- [ ] **Step 2: Test reconstruction into a fresh clone**

```bash
bash cmake/create_phase_bundle.sh \
  phase/production-bootstrap-v1 \
  /mnt/data/ultrarenderstudio-bundles
git clone /mnt/data/ultrarenderstudio-bundles/UltraRenderStudio-production-bootstrap-v1.bundle \
  /tmp/ur-bundle-restore
git -C /tmp/ur-bundle-restore fsck --full
git -C /tmp/ur-bundle-restore log --oneline --decorate -10
```

Expected: clone and `fsck` succeed; phase tag points to the verified completion commit.

- [ ] **Step 3: Persist the bundle vault copy**

Encode or upload the binary bundle and sidecar to the dedicated UltraRenderStudio bundle-vault branch in `valurius38027/toolchain`. Record the exact vault path and SHA-256 in `docs/releases/production-bootstrap.md`.

- [ ] **Step 4: Commit release metadata**

```bash
git add cmake/create_phase_bundle.sh docs/releases/production-bootstrap.md
git commit -m "release: add production bootstrap recovery bundle"
```

- [ ] **Step 5: Recreate the final bundle after metadata commit**

Delete the preliminary bundle and rerun the helper so the published bundle includes its own release metadata commit. Verify it again from a fresh clone.
