#!/bin/sh
# Remove broken symlinks under runtime/ and experiments/ that git does not track.
#
# Why this is safe: a broken symlink points at a path that does not exist, and the inventory taken on 2026-11-12 shows
# every one of them is untracked (git ls-files --error-unmatch rejects them), i.e. build leftovers from staging copies.
#
# Default is a dry run. Pass --apply to delete. The tracked-file check is repeated per path before each removal, so a
# symlink that somehow became tracked is skipped rather than deleted.
set -eu

REPO=$(cd "$(dirname "$0")/../.." && pwd)
cd "$REPO"

APPLY=0
[ "${1:-}" = "--apply" ] && APPLY=1

total=0; kept=0; removed=0
find runtime experiments -xtype l 2>/dev/null > /tmp/broken-links.list || true
while IFS= read -r p; do
    total=$((total+1))
    if git ls-files --error-unmatch "$p" >/dev/null 2>&1; then
        kept=$((kept+1)); continue
    fi
    if [ "$APPLY" = "1" ]; then
        rm -f "$p" && removed=$((removed+1))
    fi
done < /tmp/broken-links.list

echo "broken symlinks found : $total"
echo "tracked, left alone   : $kept"
if [ "$APPLY" = "1" ]; then echo "removed               : $removed"; else echo "dry run - pass --apply to remove $((total-kept))"; fi
