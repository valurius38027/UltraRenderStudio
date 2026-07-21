# Minimal Text Rendering Closure Design

- Status: approved
- Date: 2026-07-21
- Target phase: `minimal-text-rendering-v1`
- Canonical branch: `main`
- Predecessor: `phase/production-bootstrap-v1.1`

## 1. Purpose

Close the first production-usable text path for UltraRenderStudio without introducing Qt text rendering, platform font discovery, font fallback, rich text, or Dock-specific layout policy.

The completed path must be:

```text
UTF-8 label
  -> HarfBuzz shaping
  -> FreeType rasterization
  -> deterministic glyph atlas
  -> ordered widget draw command
  -> generic ur_gfx UI frame
  -> QRhi/Vulkan sampling and blending
  -> offscreen pixel evidence and real-window presentation
```

The milestone is complete only when the real editor button displays a shaped label and the full strict `main` workflow publishes an immutable phase bundle.

## 2. Scope

### 2.1 Included

- One explicitly loaded font face per `FontId`.
- Arbitrary valid UTF-8 passed to HarfBuzz.
- HarfBuzz segment-property inference.
- Single-line shaping.
- FreeType grayscale glyph rasterization.
- Deterministic, monotonic single-page glyph atlas.
- Text measurement and positioned-glyph output.
- Ordered rectangle and text commands in `ur_widgets::DrawList`.
- Generic alpha-atlas and masked-quad consumption in `ur_gfx`.
- Offscreen text pixel tests and real editor presentation.
- GCC, Clang, ASan/UBSan, Xvfb/Lavapipe, dependency-lint, documentation, tag, bundle, and Release gates.

### 2.2 Excluded

- Font fallback or system-font enumeration.
- Unicode BiDi paragraph resolution across multiple directional runs.
- Line breaking, wrapping, multi-line layout, truncation, or ellipsis.
- Color fonts, emoji composition, SVG glyphs, or embedded bitmap strikes.
- Rich text, style spans, variable-font axis controls, or user-configurable OpenType features.
- MSDF/SDF generation.
- Atlas eviction, compaction, paging, or asynchronous upload.
- General high-DPI policy. The production gate remains device-pixel-ratio 1.0; later UI-foundation work must define scale-aware cache invalidation.
- Clipping/scissor, overlays, scoped widget IDs, and layout containers. Those remain the next milestone.

A newline, carriage return, or other explicit line-separator request is rejected by the v1 single-line API rather than silently ignored.

## 3. Deterministic SDK prerequisite

The authoritative SDK must be updated before UltraRenderStudio text implementation begins.

The `valurius38027/toolchain` UltraRender profile must:

1. include the Debian package that provides DejaVu Sans;
2. guarantee `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` exists after restore;
3. keep FreeType and HarfBuzz in the locked package closure;
4. expose a verified Qt shader-baking tool for checked-in QRhi shader packages;
5. compile and run an SDK smoke that loads the font with FreeType, creates an `hb_font_t`, shapes fixed UTF-8 text, and observes non-zero glyph count and advance;
6. publish a new immutable SDK version rather than modifying `ultrarender-sdk-debian13-v2026.07.20.1`.

UltraRenderStudio must reference the deterministic default-font path through a named configuration constant. It must not copy the font binary into the source repository and must not use Fontconfig or platform discovery in this phase.

## 4. Module ownership

### 4.1 `ur_text`

`ur_text` owns:

- FreeType library lifetime;
- loaded face lifetime;
- HarfBuzz font lifetime;
- UTF-8 validation and single-line validation;
- shaping;
- text metrics;
- glyph rasterization;
- glyph-cache lookup;
- atlas allocation and pixel storage;
- atlas revision;
- prepared text data.

Its public headers must not expose FreeType or HarfBuzz types. Implementation handles are hidden behind Pimpl.

### 4.2 `ur_widgets`

`ur_widgets` owns:

- deciding which labels to draw;
- text origin and color;
- button-label centering from `ur_text` metrics;
- maintaining draw-command order relative to rectangles;
- widget input semantics.

It does not shape, rasterize, allocate atlas space, upload textures, or use QRhi types.

### 4.3 `ur_gfx`

`ur_gfx` owns:

