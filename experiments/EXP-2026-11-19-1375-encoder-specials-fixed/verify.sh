#!/usr/bin/env bash
# The ported encoder against llama_tokenize on the real vocabulary, over prompts that include a special-token
# marker and a trailing newline: the two things that were wrong before this change.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
D=$(dirname "$0")
CLI=${1:-$R/builds/goose-standalone/frankenmoe-tokenize}
MODEL=${2:-/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf}
gcc-13 -O2 -o /tmp/native_ids "$D/native_ids.c" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
    -L "$R/builds/flint-acc/bin" -lllama -lggml-base -Wl,-rpath,"$R/builds/flint-acc/bin" || exit 1
P="$D/prompts.txt"
[ -f "$D/prompts_wide.txt" ] && P="$D/prompts_wide.txt"
/tmp/native_ids "$MODEL" x --whole < "$P" 2>/dev/null > /tmp/enc_native.txt
"$CLI" "$MODEL" --stdin --ids < "$P" > /tmp/enc_goose.txt 2>/dev/null
python3 - <<'PY'
a = open('/tmp/enc_native.txt').read().strip()
b = open('/tmp/enc_goose.txt').read().strip()
na = [int(x) for x in a.strip('[]\n').split(', ')]
nb = [int(x) for x in b.strip('[]\n').split(', ')]
if na == nb:
    print(f"encoder: identical, {len(na)} ids")
    raise SystemExit(0)
n = min(len(na), len(nb))
d = [i for i in range(n) if na[i] != nb[i]]
print(f"encoder: DIFFERS native {len(na)} goose {len(nb)}, first at {d[0] if d else 'length only'}")
raise SystemExit(1)
PY
