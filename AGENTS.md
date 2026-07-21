# UltraRenderStudio Repository Authority

This file is the highest-authority repository instruction for maintainers and coding agents.

## Branch policy

- `main` is the only maintained remote branch.
- Do not create or retain remote feature, development, release, or bundle-vault branches.
- Temporary local branches and worktrees are allowed for isolation, but completed work must be integrated into `main` and the temporary branch removed.
- Durable checkpoints use annotated `phase/*` tags and GitHub Release assets, not long-lived branches.

## Source and environment authority

- The canonical source repository is `valurius38027/UltraRenderStudio`.
- The canonical branch is `main`.
- The supported production-development host is Debian 13 (`trixie`) amd64.
- The canonical SDK recovery source is `valurius38027/toolchain` and its immutable UltraRender profile releases.
- The current deterministic SDK baseline is `ultrarender-sdk-debian13-v2026.07.20.1`.
- Restore the SDK with:

```bash
sudo bash profiles/ultrarender/scripts/restore.sh latest
```

Do not replace the SDK workflow with ad-hoc package installation unless repairing the toolchain profile itself.

## Engineering gates

Every source change must preserve:

1. C++20;
2. `UR_WARNINGS_AS_ERRORS=ON`;
3. GCC Debug with ASan/UBSan;
4. Clang RelWithDebInfo strict build;
5. dependency-layer lint;
6. all CTest targets;
7. Xvfb/Lavapipe coverage for real window or GPU behavior;
8. accurate README, ADR, and `INDEX.md` claims.

Do not disable warnings, sanitizers, tests, leak detection, or runtime assertions to obtain a passing gate. Any suppression must be narrowly scoped, reproducible, and documented in an ADR.

## Architecture boundaries

The dependency direction is:

```text
ur_platform
  -> ur_gfx
  -> ur_text
  -> ur_widgets
  -> ur_dock / ur_nodegraph / ur_viewport
```

`ur_scene_bridge` is a separate thin C ABI boundary. Preserve the dependency rules in `cmake/module_dependency_lint.py`.

Public `ur_platform` headers must not expose Qt types or platform-native handles. `ur_gfx` owns graphics backend integration. Higher layers must not directly depend on QRhi/Vulkan internals.

## Implementation discipline

- Use test-first development for behavior changes and bug fixes.
- A test must fail for the intended reason before production code is added.
- Placeholder tests only prove target wiring; they must not be cited as feature completion.
- Do not begin Dock while the minimum text and UI-foundation milestones remain incomplete.
- Modify an ADR when changing ownership, lifetime, input semantics, module sequencing, or cross-platform policy.
- Keep files focused and interfaces explicit; avoid unrelated refactoring during milestone work.

## Phase completion and persistence

`PHASE_VERSION` names the latest completed phase without the `phase/` prefix.

A phase is complete only after:

1. the full strict validation workflow passes on `main`;
2. the source reality and ADRs are updated;
3. an annotated `phase/<PHASE_VERSION>` tag identifies the validated commit;
4. a complete Git bundle and SHA-256 sidecar are generated;
5. the bundle passes `git bundle verify`;
6. a fresh clone from the bundle passes `git fsck --full`;
7. the immutable bundle, sidecar, and verification report are stored in a GitHub Release.

Existing phase tags and Release assets are immutable. Never move, overwrite, or silently recreate them.
