#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a fresh staging directory}
ui=${GOOSEINFER_UI_PACKAGE:-${FRANKENMOE_UI_PACKAGE:-}}
: "${ui:?Set GOOSEINFER_UI_PACKAGE to the verified chat UI package directory}"
apex="$root/build/checkpoint-apex-exp1880-before-inkling/checkpoint"
ink="$root/build/checkpoint-inkling-exp1969-before-bonsai"
bonsai="$root/build/bonsai-mmq-pipeline-2019-http"
mtp="$root/build/bonsai-mtp-warp-rows-2047"

mkdir "$out"
mkdir -p "$out"/{bin,profiles,cuda}
install -m 755 "$src/launch-linux.sh" "$out/gooseinfer"
ln -s gooseinfer "$out/frankenmoe"
install -m 755 "$apex/frankenmoe-infer" "$out/bin/apex-server"
install -m 755 "$ink/frankenmoe-infer" "$out/bin/inkling-server"
install -m 755 "$bonsai/frankenmoe-infer" "$out/bin/bonsai-server"
install -m 755 "$mtp/bonsai-mtp-warp-rows" "$out/bin/bonsai-mtp"
install -m 644 "$bonsai/pipeline.cubin" "$out/cuda/bonsai-http.cubin"
install -m 644 "$mtp/mvq.cubin" "$out/cuda/bonsai-mtp.cubin"
install -m 644 "$root/build/bonsai-mtp-head-2025/frozen-goose.ptx" "$out/cuda/frozen-goose.ptx"
install -m 644 "$root/tools/gooseinfer-console.py" "$root/tools/frankenmoe-console.py" "$ui/LICENSE" "$out/"
install -m 644 "$root/docs/MULTIMODEL-ALPHA.md" "$out/README.md"
cp -a "$ui/web" "$ui/licenses" "$out/"

for model in apex inkling; do
    if [[ "$model" == apex ]]; then from=$apex; else from=$ink; fi
    # Retain exact flags, with the caller providing its own model path and port.
    awk '/^--serve$/{exit} /^--/{keep=1} keep && $0 !~ /[.]gguf$/ {print}' \
        "$from/argv.txt" >"$out/profiles/$model.argv"
    sed -nE '/^(GGML_|GOOSE_|I2_)/p' "$from/env.txt" >"$out/profiles/$model.env"
done
cat >"$out/profiles/bonsai.argv" <<'PROFILE'
--ctx-size
36864
--batch-size
4096
--ubatch-size
1024
--cache-type-k
q4_0
--cache-type-v
q4_0
--threads
8
--threads-batch
16
--cpu-mask
0f0f
--cpu-mask-batch
ffff
--cpu-strict
1
--cpu-strict-batch
1
PROFILE
cat >"$out/profiles/bonsai.env" <<'PROFILE'
BONSAI_FLASH_F32=1
BONSAI_FLASH_REUSE=1
BONSAI_FLASH_ORDERED_MERGE=1
BONSAI_FLASH_COMPACT=1
BONSAI_FLASH_ASYNC=1
PROFILE
cat >"$out/profiles/bonsai-mtp.env" <<'PROFILE'
BONSAI_UBATCH=1024
BONSAI_FLASH_F32=1
BONSAI_FLASH_REUSE=1
BONSAI_FLASH_ORDERED_MERGE=1
BONSAI_FLASH_COMPACT=1
BONSAI_FLASH_ASYNC=1
BONSAI_FLASH_LINEAR_Q=1
BONSAI_FFN_PREFIX=1
BONSAI_MTP_BATCH_PREFILL=1
BONSAI_MTP_VERIFY_Q8=1
BONSAI_MTP_VERIFY_EXACT=1
BONSAI_MTP_DEVICE_COMMIT=1
BONSAI_MTP_VERIFY_SHARED_ATTN=1
BONSAI_MTP_DEVICE_CHOICE=1
BONSAI_MTP_VERIFY_PARALLEL_MERGE=1
BONSAI_MTP_DRAFT_Q8_HEAD=1
BONSAI_MTP_VERIFY_ATTN_TAIL=1
BONSAI_MTP_GREEDY=1
BONSAI_MTP_VERIFY_ROWS=4
BONSAI_MTP_MVQ_SHAPE=1
PROFILE
