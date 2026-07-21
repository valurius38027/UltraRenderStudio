# Minimal Text Rendering Closure Design

- Status: approved and self-reviewed
- Date: 2026-07-21
- Target phase: `minimal-text-rendering-v1`
- Canonical branch: `main`
- Predecessor: `phase/production-bootstrap-v1.1`

## 1. Purpose

Close the first production-usable text path for UltraRenderStudio without introducing Qt text rendering, platform font discovery, font fallback, rich text, or Dock-specific layout policy.

The completed path is:

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

The milestone is complete only when the real editor button displays a shaped label and the strict `main` workflow publishes an immutable phase bundle.

## 2. Scope

### Included

- One explicitly loaded font face and pixel size per `FontId`.
- Arbitrary valid UTF-8 passed to HarfBuzz.
- HarfBuzz segment-property inference and single-run shaping.
- Single-line text only.
- FreeType grayscale glyph rasterization.
- Deterministic, monotonic, single-page glyph atlas.
- Text measurement and positioned-glyph output.
- Ordered rectangle and text commands in `ur_widgets::DrawList`.
- Generic alpha-atlas and masked-quad consumption in `ur_gfx`.
- Offscreen pixel tests and real editor presentation.
- GCC, Clang, ASan/UBSan, Xvfb/Lavapipe, dependency-lint, documentation, tag, bundle, and Release gates.

### Excluded

- Font fallback or system-font enumeration.
- Mixed-direction Unicode BiDi paragraph resolution.
- Line breaking, wrapping, multi-line layout, truncation, or ellipsis.
- Color fonts, emoji composition, SVG glyphs, or embedded bitmap strikes.
- Rich text, style spans, variable-font axes, or configurable OpenType features.
- MSDF/SDF generation.
- Atlas eviction, compaction, paging, repacking, or asynchronous upload.
- General high-DPI policy. The production gate remains device-pixel-ratio 1.0; scale-aware cache invalidation belongs to the next UI-foundation phase.
- Clipping/scissor, overlays, scoped widget IDs, and layout containers.

Newline, carriage return, and explicit Unicode line separators are rejected rather than silently ignored.

## 3. Deterministic SDK prerequisite

Before UltraRenderStudio implementation begins, `valurius38027/toolchain` must publish a new immutable UltraRender SDK release that:

1. includes the Debian package providing DejaVu Sans;
2. guarantees `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` exists after restore;
3. retains locked FreeType and HarfBuzz packages;
4. exposes and verifies a Qt shader-baking executable for checked-in QRhi shader packages;
5. runs an SDK smoke that loads DejaVu Sans with FreeType, creates an `hb_font_t`, shapes fixed UTF-8 text, and observes a non-zero glyph count and advance;
6. does not modify or overwrite `ultrarender-sdk-debian13-v2026.07.20.1`.

UltraRenderStudio references the deterministic font path through a named configuration constant. It does not vendor the font binary and does not use Fontconfig or platform font discovery in this phase.

## 4. Module ownership

### `ur_text`

`ur_text` owns FreeType and HarfBuzz lifetime, UTF-8 validation, shaping, measurement, rasterization, glyph caching, atlas allocation, atlas pixels, and atlas revision.

Its public headers expose no FreeType, HarfBuzz, Qt, QRhi, Vulkan, or platform-native types. Implementation handles are hidden behind Pimpl.

The target depends only on the C++ standard library, FreeType, and HarfBuzz. The existing unused `ur_platform` and `ur_gfx` target dependencies are removed. Graphics transport remains a separate translation boundary.

### `ur_widgets`

`ur_widgets` decides which labels to draw, computes label origin and color, centers labels from text metrics, and preserves draw order relative to rectangle commands. It does not shape, rasterize, allocate atlas space, upload textures, or use graphics-private types.

### `ur_gfx`

`ur_gfx` owns generic UI primitive transport, QRhi atlas texture creation and upload, revision tracking, masked-quad shaders, alpha blending, command-order-preserving rendering, offscreen readback, and window presentation. It does not know fonts, glyph IDs, Unicode, FreeType, or HarfBuzz.

### `ur_editor`

`ur_editor` loads the configured default font, owns the long-lived `TextSystem`, supplies the default font to widgets, translates ordered widget commands to `ur_gfx::UiFrame`, and reports glyph/atlas evidence in finite-frame smoke mode.

Font-load failure is a startup failure. The editor never degrades to an unlabeled button.

## 5. `ur_text` public model

The semantic API is:

