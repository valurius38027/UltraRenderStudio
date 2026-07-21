# UltraRenderStudio Repository Authority

This file is the highest-authority repository instruction for maintainers and coding agents.

## Branch policy

- `main` is the only maintained remote branch.
- Do not create or retain remote feature, development, release, or bundle-vault branches.
- Temporary local branches and worktrees are optional implementation tools, not mandatory ceremony. Completed work must reach `main` and temporary refs must be removed.
- Durable checkpoints use annotated `phase/*` tags and GitHub Release assets, not long-lived branches.

## Source and environment authority

- Canonical repository: `valurius38027/UltraRenderStudio`.
- Canonical branch: `main`.
- Supported production-development host: Debian 13 (`trixie`) amd64.
- Canonical SDK source: `valurius38027/toolchain` immutable UltraRender profile releases.
- Current deterministic SDK baseline: `ultrarender-sdk-debian13-v2026.07.21.1`.
- Restore with `sudo bash profiles/ultrarender/scripts/restore.sh latest` from the toolchain repository.

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
8. deterministic checked-in QShader reproduction where applicable;
9. accurate README, ADR, spec, plan, `INDEX.md`, and operations claims.

Do not disable warnings, sanitizers, tests, leak detection, or runtime assertions to obtain a passing gate. Any suppression must be narrowly scoped, independently reproducible, and documented in an ADR.

## Architecture boundaries

The architectural order is:

```text
ur_platform
  -> ur_gfx
  -> ur_text
  -> ur_widgets
  -> ur_dock / ur_nodegraph / ur_viewport
```

This order defines layering, not a requirement that every later module link every earlier module. Remove unused dependencies. `ur_scene_bridge` remains a separate thin C ABI boundary. Preserve `cmake/module_dependency_lint.py`.

Public `ur_platform` headers must not expose Qt types or platform-native handles. `ur_gfx` owns graphics-backend integration. Higher layers must not directly depend on QRhi or Vulkan internals.

## Efficient execution policy

Engineering evidence is mandatory; process ritual is not.

- Once a design is approved and implementation is authorized, proceed directly. Do not request duplicate approval because an external workflow guide suggests another handoff.
- External skills, playbooks, and agent methodologies are advisory unless required by the platform, explicitly requested by the user, or necessary for a concrete engineering risk.
- Compress or skip procedures designed mainly to constrain weak models: repeated intent confirmation, tutorial-level micro-steps, mandatory worktrees, mandatory PRs, mandatory subagents, duplicated reviews, artificial batching, and progress narration without new evidence.
- Plans must be high-signal: ownership boundaries, files, interfaces, behavioral tests, dependency order, rollback points, and completion gates.
- Test-first development applies at observable behavior boundaries. Do not manufacture redundant tests for trivial mechanical edits.
- Direct coherent commits to `main` are preferred under the single-branch policy after local validation.
- Report only material findings, blockers, design changes, completed checkpoints, or verification results.
- Efficiency never justifies skipping gates, concealing uncertainty, overstating completion, or bypassing immutable phase persistence.

## Implementation discipline

- Replace placeholder tests with behavioral tests when a module becomes active.
- Do not begin Dock until Minimal Text Rendering and Dock-precondition UI Foundation are complete.
- Modify an ADR when changing ownership, lifetime, input semantics, rendering order, module sequencing, cross-platform policy, or sanitizer boundaries.
- Keep files focused and interfaces explicit; avoid unrelated refactoring during milestone work.
- Prefer deterministic inputs and explicit errors over hidden fallback behavior.
- A feature is not complete because it compiles; it needs a real downstream consumer and observable evidence.

## Operations authority

Read `docs/operations/REPOSITORY_OPERATIONS.md` before remote mutation, recovery, CI diagnosis, binary-resource updates, or phase publication. Use the checked-in scripts under `tools/maintenance/` rather than reconstructing commands from chat history.

Remote writes must preserve a single coherent `main` history. Prefer Git Data API atomic tree commits over dozens of Contents API commits. Temporary diagnostic workflows must live in paths ignored by the product workflow and must be removed after use.

## Phase completion and persistence

`PHASE_VERSION` names the latest completed phase without the `phase/` prefix.

A phase is complete only after:

1. full strict validation passes on `main`;
2. source reality, specs, plans, ADRs, README, `INDEX.md`, and operations docs are current;
3. an annotated `phase/<PHASE_VERSION>` tag identifies the validated commit;
4. a complete Git bundle and SHA-256 sidecar are generated;
5. `git bundle verify` passes;
6. a fresh clone from the bundle passes `git fsck --full`;
7. bundle, sidecar, and verification report are stored in an immutable GitHub Release;
8. Release assets are downloaded again and the sidecar verifies successfully.

Existing phase tags and Release assets are immutable. Never move, overwrite, or silently recreate them.
