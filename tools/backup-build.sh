#!/usr/bin/env bash
# Back up the current build before any change that could alter it.
#
# Two rules this serves, both learned the hard way today:
#   - the library that produced a good number must still exist after the next build;
#   - a measurement is only valid when the model is resident in RAM, so the residency is recorded with the result.
#
# Keeps the newest KEEP sets (default 6) and drops older ones, so the folder cannot grow without bound.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
W=${BUILD:-/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build}
KEEP=${KEEP:-6}
DEST="$ROOT/Backup"
mkdir -p "$DEST"

LABEL=${1:-$(date +%Y%m%d-%H%M%S)}
OUT="$DEST/$LABEL"
[ -e "$OUT" ] && { echo "already exists: $OUT"; exit 0; }
mkdir -p "$OUT"

LIB=$W/libllama-exp371.so.0.7.0
if [ -f "$LIB" ]; then
    cp "$LIB" "$OUT/libllama.so"
    sha256sum "$OUT/libllama.so" | cut -d' ' -f1 > "$OUT/libllama.sha256"
else
    echo "no library at $LIB"; exit 1
fi

for o in b291.o fx286pic.o fx262a.o fx263-pic.o gates.o slru-policy.o slru-runtime.o slru-shim.o \
         fg-ar.o fg-ar-bridge.o fg-q35.o fg-qg.o fg-q35-bridge.o fg-plan-ffi.o fg-gr.o fg-gr-bridge.o; do
    [ -f "$W/$o" ] && cp "$W/$o" "$OUT/" 2>/dev/null || true
done

MODEL=${MODEL:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
RES="n/a"
if [ -x /tmp/res ] && [ -f "$MODEL" ]; then RES=$(/tmp/res "$MODEL" 2>/dev/null | head -1); fi
{
    echo "backup: $LABEL"
    echo "when:   $(date -Is)"
    echo "git:    $(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "lib:    $(sha256sum "$OUT/libllama.so" | cut -d' ' -f1)"
    echo "size:   $(stat -c%s "$OUT/libllama.so") bytes"
    echo "model in RAM at backup time: $RES"
} > "$OUT/INFO.txt"

# retention: newest KEEP only
ls -1dt "$DEST"/*/ 2>/dev/null | tail -n +$((KEEP + 1)) | while read -r old; do rm -rf "$old"; done
echo "backed up to $OUT"
sed -n '1,5p' "$OUT/INFO.txt"