```cpp
namespace ur::text {

using FontId = std::uint32_t;
inline constexpr FontId kInvalidFontId = 0U;

struct AtlasConfig {
    std::uint32_t width = 1024U;
    std::uint32_t height = 1024U;
    std::uint32_t padding = 1U;
};

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
    FontId font = kInvalidFontId;
    std::vector<PositionedGlyph> glyphs;
    TextMetrics metrics;
};

struct AtlasView {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
    std::span<const std::uint8_t> pixels;
    std::uint64_t revision = 0U;
};

class AtlasFullError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class TextSystem {
public:
    explicit TextSystem(AtlasConfig config = {});
    ~TextSystem();

    TextSystem(const TextSystem&) = delete;
    TextSystem& operator=(const TextSystem&) = delete;

    FontId loadFont(const FontDescriptor& descriptor);
    TextLayout shape(FontId font, std::string_view utf8);
    TextMetrics measure(FontId font, std::string_view utf8);
    TextLayout prepare(FontId font, std::string_view utf8);
    AtlasView atlas() const;
};

}  // namespace ur::text
```

`FontId` identifies one face at one fixed pixel size. Loading the same file at another size creates another `FontId` and cache namespace. Zero is always invalid.

`shape()` computes glyph order, positions, metrics, and bitmap bounds without atlas mutation. Its `atlasRect` fields are empty. `measure()` agrees with shaping advance and also does not mutate the atlas. `prepare()` performs cache lookup/rasterization and guarantees valid atlas rectangles for non-empty glyph bitmaps. Spaces and other zero-area glyphs retain empty atlas rectangles.

`AtlasView` borrows storage from `TextSystem` and is valid until the next non-const operation on that object. Callers do not retain its span across atlas mutation.

## 6. Input and error semantics

- Invalid `FontId`: `std::invalid_argument`.
- Zero atlas extent, zero font pixel size, malformed UTF-8, or line separator: `std::invalid_argument`.
- Missing, unreadable, or unsupported font: `std::runtime_error` with path context.
- Empty string: valid; zero glyphs and zero advance while preserving font line metrics.
- Missing codepoint: HarfBuzz glyph index 0 and `.notdef` rendering when supplied by the face.
- Atlas exhaustion: `AtlasFullError`; no overwrite or relocation.
- FreeType/HarfBuzz failure: contextual `std::runtime_error` while preserving object invariants.

A glyph insertion is atomic: successful allocation, bitmap copy, cache entry, then one revision increment. Failed insertion does not publish pixels, cache state, or revision.

## 7. Shaping policy

Each shaping call:

1. validates UTF-8 and single-line constraints;
2. creates or resets an `hb_buffer_t`;
3. adds the exact UTF-8 byte range;
4. calls `hb_buffer_guess_segment_properties()`;
5. calls `hb_shape()` with default features;
6. converts HarfBuzz 26.6 positions to floating-point pixel units;
7. preserves HarfBuzz glyph order;
8. computes logical advance and ink bounds from positioned glyph extents.

This supports Latin shaping, ligatures, kerning, combining marks, and one RTL run. It does not claim mixed-direction paragraph layout.

## 8. Rasterization and atlas policy

### Rasterization

- FreeType grayscale coverage only.
- Embedded bitmap strikes disabled.
- Hinting and auto-hinting disabled for deterministic v1 pixel evidence.
- Positive and negative bitmap pitch handled correctly.
- Horizontal bearing and baseline-relative vertical placement preserved.
- Zero-area glyphs remain measurable without atlas allocation.

### Atlas

```text
format: R8_UNORM
size: 1024 x 1024
padding: 1 pixel
allocator: deterministic shelf allocator
key: (FontId, glyphIndex)
revision: monotonic uint64, one increment per inserted glyph
repack: never
eviction: never
pages: one
```

Placement is deterministic for a given glyph-request sequence. Existing coordinates never change. A cached glyph request changes neither pixels nor revision.

## 9. Generic graphics model

`ur_gfx` adds a generic ordered UI-frame API without depending on `ur_text`:

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

New operations:

```cpp
FrameReadback renderUiFrame(const UiFrame&, Extent2D targetSize);
PresentResult presentUiFrame(const UiFrame&);
```

The call synchronously consumes `UiFrame` and borrowed atlas pixels. `RenderDevice` retains no span after return.

Existing `renderRects()` and `presentRects()` remain source-compatible wrappers through the generic frame path.

## 10. QRhi implementation

The Vulkan QRhi device adds:

- one device-owned R8 atlas texture;
- tracked atlas extent and uploaded revision;
- full upload on first use, extent change, revision change, or GPU-resource recreation;
- no upload when extent and revision match;
- checked-in `glyph.vert.qsb` and `glyph.frag.qsb`;
- a masked-quad pipeline;
- source-alpha blending;
- linear filtering with clamp-to-edge;
- command-order-preserving draws.

One-pixel padding prevents linear-filter bleed. The renderer may batch only adjacent primitives using the same pipeline and never globally reorders rectangles and text.

Swapchain recreation preserves CPU atlas ownership. The next frame recreates required GPU resources and uploads the current atlas before drawing masked quads.

