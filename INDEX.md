# UltraRenderStudio Code Reality Index

## Repository authority

- Canonical repository: `valurius38027/UltraRenderStudio`
- Maintained branch: `main` only
- Latest completed phase: `phase/production-bootstrap-v1.1`
- Supported development environment: Debian 13 amd64
- Toolchain authority: `valurius38027/toolchain`

## Current completed foundation

Production Bootstrap provides:

- strict GCC and Clang build closure;
- Qt-to-platform FIFO events for expose, resize, pointer, focus, and close;
- a private `QWindow` bridge without public native handles;
- Vulkan QRhi offscreen rendering and deterministic pixel readback;
- real `QWindow -> QVulkanInstance -> QRhi -> swapchain -> present` rendering;
- swapchain resize recovery under Xvfb/Lavapipe;
- two-stage topmost widget hit arbitration;
- stale active/capture cleanup and focus-loss cancellation;
- a real editor frame loop with finite-frame smoke mode.

## Module state

| Module | State | Current responsibility |
| --- | --- | --- |
| `ur_platform` | Bootstrap complete | Window ownership and backend-neutral event FIFO |
| `ur_gfx` | Vulkan bootstrap complete | Offscreen QRhi rendering and window swapchain presentation |
| `ur_text` | Placeholder | Next milestone: font, shaping, measurement, atlas, text draw data |
| `ur_widgets` | Minimal bootstrap | DrawList, rectangle button, topmost pointer arbitration |
| `ur_dock` | Placeholder | Blocked by text and UI-foundation milestones |
| `ur_nodegraph` | Placeholder | Not scheduled before Dock foundation |
| `ur_viewport` | Placeholder | Not scheduled before Dock foundation |
| `ur_scene_bridge` | Thin wrapper | C ABI session lifetime only; real engine validation pending |
| `ur_editor` | Bootstrap complete | Platform drain, widget frame, primitive translation, presentation |

## Next formal milestone

**Minimal Text Rendering Closure**:

1. deterministic font loading;
2. UTF-8 shaping through HarfBuzz;
3. glyph rasterization through FreeType;
4. glyph atlas ownership and update policy;
5. text measurement API;
6. DrawText data consumed by the graphics path;
7. visible button labels in the real editor window;
8. unit, layout, and pixel/integration evidence under both compilers.

Dock work remains prohibited until text measurement, scoped widget IDs, clipping/scissor, overlay ordering, and basic layout consumption are closed.

## Known technical debt

- Only Vulkan is implemented; D3D11, D3D12, and Metal are API reservations, not supported backends.
- `vcpkg.json` and `vcpkg-configuration.json` still contain date-like invalid baselines and are not the production dependency path.
- Windows Ninja presets have not passed a production cross-platform gate.
- The vendored UltraRender C ABI header lacks a recorded upstream commit/tag and capability negotiation.
- Placeholder modules still have compile-wiring tests that must be replaced when implementation begins.
- The real Vulkan window process has one narrowly documented loader-level LSan suppression; offscreen and other targets retain normal leak checking.

## Authoritative documents

1. `AGENTS.md` — repository operating constraints.
2. `INDEX.md` — current code reality and next milestone.
3. `docs/architecture/*.md` — architectural decisions and sequencing.
4. `docs/superpowers/specs/` — approved feature designs.
5. `docs/superpowers/plans/` — implementation plans.
6. `README.md` — developer entry and verified commands.
