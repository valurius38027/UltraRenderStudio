# UltraRenderStudio Repository Authority

This file is the highest-authority repository instruction for maintainers and coding agents.

## Branch policy

- `main` is the only maintained remote branch.
- Do not create or retain remote feature, development, release, or bundle-vault branches.
- Temporary local branches and worktrees are optional implementation tools, not mandatory ceremony. Completed work must reach `main` and temporary refs must be removed.
- Durable checkpoints use annotated `phase/*` tags and GitHub Release assets, not long-lived branches.

## Source and environment authority

- The canonical source repository is `valurius38027/UltraRenderStudio`.
- The canonical branch is `main`.
- The supported production-development host is Debian 13 (`trixie`) amd64.
- The canonical SDK recovery source is `valurius38027/toolchain` and its immutable UltraRender profile releases.
- The current deterministic SDK baseline is `ultrarender-sdk-debian13-v2026.07.20.1` until a newer profile release passes its publication gates.
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
8. accurate README, ADR, spec, plan, and `INDEX.md` claims.

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

This order defines layering, not a requirement that every later module directly link every earlier module. Remove unused dependencies. `ur_scene_bridge` remains a separate thin C ABI boundary. Preserve the enforced dependency rules in `cmake/module_dependency_lint.py`.

Public `ur_platform` headers must not expose Qt types or platform-native handles. `ur_gfx` owns graphics-backend integration. Higher layers must not directly depend on QRhi or Vulkan internals.

## Efficient execution policy

Engineering evidence is mandatory; process ritual is not.

- Once the user has approved a design and explicitly authorized implementation, proceed directly. Do not request a second approval merely because an external workflow guide suggests another handoff.
- External skills, playbooks, and agent methodologies are advisory unless required by the platform, explicitly requested by the user, or necessary to control a concrete engineering risk.
- Compress or skip procedures designed mainly to constrain weak models: repeated intent confirmation, tutorial-level micro-steps, mandatory worktrees, mandatory PRs, mandatory subagents, duplicated reviews, artificial batching, and progress narration without new evidence.
- A plan must be high-signal: exact ownership boundaries, files, interfaces, test strategy, dependency order, rollback points, and completion gates. It need not paste trivial implementation code or decompose every edit into two-minute steps.
- Use the strongest available reasoning directly. Group tightly coupled changes into coherent implementation units instead of optimizing for maximum task count.
- Test-first development applies at observable behavior boundaries. Record an intended failing test or failing gate before implementing each coherent behavior group; do not manufacture redundant tests for trivial mechanical edits.
- Direct commits to `main` are permitted and preferred under the single-branch policy when each commit is coherent and locally validated.
- Report progress only when there is a material finding, design change, blocker, completed checkpoint, or verification result.
- Never use efficiency as justification to skip engineering gates, conceal uncertainty, overstate completion, or bypass immutable phase persistence.

## Implementation discipline

- Replace placeholder tests with behavioral tests when a module becomes active.
- Do not begin Dock while the minimum text and UI-foundation milestones remain incomplete.
- Modify an ADR when changing ownership, lifetime, input semantics, module sequencing, cross-platform policy, or a sanitizer boundary.
- Keep files focused and interfaces explicit; avoid unrelated refactoring during milestone work.
- Prefer deterministic inputs and explicit errors over hidden fallback behavior.
- A feature is not complete because it compiles; it must have a real downstream consumer and observable evidence.

## Phase completion and persistence

`PHASE_VERSION` names the latest completed phase without the `phase/` prefix.

A phase is complete only after:

1. the full strict validation workflow passes on `main`;
2. the source reality, specs, plans, ADRs, README, and `INDEX.md` are updated;
3. an annotated `phase/<PHASE_VERSION>` tag identifies the validated commit;
4. a complete Git bundle and SHA-256 sidecar are generated;
5. the bundle passes `git bundle verify`;
6. a fresh clone from the bundle passes `git fsck --full`;
7. the immutable bundle, sidecar, and verification report are stored in a GitHub Release;
8. the Release assets are downloaded again and the sidecar verifies successfully.

Existing phase tags and Release assets are immutable. Never move, overwrite, or silently recreate them.
