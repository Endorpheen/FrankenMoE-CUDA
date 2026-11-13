#!/bin/bash
# Remove old local run sets under builds/, keeping the newest N (default 1).
#
# Every run set is a full copy of a runtime build (over 1.5 GiB), so old ones
# are deleted as soon as a newer one is verified. Names are date-stamped
# (goose-YYYY-MM-DD-...), so lexical order is chronological order.
#
# Usage:
#   scripts/prune-local-builds.sh            # list what would be removed
#   scripts/prune-local-builds.sh --keep 2   # keep the two newest
#   scripts/prune-local-builds.sh --yes      # actually delete
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILDS="$ROOT/builds"
keep=1
apply=0

while [ $# -gt 0 ]; do
    case "$1" in
        --keep) keep="${2:?--keep needs a number}"; shift 2 ;;
        --yes)  apply=1; shift ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

if ! [[ "$keep" =~ ^[0-9]+$ ]] || [ "$keep" -lt 1 ]; then
    echo "--keep must be a positive integer" >&2
    exit 2
fi

if [ ! -d "$BUILDS" ]; then
    echo "no builds directory at $BUILDS"
    exit 0
fi

mapfile -t sets < <(find "$BUILDS" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort)
total=${#sets[@]}
if [ "$total" -le "$keep" ]; then
    echo "nothing to prune: $total run set(s), keeping $keep"
    exit 0
fi

remove=${sets[@]:0:$((total - keep))}
keep_list=${sets[@]:$((total - keep))}

echo "keeping: $keep_list"
for name in $remove; do
    path="$BUILDS/$name"
    resolved="$(realpath -m "$path")"
    if [ "$resolved" != "$BUILDS/$name" ] || [ -L "$path" ]; then
        echo "refusing unsafe path: $path" >&2
        exit 1
    fi
    if [ "$apply" -eq 1 ]; then
        rm -rf -- "$path"
        echo "removed: $path"
    else
        echo "would remove: $path"
    fi
done

if [ "$apply" -eq 0 ]; then
    echo "dry run only; pass --yes to delete"
fi
