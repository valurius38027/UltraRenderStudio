# Production Bootstrap Recovery Release

- Phase tag: `phase/production-bootstrap-v1.1`
- Recovery bundle: `UltraRenderStudio-production-bootstrap-v1.1.bundle`
- Digest sidecar: `UltraRenderStudio-production-bootstrap-v1.1.bundle.sha256`
- Remote source repository: pending; the attempted encoded bundle vault failed integrity verification and is explicitly non-authoritative.

## Included state

The bundle contains the complete repository history, the `main` baseline, the
`feat/production-bootstrap` completion branch, and every annotated checkpoint tag through
`phase/production-bootstrap-v1.1`.

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
git clone UltraRenderStudio-production-bootstrap-v1.1.bundle UltraRenderStudio
cd UltraRenderStudio
git switch feat/production-bootstrap
sudo bash /path/to/toolchain/profiles/ultrarender/scripts/restore.sh latest
```

After restore, run the GCC and Clang commands in the repository `README.md` before continuing.
