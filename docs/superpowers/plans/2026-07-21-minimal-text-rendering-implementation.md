# Minimal Text Rendering Closure Implementation Plan

**Target:** `minimal-text-rendering-v1`  
**Branch policy:** `main` only  
**Design authority:** `docs/superpowers/specs/2026-07-21-minimal-text-rendering-design.md`

## Execution model

This plan is intentionally high-signal. It defines ownership, dependencies, failing behavior boundaries, coherent commits, and gates without tutorial-style micro-steps. Approved implementation proceeds directly on `main`; local isolation is optional.

## Dependency sequence

1. **Deterministic SDK prerequisite**
   - Update `valurius38027/toolchain/profiles/ultrarender`.
   - Lock `fonts-dejavu-core` and `qt6-shader-baker`.
   - Verify DejaVu Sans loading, HarfBuzz shaping, and `.qsb` generation in the clean offline restore container.
   - Publish an immutable SDK release before the final source CI gate.

2. **`ur_text`: shaping and measurement**
   - Replace the placeholder API and test.
   - Public Pimpl-based `TextSystem`; no Qt, QRhi, Vulkan, `ur_platform`, or `ur_gfx` dependency.
   - Validate UTF-8 and single-line constraints.
   - Shape with HarfBuzz and derive metrics from FreeType at a fixed pixel size.
   - Failing boundaries: invalid font/input, malformed UTF-8, line separators, `.notdef`, RTL/combining runs, measure/shape consistency, atlas immutability during measurement.

3. **`ur_text`: rasterization and atlas**
   - FreeType grayscale coverage with bitmap/hinting disabled according to the design.
   - Deterministic one-page shelf allocation, monotonic revision, no movement or eviction.
   - Failing boundaries: revision semantics, cache reuse, zero-area glyphs, deterministic placement, invalid pitch/extent, and atomic atlas exhaustion.

4. **`ur_gfx`: generic ordered UI frame**
   - Add `AlphaAtlasView`, `MaskedQuadPrimitive`, ordered `UiPrimitive`, and `UiFrame`.
   - Retain rectangle APIs as wrappers.
   - Validate borrowed atlas extent/pixel count and masked source bounds synchronously.
   - Failing boundaries: rectangle compatibility, empty frame, missing/malformed atlas, invalid quad bounds.

5. **QRhi masked-quad rendering**
   - Add deterministic glyph shaders produced by the locked Qt shader baker.
   - R8 texture, linear clamp sampler, source-alpha blending.
   - Preserve command order through adjacent-type batches; never globally reorder rects and glyphs.
   - Window resources upload atlas only on extent/revision change or GPU-resource recreation.
   - Failing boundaries: opaque/transparent mask pixels, tint/alpha, rect-text-rect overlap, upload count, resize/present recovery.

6. **`ur_widgets`: label commands**
   - Convert `DrawList` to an ordered rect/text variant.
   - `Context` receives a non-owning `TextSystem` and default `FontId`.
   - Prepare labels, center by advance and line metrics, preserve background-then-text order.
   - Expand prepared glyphs into generic masked quads at the widgets-to-gfx translation boundary.
   - Failing boundaries: command order, baseline/centering, input-arbitration regression, overlapping widget Z order.

7. **Editor and end-to-end evidence**
   - Editor owns `TextSystem` before `Context` and loads the deterministic font path.
   - Present `UiFrame`, not rectangle-only vectors.
   - Finite-frame smoke reports positive `text_glyphs` and `atlas_revision`.
   - Offscreen integration proves `UTF-8 -> shaping -> raster -> atlas -> DrawList -> UiFrame -> Vulkan -> pixels`.
   - Xvfb/Lavapipe proves real window text presentation and resize recovery.

8. **Documentation and phase publication**
   - Add text ownership/atlas ADR and update ADR-007, README, INDEX, SDK baseline, and exclusions.
   - Set `PHASE_VERSION=minimal-text-rendering-v1` only after all source behavior is closed.
   - Run strict GCC ASan/UBSan, Clang, all CTest, dependency lint, offscreen pixel, and real-window smoke gates.
   - Publish immutable annotated tag, complete Git bundle, SHA-256 sidecar, verification report, fresh-clone `git fsck`, and remote download checksum round-trip.

## Coherent commit checkpoints

- SDK font/shader prerequisite.
- `ur_text` shaping/measurement/atlas.
- generic UI-frame contract.
- QRhi masked-quad renderer.
- widget text commands and translator.
- editor/end-to-end integration.
- documentation, phase version, validation, and release.

A checkpoint may combine tightly coupled files. It must not claim completion until its observable tests pass. No existing phase tag or Release may be moved or overwritten.
