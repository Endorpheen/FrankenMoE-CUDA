#!/bin/bash
# Create an annotated tag and a GitHub Release from main.
#
# Usage:
#   scripts/make-release.sh --version v0.1.0 --notes-file notes.md [--asset FILE]... [--yes]
#
# Without --yes it only prints what would happen (dry run).
#
# The release decisions — tag acceptance, prerelease detection, the runtime
# asset class, both size limits and the runtime-asset retention chain — are the
# P10-05 S1 slice, `goose/misc/release-plan.goose`, reached through
# `build/goose-hosts/pkg` (`bash tools/goose-hosts/build.sh`). git, gh, stat and
# the archive work stay here. BMOE_GOOSE_PKG=0 restores the inline decisions.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
# shellcheck source=../tools/goose-hosts/client.sh
. "$ROOT/tools/goose-hosts/client.sh"

if goosehost_init "$ROOT" pkg BMOE_GOOSE_PKG; then PKG_HOST=1; else PKG_HOST=0; fi

version=""
notes=""
apply=0
assets=()
keep_old_runtime=0

while [ $# -gt 0 ]; do
    case "$1" in
        --version) version="${2:?}"; shift 2 ;;
        --notes-file) notes="${2:?}"; shift 2 ;;
        --asset) assets+=("${2:?}"); shift 2 ;;
        --keep-old-runtime) keep_old_runtime=1; shift ;;
        --yes) apply=1; shift ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

[ -n "$version" ] || { echo "--version is required" >&2; exit 2; }
if [ "$PKG_HOST" = 1 ]; then
    [ "$(goosehost_ask "VERSION $(goosehost_hex "$version")")" = "1" ] || {
        echo "version must look like v0.1.0 or v0.1.0-rc.1" >&2; exit 2;
    }
else
    [[ "$version" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-rc\.[0-9]+)?$ ]] || {
        echo "version must look like v0.1.0 or v0.1.0-rc.1" >&2; exit 2;
    }
fi
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

if [ "$PKG_HOST" = 1 ]; then
    # git ls-tree's own field order, then the module's `> 100 MB` rule per row.
    large=$(git ls-tree -r -l HEAD | awk '{print $4, $5}' | {
        while read -r size path; do
            [ -n "$size" ] || continue
            [ "$(goosehost_ask "TREE_LIMIT $size")" = "1" ] && printf '%s\n' "$path"
        done
    } | head -5)
else
    large=$(git ls-tree -r -l HEAD | awk '$4 > 100000000 {print $5}' | head -5)
fi
if [ -n "$large" ]; then
    echo "files above 100 MB in the tag tree:" >&2
    echo "$large" >&2
    exit 1
fi

has_runtime=0
for asset in "${assets[@]:-}"; do
    [ -n "$asset" ] || continue
    [ -f "$asset" ] || { echo "asset not found: $asset" >&2; exit 1; }
    size=$(stat -c %s "$asset")
    if [ "$PKG_HOST" = 1 ]; then
        if [ "$(goosehost_ask "ASSET_LIMIT $size")" = "1" ]; then
            echo "asset over the 2 GiB GitHub limit: $asset" >&2
            exit 1
        fi
        if [ "$(goosehost_ask "RUNTIME_ASSET $(goosehost_hex "$asset")")" = "1" ]; then
            has_runtime=1
        fi
    else
        if [ "$size" -gt 2147483648 ]; then
            echo "asset over the 2 GiB GitHub limit: $asset" >&2
            exit 1
        fi
        case "$(basename "$asset")" in goose-runtime-*) has_runtime=1 ;; esac
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
if [ "$PKG_HOST" = 1 ]; then
    if [ "$(goosehost_ask "PRERELEASE $(goosehost_hex "$version")")" = "1" ]; then
        args+=(--prerelease)
    fi
else
    if [[ "$version" == *-rc.* ]]; then
        args+=(--prerelease)
    fi
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

# Keep one heavy runtime archive on GitHub: the newest. Older release notes stay,
# only their runtime assets are removed. Runs after the new release exists, so a
# failed publish never leaves the project without a binary.
if [ "$has_runtime" -eq 1 ] && [ "$keep_old_runtime" -eq 0 ]; then
    while read -r old; do
        [ -n "$old" ] || continue
        [ "$old" = "$version" ] && continue
        while read -r name; do
            [ -n "$name" ] || continue
            if [ "$PKG_HOST" = 1 ]; then
                # One guard chain in the module: this release carries a runtime
                # archive, --keep-old-runtime is off, the tag is present and is
                # not the new one, and gh's asset name passes the selector.
                if [ "$(goosehost_ask "KEEP 1 0 1 0 $(goosehost_hex "$name")")" = "1" ]; then
                    continue
                fi
            else
                case "$name" in goose-runtime-*) ;; *) continue ;; esac
            fi
            gh release delete-asset "$old" "$name" --yes
            echo "removed old runtime asset: $old/$name"
        done < <(gh release view "$old" --json assets --jq '.assets[].name')
    done < <(gh release list --limit 100 --json tagName --jq '.[].tagName')
fi
