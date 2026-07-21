# UltraRenderStudio Code Reality Index

## Repository authority

- Canonical repository: `valurius38027/UltraRenderStudio`
- Maintained branch: `main` only
- Latest completed phase candidate: `minimal-text-rendering-v1` (complete locally; authoritative only after main CI and Release)
- Supported development environment: Debian 13 amd64
- Deterministic SDK: `ultrarender-sdk-debian13-v2026.07.21.1`
- Toolchain authority: `valurius38027/toolchain`

## Completed implementation foundation

- Qt-to-platform FIFO events for expose, resize, pointer, focus, and close;
- private `QWindow` bridge without public native handles;
- Vulkan QRhi offscreen rendering, deterministic readback, real swapchain/present and resize recovery;
- two-stage topmost widget hit arbitration and stale capture cleanup;
- deterministic DejaVu Sans loading, UTF-8 HarfBuzz shaping and FreeType grayscale rasterization;
- deterministic single-page R8 glyph atlas with monotonic revision;
- ordered rectangle/text `UiFrame` rendering with alpha-masked glyph quads;
- real editor button labels and finite-frame glyph/atlas evidence;
- GCC/Clang, ASan/UBSan, QShader reproduction, pixel-chain and Xvfb/Lavapipe gates.

## Module state

| Module | State | Current responsibility |
| --- | --- | --- |
| `ur_platform` | Bootstrap complete | Window ownership and backend-neutral event FIFO |
| `ur_gfx` | Text-capable Vulkan foundation | Ordered UiFrame, R8 atlas, offscreen and swapchain presentation |
| `ur_text` | Minimal text closure complete | Font lifetime, UTF-8 shaping, metrics, rasterization and CPU atlas |
| `ur_widgets` | Text-capable minimal UI | Ordered draw commands, button input, label placement and UiFrame translation |
| `ur_dock` | Placeholder | Blocked by Dock-precondition UI Foundation |
| `ur_nodegraph` | Placeholder | Not scheduled before Dock foundation |
| `ur_viewport` | Placeholder | Not scheduled before Dock foundation |
| `ur_scene_bridge` | Thin wrapper | C ABI session lifetime only; real engine validation pending |
| `ur_editor` | Text-capable bootstrap | Event loop, widget frame, text rendering and presentation |

## Next formal milestone

**Dock-precondition UI Foundation**:

1. scoped hierarchical widget IDs;
2. clip stack and QRhi dynamic scissor propagation;
3. explicit overlay layer ordering without global primitive reordering;
4. minimal row/column layout consumption;
5. device-pixel-ratio propagation and scale-aware invalidation;
6. nested-container and overlap behavioral tests;
7. offscreen pixel and real-window evidence.

Dock implementation remains prohibited until these foundations are closed.

## Known technical debt

- Only Vulkan is implemented; D3D11, D3D12, and Metal are reservations, not supported backends.
- Font fallback, mixed-direction paragraphs, multiline layout, color fonts and atlas paging are not implemented.
- `vcpkg.json` and `vcpkg-configuration.json` are not the production dependency path.
- Windows Ninja presets have not passed a production cross-platform gate.
- The vendored UltraRender C ABI header lacks an upstream commit/tag and capability negotiation.
- Placeholder modules retain compile-wiring tests until implementation begins.
- Narrow Qt/Mesa/Vulkan process-lifetime LSan boundaries remain documented in ADR-006/008/009.

## Authoritative documents

1. `AGENTS.md` — repository operating constraints.
2. `INDEX.md` — current code reality and next milestone.
3. `docs/operations/REPOSITORY_OPERATIONS.md` — recovery, remote mutation, CI diagnosis and release procedures.
4. `docs/architecture/*.md` — architectural decisions and sequencing.
5. `docs/superpowers/specs/` — approved feature designs.
6. `docs/superpowers/plans/` — implementation plans.
7. `README.md` — developer entry and verified commands.