## 11. Widget command model

`DrawList` becomes an ordered variant stream:

```cpp
struct TextCommand {
    ur::text::TextLayout layout;
    float originX = 0.0F;
    float baselineY = 0.0F;
    Color color;
};

using DrawCommand = std::variant<RectCommand, TextCommand>;
```

`Context` receives a non-owning `TextSystem&` and default `FontId`; both outlive the context.

Button submission order is background rectangle then prepared text. Hit testing continues to use only the caller-provided button rectangle. Text does not create another widget or hit region.

Horizontal centering uses `advanceWidth`. Vertical centering derives the baseline from ascender, descender, and line height, not content-dependent ink bounds. Existing two-stage topmost arbitration and one-frame click handoff remain unchanged.

## 12. Editor integration

`EditorApp` owns `TextSystem` before `Context` and loads:

```text
font: /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf
pixel size: 16
```

The existing `Primary action` button is the first real consumer. The editor converts ordered widget commands into ordered `UiPrimitive` values and attaches the current atlas view.

Finite-frame smoke output adds positive values for:

```text
text_glyphs=<n>
atlas_revision=<n>
```

Existing `attempted_frames`, `presented_frames`, and `button_toggled` fields remain.

## 13. Test requirements

### `ur_text`

Replace the placeholder test and cover:

- deterministic DejaVu Sans load;
- missing font and zero-size failures;
- empty string metrics;
- malformed UTF-8 and newline rejection;
- ASCII and non-ASCII shaping;
- ligature or kerning behavior using a known DejaVu Sans sequence;
- combining marks;
- one RTL run;
- missing character producing glyph 0;
- measure/shape consistency;
- `shape()` and `measure()` not changing atlas revision;
- first `prepare()` growing the atlas;
- repeated glyph reuse without revision change;
- one revision increment per new glyph;
- non-empty bitmap coverage;
- zero-area space behavior;
- deterministic shelf placement;
- explicit exhaustion with a small test atlas.

Tests use stable invariants or documented tolerances rather than undocumented FreeType allocation details.

### `ur_gfx`

Cover first atlas upload, unchanged-revision upload suppression through a test seam, masked-quad pixel placement, alpha/tint composition, rect/text/rect order, empty frame behavior, invalid atlas bounds, and resize followed by valid text presentation.

### `ur_widgets`

Cover background-then-text order, horizontal centering, vertical baseline, label color, existing input behavior, and overlapping-button draw/input Z-order consistency.

### Integration

At least one deterministic offscreen test executes:

```text
UTF-8 -> HarfBuzz -> FreeType -> atlas -> DrawList -> UiFrame -> Vulkan -> readback
```

It proves the expected label region contains foreground coverage distinct from the button background.

The Xvfb/Lavapipe editor smoke presents non-zero frames and reports positive glyph count and atlas revision under the strict GCC and Clang workflow.

## 14. Documentation and compatibility

Update:

- `README.md` with font prerequisite, text capability, verified commands, and exclusions;
- `INDEX.md` with `ur_text` completion and the next UI-foundation milestone;
- ADR-007 sequencing status;
- a new text ownership and atlas ADR;
- sanitizer ADRs only when a new narrow suppression is independently proven necessary.

Existing phase tags and Releases remain immutable. Rectangle APIs remain source-compatible during this phase.

## 15. Delivery checkpoints

The implementation plan separates:

1. deterministic SDK font and shader-tool prerequisite;
2. shaping and measurement;
3. rasterization and atlas;
4. generic `ur_gfx` UI-frame model;
5. QRhi masked-quad rendering;
6. ordered widget text commands and labels;
7. editor integration and real-window smoke;
8. documentation, strict validation, phase version, tag, bundle, Release, and remote checksum round-trip.

Each checkpoint uses test-first development. The intended test failure is observed before production implementation, and relevant gates pass before the next checkpoint.

## 16. Completion criteria

Set:

```text
PHASE_VERSION=minimal-text-rendering-v1
```

The phase is complete only when:

1. the new deterministic SDK release publishes and restores successfully;
2. GCC Debug with ASan/UBSan passes;
3. Clang RelWithDebInfo passes;
4. all CTest targets pass;
5. dependency-layer lint passes;
6. offscreen text pixel evidence passes;
7. Xvfb/Lavapipe real-window text presentation passes;
8. README, INDEX, and ADR claims match code reality;
9. annotated tag `phase/minimal-text-rendering-v1` identifies the validated commit;
10. the complete Git bundle, SHA-256 sidecar, and verification report are published in an immutable GitHub Release;
11. the Release assets are downloaded again and the sidecar verifies successfully.

After completion, the next milestone is Dock-precondition UI Foundation: scoped IDs, clipping/scissor, overlay ordering, and basic layout consumption. Dock implementation remains prohibited until that milestone closes.
