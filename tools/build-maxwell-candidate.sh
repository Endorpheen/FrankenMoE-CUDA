#!/usr/bin/env bash
# Build a Maxwell candidate on the accepted EXP906 base.
#
# The accepted runtime is our-src (13 files with Goose/EXP906 patches) plus the
# flint models and arch sources, linked by link-fixed/commands.json. This script
# recompiles ONLY the four objects Maxwell touches (arch, model, context,
# maxwell) and relinks in a new directory; every other object and every ggml
# library stays byte-identical to the accepted reference.
#
# The reference directory is never written to.
#
# Usage: bash tools/build-maxwell-candidate.sh <out-dir>
set -euo pipefail
OUT=${1:?out dir}
BASE=${FG_BASE:-/home/end0/AI/LlamaServer/runtime-build-tree-v8/our-src}
SRC=${FG_SRC:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
RECOVERY=${FG_RECOVERY:-/home/end0/AI/LlamaServer/franken-recovery-20261116}
REF=${FG_REF:-/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run}
mkdir -p "$OUT/obj" "$OUT/src/models" "$OUT/run"
cp -a "$BASE"/. "$OUT/src"/
cp "$SRC"/src/models/*.cpp "$SRC"/src/models/models.h "$OUT/src/models/"
cp "$SRC"/src/llama-arch.cpp "$OUT/src/"

python3 - "$OUT" <<'PY'
import re, sys
out = sys.argv[1]
def rd(p): return open(p).read()
def wr(p, s): open(p, "w").write(s)

p = f"{out}/src/llama-arch.h"; s = rd(p)
if "LLM_ARCH_MAXWELL" not in s:
    s = s.replace("    LLM_ARCH_UNKNOWN,", "    LLM_ARCH_UNKNOWN,\n    LLM_ARCH_MAXWELL,", 1)
if "LLM_KV_MAXWELL_BSGQA_BLOCK_SIZE" not in s:
    end = s.index("};", s.index("enum llm_kv {"))
    s = s[:end] + """    LLM_KV_MAXWELL_BSGQA_BLOCK_SIZE,
    LLM_KV_MAXWELL_BSGQA_SUPPORT_BLOCKS,
    LLM_KV_MAXWELL_BSGQA_PREFIX_WINDOW,
    LLM_KV_MAXWELL_BSGQA_REMOTE_BLOCKS,
""" + s[end:]
wr(p, s)

p = f"{out}/src/llama-model.cpp"; s = rd(p)
if "llama_model_maxwell" not in s:
    for anchor in ("        case LLM_ARCH_BAILINGMOE3:\n            return new llama_model_bailingmoe3(params);",
                   "        case LLM_ARCH_MINIMAX_M3:\n            return new llama_model_minimax_m3(params);"):
        if anchor in s:
            s = s.replace(anchor, anchor + "\n        case LLM_ARCH_MAXWELL:\n            return new llama_model_maxwell(params);", 1)
            break
    else:
        sys.exit("factory anchor not found")
    m = re.search(r"(        case LLM_ARCH_[A-Z0-9_]+:\n            return LLAMA_ROPE_TYPE_NEOX;)", s)
    s = s.replace(m.group(1), "        case LLM_ARCH_MAXWELL:\n" + m.group(1), 1)
    idx = s.find("        case LLM_ARCH_MINIMAX_M3:\n            {")
    if idx < 0:
        sys.exit("msa anchor not found")
    s = s[:idx] + """        case LLM_ARCH_MAXWELL:
            {
                if (hparams.indexer_head_size > 0) {
                    res = new llama_kv_cache_msa(
                            *this, params.type_k, params.type_v, !cparams.flash_attn,
                            cparams.offload_kqv, cparams.kv_unified, cparams.n_ctx_seq,
                            cparams.n_seq_max, 1, hparams.n_swa, hparams.swa_type,
                            nullptr, nullptr, nullptr);
                } else {
                    res = new llama_kv_cache(
                            *this, hparams, params.type_k, params.type_v, !cparams.flash_attn,
                            cparams.offload_kqv, cparams.kv_unified, cparams.n_ctx_seq,
                            cparams.n_seq_max, 1, hparams.n_swa, hparams.swa_type,
                            nullptr, nullptr, nullptr, nullptr);
                }
            } break;
""" + s[idx:]
wr(p, s)

p = f"{out}/src/llama-context.cpp"; s = rd(p)
if "LLM_ARCH_MAXWELL" not in s:
    m = re.search(r"(        model\.arch == LLM_ARCH_HY_V4\)) \{", s)
    if not m:
        sys.exit("budget anchor not found")
    s = s.replace(m.group(1), "        model.arch == LLM_ARCH_MAXWELL ||\n" + m.group(1), 1)
wr(p, s)
print("sources patched")
PY

FLAGS="-O3 -DNDEBUG -std=gnu++17 -fPIC -fvisibility=hidden -fopenmp \
-DGGML_BACKEND_SHARED -DGGML_SHARED -DGGML_USE_CPU -DGGML_USE_CUDA \
-DLLAMA_BUILD -DLLAMA_SHARED \
-I$OUT/src -I$SRC/src -I$SRC/build-cuda/src -I$SRC/include -I$SRC/ggml/include \
-I$BASE/../upstream-include"
for f in llama-arch.cpp llama-model.cpp llama-context.cpp; do
    echo "cc $f"
    g++-13 $FLAGS -c "$OUT/src/$f" -o "$OUT/obj/${f%.cpp}.o"
done
echo "cc models/maxwell.cpp"
g++-13 $FLAGS -c "$OUT/src/models/maxwell.cpp" -o "$OUT/obj/maxwell.o"

python3 - "$OUT" "$RECOVERY" <<'PY'
import json, subprocess, sys
out, recovery = sys.argv[1], sys.argv[2]
cmd = json.load(open(f"{recovery}/link-fixed/commands.json"))[1]
repl = {
    f"{recovery}/core-clean/llama-model.cpp.o": f"{out}/obj/llama-model.o",
    f"{recovery}/core-clean/llama-context.cpp.o": f"{out}/obj/llama-context.o",
    f"{recovery}/core-clean/vendor-models/llama-arch.o": f"{out}/obj/llama-arch.o",
}
args = [f"{out}/libllama.so.0" if a == f"{recovery}/link-fixed/libllama.so.0" else repl.get(a, a) for a in cmd]
i = next(n for n, a in enumerate(args) if a.startswith(f"{recovery}/support/bin/"))
args.insert(i, f"{out}/obj/maxwell.o")
r = subprocess.run(args, capture_output=True, text=True)
print(r.stderr[-1500:])
sys.exit(r.returncode)
PY

cp -a "$REF"/llama-server "$REF"/libggml*.so.0 "$REF"/libllama-common.so.0 \
      "$REF"/libllama-server-impl.so "$REF"/libmtmd.so.0 "$OUT/run/"
cp "$OUT/libllama.so.0" "$OUT/run/libllama.so.0"
cd "$OUT/run" && sha256sum llama-server lib*.so* > SHA256SUMS
echo "candidate: $(stat -c%s "$OUT/run/libllama.so.0") bytes, FG=$(strings "$OUT/run/libllama.so.0" | grep -c 'FG[0-9][0-9][0-9]')"
sha256sum libggml-cuda.so.0
