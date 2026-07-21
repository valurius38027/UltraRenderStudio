# ADR-009: Qt Generic Unix Services LSan process boundary

- Status: accepted
- Date: 2026-07-21

## Context

On Debian 13 GCC/ASan, all `ur_gfx_tests` behavior passed but process exit reported 344 bytes rooted at `QGenericUnixServices::QGenericUnixServices()`. No UltraRenderStudio frame appeared in the allocation chain.

A same-runner four-way probe produced:

```text
qgui_only=1
qgui_vulkan=1
ur_gfx_without_suppression=1
ur_gfx_targeted_suppression=0
```

A program containing only `QGuiApplication` reproduced the same allocation; adding `QVulkanInstance` did not change it.

## Decision

- Match only `QGenericUnixServices::QGenericUnixServices` in GUI-process LSan suppression files.
- Apply the rule only to targets that instantiate `QGuiApplication`.
- Keep real Vulkan window processes on the separate Qt + Mesa + Vulkan-loader suppression file.
- Do not use `detect_leaks=0`, byte thresholds, or broad `Qt`/`QObject` patterns.
- Re-run the minimal probe after Qt upgrades and delete the rule when no longer reproducible.

## Covered targets

- `ur_platform_tests`: Qt GUI boundary;
- `ur_gfx_tests`, `ur_widgets_render_tests`, `ur_integration_tests`: Qt GUI + Mesa boundary;
- `ur_window_present_tests`, `ur_editor_smoke`: Qt GUI + Mesa + Vulkan-loader boundary.

Pure logic tests receive no suppression.
