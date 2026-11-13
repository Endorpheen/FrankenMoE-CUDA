#!/bin/bash
# Assemble the unified Apex run: canonical baseline + the Goose logging library.
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-12-421-frankenmoe-logging
BASE=$F/runtime/reconstructed/build/stage-run
UNI=$F/runtime/exp421/unified

bash "$E/build_stage.sh"

rm -rf "$UNI"
mkdir -p "$UNI"
for f in "$BASE"/*; do
    ln -s "$(readlink -f "$f")" "$UNI/$(basename "$f")"
done
rm -f "$UNI/libllama-common.so" "$UNI/libllama-common.so.0" "$UNI/libllama-common.so.0.7.0"
cp "$F/runtime/exp421/build/bin/libllama-common.so.0.7.0" "$UNI/libllama-common.so.0.7.0"
ln -s libllama-common.so.0.7.0 "$UNI/libllama-common.so.0"
ln -s libllama-common.so.0.7.0 "$UNI/libllama-common.so"

python3 - "$F" "$UNI" <<'PY'
import hashlib, json, sys
from pathlib import Path
F, U = Path(sys.argv[1]), Path(sys.argv[2])
files = ['llama-server', 'libllama.so.0.7.0', 'libllama-common.so.0.7.0',
         'libllama-server-impl.so', 'libggml-base.so.0.27.0',
         'libggml-cpu.so.0.27.0', 'libggml-cuda.so.0.27.0']
def h(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()
manifest = {
    'baseline': str(F / 'runtime/reconstructed/build/stage-run'),
    'logging_common': str(F / 'runtime/exp421/build/bin/libllama-common.so.0.7.0'),
    'files': {n: h((U / n).resolve()) for n in files},
    'flags': ['fixed exp244 argv', '--moe-cache-mib 3584'],
    'profile': {'context': 262144, 'prompt_tokens': 32768, 'n_predict': 128},
}
(U / 'MANIFEST.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps(manifest, indent=2))
PY
echo UNIFIED_READY
