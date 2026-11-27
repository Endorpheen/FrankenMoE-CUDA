#!/usr/bin/env bash
# Assemble a release from an accepted set, so the packaging step is a command and not a memory.
#
# The release published on 2026-11-19 was assembled by hand; this is that assembly written down.
# It does not build anything: it takes a set of libraries and a host binary that already passed
# acceptance, verifies it against the two pre-flight gates, and produces the archive, its checksum
# and a revision file.
#
# Usage: bash tools/assemble-release.sh <set-dir> <out-dir> <tag> [manifest.md]
#
# Refuses when a required file is missing or a pre-flight gate fails. Never writes inside <set-dir>:
# the accepted set is immutable (rule 22).
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SET=${1:?usage: assemble-release.sh <set-dir> <out-dir> <tag> [manifest.md]}
OUT=${2:?out-dir required}
TAG=${3:?tag required}
MANIFEST=${4:-}

[ -d "$SET" ] || { echo "no such set: $SET" >&2; exit 2; }
[ -e "$OUT" ] && { echo "refusing to overwrite $OUT" >&2; exit 2; }

required=(llama-server libllama.so.0 libllama-common.so.0 libllama-server-impl.so libmtmd.so.0
          libggml.so.0 libggml-base.so.0 libggml-cpu.so.0 libggml-cuda.so.0 libmoe_mmid_tiled.so)
missing=()
for f in "${required[@]}"; do
    [ -e "$SET/$f" ] || missing+=("$f")
done
if [ "${#missing[@]}" -ne 0 ]; then
    printf 'set is incomplete, missing: %s\n' "${missing[*]}" >&2
    exit 3
fi

echo "== pre-flight: the accepted components are in the shipped library"
python3 "$ROOT/tools/check-accepted-components.py" "$SET"
echo "== pre-flight: the cumulative baseline is present"
python3 "$ROOT/tools/check-cumulative-baseline.py" "$SET"

mkdir -p "$OUT"
for f in "${required[@]}"; do
    cp -L "$SET/$f" "$OUT/$f"
done
# The versioned and unversioned names as well: a set carries libggml.so and libggml.so.0.27.0 next
# to the SONAME the binary asks for, and a release that drops them is smaller than the set it claims
# to be. Dereferenced, so the archive holds files and not links into the build tree.
for f in "$SET"/lib*.so*; do
    [ -e "$f" ] || continue
    base=$(basename "$f")
    [ -e "$OUT/$base" ] || cp -L "$f" "$OUT/$base"
done

REV=$(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)
printf '%s\n' "$REV" > "$OUT/REVISION"

# The build command is part of the artefact. On 2026-11-19 a fresh tools/build-independent-apex.sh run
# produced a library with 87 unit doors where the accepted release has 198 and four accepted components
# missing, and nothing in the release said which path had produced it - so "reproducible" could not be
# checked and the difference had to be reconstructed by forensics. A release now names its producer, or
# the field says unknown, which is itself the finding.
if [ -n "${FG_BUILD_COMMAND:-}" ]; then
    printf '%s\n' "$FG_BUILD_COMMAND" > "$OUT/BUILD-COMMAND"
elif [ -f "$SET/BUILD-COMMAND" ]; then
    cp "$SET/BUILD-COMMAND" "$OUT/BUILD-COMMAND"
else
    printf 'unknown: the producing command was not recorded when this set was built\n' > "$OUT/BUILD-COMMAND"
fi

if [ -n "$MANIFEST" ]; then
    [ -f "$MANIFEST" ] || { echo "no such manifest: $MANIFEST" >&2; exit 2; }
    cp "$MANIFEST" "$OUT/MANIFEST.md"
else
    {
        printf '# GooseInfer CUDA runtime - %s\n\n' "$TAG"
        printf 'Assembled from an accepted set. Source revision `%s`.\n\n' "$REV"
        printf 'Every file here is listed in SHA256SUMS. Weights are not included.\n'
    } > "$OUT/MANIFEST.md"
fi

# The published set of 2026-11-19 carried RUNPATH=/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin,
# an absolute path from the build host, which makes the archive work on that host even without
# LD_LIBRARY_PATH and hides the requirement. Not fatal here - the accepted set has it - but named
# loudly so the next build does not repeat it.
if readelf -d "$OUT/llama-server" 2>/dev/null | grep -q '/home/end0'; then
    printf 'WARNING: llama-server carries a RUNPATH into the build tree; the archive will need\n' >&2
    printf '         LD_LIBRARY_PATH and will silently prefer that tree on the build host.\n' >&2
    printf '         The fix for the next build is -Wl,-rpath,\x27$ORIGIN\x27.\n' >&2
fi

(cd "$OUT" && sha256sum llama-server *.so* > SHA256SUMS)

ARCHIVE="$OUT/../$(basename "$OUT")-linux-x86_64.tar.gz"
tar -czf "$ARCHIVE" -C "$OUT" .
sha256sum "$ARCHIVE" > "$ARCHIVE.sha256"

printf 'assembled: %s\n' "$OUT"
printf 'archive:   %s\n' "$ARCHIVE"
cat "$ARCHIVE.sha256"
