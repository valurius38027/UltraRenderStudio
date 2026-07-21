#!/usr/bin/env bash
set -euo pipefail
repo=${1:-https://github.com/valurius38027/UltraRenderStudio.git}
dest=${2:-UltraRenderStudio}
[[ ! -e $dest ]] || { echo "destination exists: $dest" >&2; exit 2; }
git clone "$repo" "$dest"
git -C "$dest" switch main
git -C "$dest" fsck --full
printf 'Recovered %s at %s\n' "$dest" "$(git -C "$dest" rev-parse HEAD)"
