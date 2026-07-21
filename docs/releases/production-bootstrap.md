# Production Bootstrap Recovery Release

- Phase tag: `phase/production-bootstrap-v1.1`
- Recovery bundle: `UltraRenderStudio-production-bootstrap-v1.1.bundle`
- Digest sidecar: `UltraRenderStudio-production-bootstrap-v1.1.bundle.sha256`
- Canonical source repository: `https://github.com/valurius38027/UltraRenderStudio.git`
- Canonical branch: `main`

## Included state

The original recovery bundle contains the complete pre-remote implementation history. The canonical
GitHub repository now maintains only `main`; durable milestones are represented by annotated
`phase/*` tags and immutable GitHub Release assets.

The completed phase includes:

1. GCC and Clang strict-warning closure;
2. backend-neutral platform event FIFO;
3. real Vulkan QRhi swapchain presentation and resize recovery;
4. topmost immediate-mode hit arbitration and stale capture cleanup;
5. a real editor event/render loop with finite-frame smoke mode;
6. updated architectural authority for the next text milestone.

## Sanitizer note

The real Vulkan window integration process uses a test-local LSan suppression
for one 72-byte process-exit allocation attributed directly to
`libvulkan.so.1`. All project-owned QRhi resources are explicitly destroyed
before detaching the window. The suppression is not applied to offscreen
rendering or the remaining test suite.

## Integrity model

A bundle cannot contain a commit that records the bundle's own final digest without changing that
digest. Therefore the immutable annotated tag identifies the source state, while the adjacent
`.sha256` sidecar and vault manifest are the authoritative binary integrity records.

Every published phase bundle must pass:

```bash
git bundle verify UltraRenderStudio-production-bootstrap-v1.1.bundle
git clone UltraRenderStudio-production-bootstrap-v1.1.bundle restored
git -C restored fsck --full
```

The sidecar is checked with:

```bash
sha256sum -c UltraRenderStudio-production-bootstrap-v1.1.bundle.sha256
```

## Restore

```bash
sha256sum -c UltraRenderStudio-production-bootstrap-v1.1.bundle.sha256
git clone https://github.com/valurius38027/UltraRenderStudio.git
cd UltraRenderStudio
git switch main
git clone https://github.com/valurius38027/toolchain.git .toolchain
sudo bash .toolchain/profiles/ultrarender/scripts/restore.sh latest
```

After restore, run the GCC and Clang commands in the repository `README.md` before continuing.

## Canonical remote publication

The canonical `main` workflow validates GCC/ASan/UBSan, Clang, CTest, Xvfb/Lavapipe, editor
smoke, and dependency layering. When `PHASE_VERSION` identifies a phase without an existing tag,
the workflow creates `phase/<PHASE_VERSION>`, generates a complete bundle, verifies a fresh clone,
and publishes the bundle, checksum, and verification report as immutable Release assets.
