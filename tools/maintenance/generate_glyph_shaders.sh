#!/usr/bin/env bash
set -euo pipefail
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
qsb=${UR_QSB:-/usr/lib/qt6/bin/qsb}
[[ -x $qsb ]] || { echo "qsb not executable: $qsb" >&2; exit 2; }
out=$repo_root/libs/ur_gfx/src/shaders
LANG=C.UTF-8 LC_ALL=C.UTF-8 "$qsb" -o "$out/glyph.vert.qsb" "$out/glyph.vert"
LANG=C.UTF-8 LC_ALL=C.UTF-8 "$qsb" -o "$out/glyph.frag.qsb" "$out/glyph.frag"
sha256sum "$out/glyph.vert.qsb" "$out/glyph.frag.qsb"