- generic UI primitive transport;
- QRhi atlas texture creation and update;
- revision-based upload suppression;
- masked-quad shaders and pipeline;
- alpha blending;
- command-order-preserving rendering;
- offscreen readback and window presentation.

It does not know glyph IDs, Unicode, fonts, HarfBuzz, or FreeType.

### 4.4 `ur_editor`

`ur_editor` owns:

- loading the configured default font at startup;
- holding the long-lived `TextSystem`;
- passing the text service and default font to widgets;
- translating ordered widget commands into `ur_gfx::UiFrame`;
- reporting glyph and atlas evidence in finite-frame smoke mode.

Font-load failure is a startup error. The editor must not silently fall back to an unlabeled button.

## 5. `ur_text` public model

The public interface will follow this semantic model; exact names may be adjusted only in the implementation plan if the meaning and ownership remain unchanged.

```cpp
namespace ur::text {

using FontId = std::uint32_t;

struct FontDescriptor {
    std::filesystem::path filePath;
    std::uint32_t pixelSize = 16U;
};

struct TextBounds {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

struct TextMetrics {
    float advanceWidth = 0.0F;
    float ascender = 0.0F;
    float descender = 0.0F;
    float lineHeight = 0.0F;
    TextBounds inkBounds;
};

struct AtlasRect {
    std::uint32_t x = 0U;
    std::uint32_t y = 0U;
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
};

struct PositionedGlyph {
    std::uint32_t glyphIndex = 0U;
    float originX = 0.0F;
    float originY = 0.0F;
    float advanceX = 0.0F;
    float advanceY = 0.0F;
    TextBounds bitmapBounds;
    AtlasRect atlasRect;
};

struct TextLayout {
    FontId font = 0U;
    std::vector<PositionedGlyph> glyphs;
    TextMetrics metrics;
};

struct AtlasView {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::span<const std::uint8_t> pixels;
    std::uint64_t revision = 0U;
};

class TextSystem {
public:
    explicit TextSystem(AtlasConfig config = {});
    ~TextSystem();

    FontId loadFont(const FontDescriptor& descriptor);
    TextLayout shape(FontId font, std::string_view utf8);
    TextMetrics measure(FontId font, std::string_view utf8);
    TextLayout prepare(FontId font, std::string_view utf8);
    AtlasView atlas() const;
};

}  // namespace ur::text
```

`FontId` identifies a face at one fixed pixel size. Loading the same file at a different pixel size creates a different `FontId` and therefore a separate glyph-cache namespace.

`shape()` computes positioned glyphs and metrics but does not require rasterization. `prepare()` guarantees every returned glyph has a valid atlas allocation, except zero-area glyphs such as spaces, which retain an empty atlas rectangle. `measure()` must agree with the shaping advance and must not force atlas growth.

Returned `AtlasView` storage is borrowed from `TextSystem` and remains valid until the next non-const operation on that same object. Callers must not retain the span across atlas mutation.

## 6. Input and error semantics

The v1 API uses explicit exceptions derived from `std::runtime_error` for startup/configuration failures and `std::invalid_argument` for invalid caller input.

Required behavior:

- invalid `FontId`: reject;
- missing, unreadable, or unsupported font file: reject with path context;
- zero pixel size: reject;
- malformed UTF-8: reject;
- explicit line break or separator: reject;
- empty UTF-8 string: valid, zero glyphs, zero advance, font line metrics preserved;
- missing character in the loaded font: HarfBuzz glyph index 0, rendered as `.notdef` when the face provides it;
- atlas exhaustion: throw a dedicated `AtlasFullError`; never overwrite or move existing glyphs;
- FreeType/HarfBuzz failure: include operation context and preserve module invariants.

No operation may partially publish an atlas revision. A glyph is inserted atomically: allocation, bitmap copy, cache entry, then revision increment.

## 7. Shaping policy

For each call:

1. validate UTF-8 and single-line constraints;
2. create or reset an `hb_buffer_t`;
3. add UTF-8 bytes with exact length;
4. call `hb_buffer_guess_segment_properties()`;
5. call `hb_shape()` with default OpenType features;
6. convert HarfBuzz 26.6 positions to `float` pixel units;
7. preserve glyph order returned by HarfBuzz;
8. compute logical advance and ink bounds from positioned glyph extents.

