#!/usr/bin/env bash
set -euo pipefail
[[ $# -eq 2 ]] || { echo "usage: $0 <bundle> <phase-tag>" >&2; exit 2; }
bundle=$(realpath "$1")
tag=$2
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
(
  cd "$(dirname "$bundle")"
  sha256sum -c "$(basename "$bundle").sha256"
)
git bundle verify "$bundle"
git clone "$bundle" "$dir/repo"
git -C "$dir/repo" fsck --full
git -C "$dir/repo" rev-parse "$tag^{}"
