# Repository Operations and Agent Handoff

This is the executable operations authority for UltraRenderStudio. It records the remote-interaction details that are easy to lose when an agent or host changes.

## 1. Authority and invariants

- Repository: `valurius38027/UltraRenderStudio`.
- Remote branch: `main` only.
- SDK: `valurius38027/toolchain`, release `ultrarender-sdk-debian13-v2026.07.21.1` or a newer fully verified immutable release.
- Phase persistence: annotated `phase/*` tag plus GitHub Release bundle, SHA-256 sidecar and verification report.
- Never move an existing phase tag or overwrite its Release assets.

## 2. Workspace recovery after host cleanup

Preferred path:

```bash
tools/maintenance/recover_workspace.sh \
  https://github.com/valurius38027/UltraRenderStudio.git UltraRenderStudio
```

When network access is unavailable, clone the latest verified Release bundle, verify its sidecar, run `git bundle verify`, clone it, then `git fsck --full`. An uncommitted patch is only a recovery input; it is never repository authority.

Restore the SDK from the toolchain repository, not ad-hoc APT commands:

```bash
sudo bash profiles/ultrarender/scripts/restore.sh \
  ultrarender-sdk-debian13-v2026.07.21.1
```

## 3. Normal remote mutation

For many-file coherent changes, prefer Git Data API semantics:

1. resolve current `main` commit SHA;
2. create binary/text blobs as needed;
3. create one tree with `base_tree` set to current `main` (the connector accepts the commit SHA);
4. create one commit with that commit as parent;
5. fast-forward `refs/heads/main` with force disabled;
6. verify the resulting tree/files and observe the product workflow.

This avoids dozens of Contents API commits. Use Contents API only for a small independent text edit.

Binary blobs must be supplied as Base64 and the returned Git blob SHA must equal local `git hash-object <file>`. For the glyph packages the known v1 hashes are:

```text
glyph.vert.qsb git blob: 5d73f516cc891e7cb3096bc08a90d874ee6d8201
glyph.frag.qsb git blob: 9598f30fdc78b1b3fc9a87c7029a946589ec3512
```

## 4. Large or restricted payload fallback

Do not upload one opaque archive and assume the connector preserved it. A previous single-blob import changed bytes and was correctly rejected by total SHA-256 verification.

Fallback procedure:

1. split the payload into fixed small chunks;
2. calculate SHA-256 and Git blob SHA for every chunk;
3. upload each chunk separately and compare the returned Git SHA;
4. use a temporary issue-comment workflow under a path ignored by `main.yml`;
5. fetch each blob, Base64-decode and concatenate in explicit order;
6. verify the total SHA-256 before extraction;
7. regenerate deterministic binary resources and verify their digests;
8. commit once to `main`;
9. delete the temporary workflow immediately.

A failed checksum is a hard stop. Never bypass it.

## 5. Deterministic glyph shaders

Source files are authoritative. Regenerate with:

```bash
tools/maintenance/generate_glyph_shaders.sh
```

Expected SHA-256 for Minimal Text Rendering v1:

```text
d7320f79ee44f77d5c1b1f6d48c676e2f760b98fb6397f95ba2282aa751c708d  glyph.vert.qsb
896742f61a4df22510cefdd4ee34b026ae5a1a283012c1df29256a0722a63f59  glyph.frag.qsb
```

CMake regenerates and byte-compares them during every build.

## 6. CI observation and diagnosis

Do not infer that a workflow is queued forever from missing tags or a 404. Obtain the actual run and job states.

Permanent Issue #1 commands:

```text
/run-main-validation
/snapshot-main-status
```

For a failure, fetch job summaries first, then the failed job log. If connector output truncates the log, use a temporary read-only issue-comment workflow to write only the last 100-200 lines or a narrow error context into Issue #1. Remove the diagnostic workflow after use.

Temporary workflow changes must be listed in `main.yml` `paths-ignore`, otherwise every diagnostic edit downloads the full SDK and starts product validation.

## 7. Phase Release implementation details

The product workflow validates GCC/ASan/UBSan, Clang, all CTest, Xvfb/Lavapipe, QShader reproduction and dependency layering.

The release job:

1. reads `PHASE_VERSION`;
2. skips publication when the immutable phase tag already exists;
3. creates a local annotated tag and full bundle;
4. runs `git bundle verify`;
5. fresh-clones the bundle, runs `git fsck --full`, and checks tag-to-commit identity;
6. creates the remote annotated tag through Git Data REST (`git/tags`, then `git/refs`), because ordinary App-token `git push` can be blocked when the commit contains workflow files;
7. creates the Release;
8. downloads bundle and sidecar again and runs `sha256sum -c`.

On failure the workflow removes any partial Release and newly created remote tag ref.

## 8. Known remote-interaction traps

- Do not create long-lived feature or vault branches.
- Do not use a repository of Base64 bundle fragments as a backup; use Release assets.
- Do not treat an Actions run as complete merely because it was dispatched.
- Do not repeatedly add status workflows; retain one minimal dispatcher and use temporary diagnostics only when needed.
- Do not use broad LSan suppressions or disable leak detection.
- Do not claim a phase complete before the remote Release round-trip passes.
- Do not reconstruct source from chat logs when a verified bundle or `main` exists.

## 9. Handoff checklist

A replacement agent starts by reading, in order:

1. `AGENTS.md`;
2. `INDEX.md`;
3. this operations document;
4. current phase spec and plan;
5. relevant ADRs;
6. recent `main` commits and Issue #1 workflow status.

It then restores the SDK, runs the strict local baseline, and continues from the first unmet completion gate. It must not repeat already closed diagnostic experiments unless the dependency version changed.