This supports single-run Latin shaping, ligatures, kerning, combining marks, and a single RTL run. The API does not claim full mixed-direction paragraph layout.

## 8. Rasterization and atlas policy

### 8.1 Rasterization

- FreeType grayscale coverage only.
- Disable embedded bitmap strikes.
- Disable hinting and auto-hinting for the deterministic v1 pixel gate.
- Convert positive and negative bitmap pitch correctly.
- Preserve horizontal bearing and baseline-relative vertical placement.
- Spaces and other zero-area glyphs remain measurable without atlas allocation.

### 8.2 Atlas

Default configuration:

```text
format: R8_UNORM
size: 1024 x 1024
padding: 1 pixel around each non-empty glyph
allocator: deterministic shelf allocator
key: (FontId, glyphIndex)
revision: monotonic uint64
repack: never
eviction: never
pages: one
```

Shelf placement is deterministic for a given glyph-request sequence. Existing glyph coordinates never change. Re-requesting a cached glyph does not change pixels or revision.

## 9. Generic graphics model

`ur_gfx` will add a generic ordered UI-frame API without depending on `ur_text`.

```cpp
struct AlphaAtlasView {
    Extent2D size;
    std::span<const std::uint8_t> pixels;
    std::uint64_t revision = 0U;
};

struct MaskedQuadPrimitive {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
    std::uint32_t atlasX = 0U;
    std::uint32_t atlasY = 0U;
    std::uint32_t atlasWidth = 0U;
    std::uint32_t atlasHeight = 0U;
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

using UiPrimitive = std::variant<RectPrimitive, MaskedQuadPrimitive>;

struct UiFrame {
    std::vector<UiPrimitive> primitives;
    std::optional<AlphaAtlasView> alphaAtlas;
};
```

New device operations:

```cpp
FrameReadback renderUiFrame(const UiFrame&, Extent2D targetSize);
PresentResult presentUiFrame(const UiFrame&);
```

`UiFrame` and atlas spans are synchronously consumed. `RenderDevice` must not retain borrowed spans after the call returns.

Existing `renderRects()` and `presentRects()` remain as compatibility wrappers implemented through the generic frame path until a later cleanup phase.

## 10. QRhi implementation

The Vulkan QRhi device will add:

- one R8 atlas texture owned by the device;
- tracked uploaded atlas extent and revision;
- complete upload on first use, extent change, or revision change;
- no upload when extent and revision match;
- checked-in `glyph.vert.qsb` and `glyph.frag.qsb` resources;
- a masked-quad graphics pipeline;
- standard source-alpha blending;
- nearest or linear filtering selected once and covered by pixel tests;
- command-order-preserving draws.

The selected v1 sampler is linear filtering with clamp-to-edge. One-pixel atlas padding prevents neighboring-glyph bleed.

The renderer may batch only adjacent primitives that use the same pipeline. It must not globally reorder rectangles and text, because submission order is the Z order.

Swapchain recreation must preserve CPU atlas ownership and recreate GPU texture/pipeline resources as required. The next frame must upload the current atlas snapshot before drawing masked quads.

## 11. Widget command model

`DrawList` changes from a rectangle-only vector to an ordered variant stream:

```cpp
struct TextCommand {
    ur::text::TextLayout layout;
    float originX = 0.0F;
    float baselineY = 0.0F;
    Color color;
};

using DrawCommand = std::variant<RectCommand, TextCommand>;
```

`Context` receives a non-owning `TextSystem` reference and default `FontId`. The referenced objects must outlive the context.

Button submission order is:

1. background rectangle;
2. prepared label text.

Button hit testing continues to use the caller-provided button rectangle only. Text does not create an independent widget or hit region.

Horizontal centering uses `advanceWidth`. Vertical centering uses font ascender, descender, and line height to derive a baseline within the button rectangle. Ink bounds are not used as the primary alignment box because different glyph contents would otherwise shift labels vertically.

The existing two-stage topmost input arbitration and one-frame click handoff remain unchanged.

## 12. Editor integration

`EditorApp` will own `TextSystem` before `Context` and load:

```text
/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
pixel size: 16
```

Construction order must guarantee the text system and loaded font outlive widgets and every draw command consumed in a frame.

The existing `Primary action` button becomes the first real consumer. The translator converts each ordered widget command into one or more ordered `ur_gfx` primitives and attaches the current atlas view.

