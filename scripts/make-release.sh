#!/bin/bash
# Create an annotated tag and a GitHub Release from main.
#
# Usage:
#   scripts/make-release.sh --version v0.1.0 --notes-file notes.md [--asset FILE]... [--yes]
#
# Without --yes it only prints what would happen (dry run).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

version=""
notes=""
apply=0
assets=()

while [ $# -gt 0 ]; do
    case "$1" in
        --version) version="${2:?}"; shift 2 ;;
        --notes-file) notes="${2:?}"; shift 2 ;;
        --asset) assets+=("${2:?}"); shift 2 ;;
        --yes) apply=1; shift ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

[ -n "$version" ] || { echo "--version is required" >&2; exit 2; }
[[ "$version" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-rc\.[0-9]+)?$ ]] || {
    echo "version must look like v0.1.0 or v0.1.0-rc.1" >&2; exit 2;
}
[ -n "$notes" ] && [ -f "$notes" ] || { echo "--notes-file must exist" >&2; exit 2; }

echo "release $version"
echo "  repository: $(git remote get-url origin)"
echo "  commit:     $(git rev-parse HEAD)"

if [ -n "$(git status --porcelain)" ]; then
    echo "working tree is not clean; commit or stash first" >&2
    exit 1
fi
if [ "$(git rev-parse HEAD)" != "$(git rev-parse origin/main)" ]; then
    echo "HEAD is not origin/main; push first" >&2
    exit 1
fi
if git rev-parse "$version" >/dev/null 2>&1; then
    echo "tag $version already exists" >&2
    exit 1
fi
if ! python3 tools/check-goose-tree.py; then
    echo "goose tree check failed" >&2
    exit 1
fi

if [ ! -f "$notes" ]; then
    echo "notes file missing" >&2
    exit 1
fi

large=$(git ls-tree -r -l HEAD | awk '$4 > 100000000 {print $5}' | head -5)
if [ -n "$large" ]; then
    echo "files above 100 MB in the tag tree:" >&2
    echo "$large" >&2
    exit 1
fi

for asset in "${assets[@]:-}"; do
    [ -n "$asset" ] || continue
    [ -f "$asset" ] || { echo "asset not found: $asset" >&2; exit 1; }
    size=$(stat -c %s "$asset")
    if [ "$size" -gt 2147483648 ]; then
        echo "asset over the 2 GiB GitHub limit: $asset" >&2
        exit 1
    fi
    echo "  asset: $asset ($(du -h "$asset" | cut -f1))"
done

if [ "$apply" -eq 0 ]; then
    echo
    echo "dry run: would create annotated tag $version, push it, and publish the release"
    echo "notes:"
    sed 's/^/  /' "$notes" | head -40
    echo
    echo "pass --yes to publish"
    exit 0
fi

git tag -a "$version" -F "$notes"
git push origin "$version"

args=(release create "$version" --title "$version" --notes-file "$notes")
if [[ "$version" == *-rc.* ]]; then
    args+=(--prerelease)
fi
for asset in "${assets[@]:-}"; do
    [ -n "$asset" ] || continue
    args+=("$asset")
done

gh "${args[@]}"
echo "published: $(gh release view "$version" --json url --jq .url)"
for asset in "${assets[@]:-}"; do
    [ -n "$asset" ] || continue
    sha256sum "$asset"
done
