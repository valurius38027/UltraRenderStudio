#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 <phase/tag> <output-directory>" >&2
}

if [[ $# -ne 2 ]]; then
    usage
    exit 2
fi

tag=$1
output_dir=$2

if [[ ! $tag =~ ^phase/[a-z0-9][a-z0-9._-]*$ ]]; then
    echo "invalid phase tag: $tag" >&2
    exit 2
fi

repo_root=$(git rev-parse --show-toplevel 2>/dev/null) || {
    echo "not inside a Git repository" >&2
    exit 1
}
cd "$repo_root"

if [[ -n $(git status --porcelain --untracked-files=all) ]]; then
    echo "refusing to bundle a dirty worktree" >&2
    exit 1
fi

if git show-ref --verify --quiet "refs/tags/$tag"; then
    echo "phase tag already exists: $tag" >&2
    exit 1
fi

mkdir -p "$output_dir"
output_dir=$(cd "$output_dir" && pwd -P)
phase_name=${tag#phase/}
bundle_path="$output_dir/UltraRenderStudio-$phase_name.bundle"
sidecar_path="$bundle_path.sha256"
temporary_bundle="$bundle_path.tmp"
created_tag=false

cleanup() {
    status=$?
    rm -f "$temporary_bundle"
    if [[ $status -ne 0 && $created_tag == true ]]; then
        git tag -d "$tag" >/dev/null 2>&1 || true
    fi
    exit "$status"
}
trap cleanup EXIT

git tag -a "$tag" -m "UltraRenderStudio phase $phase_name"
created_tag=true

git bundle create "$temporary_bundle" --all
git bundle verify "$temporary_bundle"
mv "$temporary_bundle" "$bundle_path"
(
    cd "$output_dir"
    sha256sum "$(basename "$bundle_path")" > "$(basename "$sidecar_path")"
)

printf 'tag=%s\n' "$tag"
printf 'commit=%s\n' "$(git rev-parse "$tag^{commit}")"
printf 'bundle=%s\n' "$bundle_path"
printf 'sidecar=%s\n' "$sidecar_path"
cat "$sidecar_path"

trap - EXIT