Finite-frame smoke output adds:

```text
text_glyphs=<positive integer>
atlas_revision=<positive integer>
```

The current `attempted_frames`, `presented_frames`, and `button_toggled` fields remain.

## 13. Tests

### 13.1 `ur_text`

Behavioral tests must replace the placeholder test and cover:

- deterministic DejaVu Sans load;
- missing font failure;
- zero pixel-size failure;
- empty string metrics;
- malformed UTF-8 rejection;
- newline rejection;
- ASCII shaping;
- non-ASCII UTF-8 shaping;
- ligature or kerning behavior with a string known to exercise the font;
- combining-mark positioning;
- one RTL run;
- missing character producing glyph 0;
- measure/shape advance consistency;
- `measure()` not changing atlas revision;
- first `prepare()` growing the atlas;
- repeated glyph reuse without revision change;
- new glyph increasing revision exactly once per inserted glyph;
- non-empty bitmap coverage;
- zero-area space glyph behavior;
- deterministic shelf placement;
- explicit atlas exhaustion using a small test atlas.

Tests must not assert undocumented FreeType internal allocation patterns. Pixel and metric assertions use tolerances or stable invariants where exact values are not guaranteed by the public libraries.

### 13.2 `ur_gfx`

- first atlas upload;
- unchanged revision avoiding upload, exposed through a narrow test statistic or seam rather than timing;
- masked quad appears at the expected pixel location;
- alpha and tint composition;
- ordered rect/text/rect overlap;
- empty UI frame;
- invalid atlas bounds rejection;
- swapchain resize followed by valid text presentation.

### 13.3 `ur_widgets`

- button produces background then text command;
- horizontal centering from advance;
- vertical baseline from line metrics;
- label color;
- existing hover/active/click tests remain green;
- overlapping-button command order remains consistent with input Z order.

### 13.4 Integration

At least one deterministic offscreen test must execute:

```text
UTF-8 -> HarfBuzz -> FreeType -> atlas -> DrawList -> UiFrame -> Vulkan -> readback
```

It must prove that the expected label region contains foreground coverage distinct from the button background.

The real editor Xvfb/Lavapipe smoke must present non-zero frames and report positive glyph count and atlas revision under GCC and Clang validation.

## 14. Documentation and compatibility

The implementation must update:

- `README.md` with font prerequisite, text capability, build/test commands, and honest exclusions;
- `INDEX.md` with `ur_text` completion state and the next UI-foundation milestone;
- ADR-007 sequencing status;
- a new text ownership/atlas ADR;
- any sanitizer ADR only if a new narrow suppression is proven necessary.

No existing phase tag or Release may move. The current rectangle APIs remain source-compatible during this phase.

## 15. Delivery sequence

The implementation plan must separate at least these checkpoints:

1. deterministic SDK font and shader-tool prerequisite;
2. `ur_text` shaping and measurement;
3. glyph rasterization and atlas;
4. generic `ur_gfx` UI-frame model;
5. QRhi masked-quad rendering;
6. ordered widget text commands and button labels;
7. editor integration and real-window smoke;
8. documentation, strict validation, phase version, tag, bundle, Release, and remote checksum round-trip.

Each checkpoint uses test-first development and lands on `main` only after its intended failing test has been observed and the relevant local gates pass.

## 16. Completion criteria

Set:

```text
PHASE_VERSION=minimal-text-rendering-v1
```

The phase is complete only when:

1. the new deterministic SDK release is published and restores successfully;
2. GCC Debug with ASan/UBSan passes;
3. Clang RelWithDebInfo passes;
4. every CTest target passes;
5. the dependency-layer lint passes;
6. offscreen text pixel evidence passes;
7. Xvfb/Lavapipe real-window text presentation passes;
8. README, INDEX, and ADR claims match code reality;
9. annotated tag `phase/minimal-text-rendering-v1` identifies the validated commit;
10. the complete Git bundle, SHA-256 sidecar, and verification report are published in an immutable GitHub Release;
11. the Release assets are downloaded again and the sidecar verifies successfully.

After completion, the next formal milestone is Dock-precondition UI Foundation: scoped IDs, clipping/scissor, overlay ordering, and basic layout consumption. Dock implementation remains prohibited until that milestone closes.
