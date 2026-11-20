#!/bin/bash
# Compile Goose device arithmetic through OpenCL C to driver-loadable PTX.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${1:?usage: build-goose-cuda-kernels.sh output-directory}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CLANG=${CLANG:-clang-18}
mkdir -p "$OUT"

sed '/^fn main()/,$d; s/^fn fg182d_q4_K(/export fn fg182d_q4_K(/' \
    "$ROOT/goose/cuda/goose-cuda-dequant-q4k/device.goose" > "$OUT/q4k.goose"
sed '/^fn main()/,$d; s/^fn q6_K(/export fn q6_K(/' \
    "$ROOT/goose/cuda/goose-cuda-dequant-q6k/device.goose" > "$OUT/q6k.goose"
cp "$ROOT/goose/cuda/standalone-matmul.goose" "$OUT/matmul.goose"
sed -n '/^struct F2 {/,/^}/p; /^fn dequant_q4_0(/,/^}/p' \
    "$ROOT/goose/cuda/goose-cuda-flash-attention/staging-device.goose" |
    sed 's/^fn dequant_q4_0(/export fn dequant_q4_0(/' > "$OUT/q4-attention.goose"
cat "$ROOT/goose/cuda/standalone-q4-attention.goose" >> "$OUT/q4-attention.goose"
sed '/^fn main()/,$d; s/^fn gdn(/export fn gdn(/' \
    "$ROOT/goose/cuda/goose-cuda-gdn/scalar-device.goose" > "$OUT/gdn.goose"
sed '/^fn main()/,$d; s/^fn gdn_v128(/export fn gdn_v128(/' \
    "$ROOT/goose/cuda/goose-cuda-gdn/mma-device.goose" > "$OUT/gdn-mma.goose"
sed -n '/^extern "fg176r_ld_f32"/p; /^extern "fg176r_st_f16"/p; /^extern "fg176r_st_u8"/p;
        /^extern "fg176r_fabs"/p; /^extern "fg176r_i8trunc"/p;
        /^fn fg176r_quant_q4_0(/,/^}/p' \
    "$ROOT/goose/cuda/goose-cuda-set-rows/device.goose" |
    sed 's/^fn fg176r_quant_q4_0(/export fn fg176r_quant_q4_0(/' > "$OUT/q4-set-rows.goose"
cp "$ROOT/goose/cuda/standalone-vector.goose" "$OUT/vector.goose"
sed -n '/^fn cv_offset(/,/^}/p' "$ROOT/goose/cuda/standalone-vector.goose" |
    sed 's/cv_offset/rms_offset/g; s/cv_word/rms_word/g' > "$OUT/rms-reference.goose"
sed -n '/^struct F4 {/,/^}/p; /^extern "fg168_\(load\|store\|load4\|store4\|shuffle\|barrier\|rsqrt\|fma\)"/p;
        /^fn warp(/,/^}/p; /^fn block_sum(/,/^}/p; /^fn rms_row(/,/^}/p' \
    "$ROOT/goose/cuda/goose-cuda-normalization/device.goose" |
    sed 's/\bF4\b/RmsVec/g; s/store(shared,/rms_shared_store(shared,/; s/load(shared,/rms_shared_load(shared,/' >> "$OUT/rms-reference.goose"
cat "$ROOT/goose/cuda/standalone-rms-reference.goose" >> "$OUT/rms-reference.goose"
sed -n '/^fn silu(/,/^}/p; /^fn sigmoid(/,/^}/p; /^fn softplus(/,/^}/p' \
    "$ROOT/goose/cpu/goose-cpu-backend-api/pointwise.goose" >> "$OUT/vector.goose"
cp "$ROOT/goose/cuda/standalone-rope.goose" "$OUT/rope.goose"
sed -n '/^fn yarn_pair(/,/^}/p; /^fn rope_sector_axis(/,/^}/p; /^fn rope_reset_axis(/,/^}/p' \
    "$ROOT/goose/cpu/goose-cpu-backend-api/rope.goose" >> "$OUT/rope.goose"
sed -n '/^extern "fg170s_load"/p; /^extern "fg170s_store"/p; /^extern "fg170s_expf"/p;
        /^extern "fg170s_lc"/p; /^extern "fg170s_sync"/p;
        /^fn silu(/,/^}/p; /^fn ssm_conv_serial(/,/^}/p' \
    "$ROOT/goose/cuda/goose-cuda-ssm-convolution/device.goose" |
    sed 's/^fn ssm_conv_serial(/export fn ssm_conv_serial(/; s/\bsilu\b/ssm_silu/g' > "$OUT/ssm-conv.goose"
cp "$ROOT/goose/cuda/standalone-routing.goose" "$OUT/routing.goose"
sed -n '/^fn warp_max(/,/^}/p' "$ROOT/goose/chat/goose-cuda-softmax/device.goose" >> "$OUT/routing.goose"
sed -n '/^fn cv_warp_sum(/,/^}/p' "$ROOT/goose/cuda/standalone-vector.goose" >> "$OUT/routing.goose"
sed '/^fn argsort(x/,$d; /^extern "fg283d_smem"/d' "$ROOT/goose/cuda/goose-cuda-argsort/device.goose" >> "$OUT/routing.goose"
sed -n '/^extern "fg169t_shuffle"/p; /^extern "fg169t_ishuffle"/p; /^struct RouteWinner /p;
        /^fn topk_winner(/,/^}/p' "$ROOT/goose/cuda/goose-cuda-reductions/topk/device-e8.goose" >> "$OUT/routing.goose"
cp "$ROOT/goose/cuda/standalone-transform.goose" "$OUT/transform.goose"
sed '/^fn main()/,$d; s/^fn fg183f_row(/export fn fg183f_row(/' \
    "$ROOT/goose/cuda/goose-cuda-fwht/device.goose" > "$OUT/fwht.goose"
sed '/^fn main()/,$d; s/^fn fg179w_reduce(/export fn fg179w_reduce(/' \
    "$ROOT/goose/cuda/goose-cuda-moe-weighted/device.goose" > "$OUT/moe-weighted.goose"
sed '/^fn main()/,$d; s/^fn fg180q_mmq(/export fn fg180q_mmq(/;
    s/^fn mmq_reciprocal(.*$/extern "fg180q_rcp" fn mmq_reciprocal(value: f32) -> f32;/' \
    "$ROOT/goose/cuda/goose-cuda-mmq-q8-1/device.goose" > "$OUT/mmq-quant.goose"
cp "$ROOT/goose/cuda/standalone-mmq.goose" "$OUT/mmq.goose"
cp "$ROOT/goose/cuda/standalone-dense-mma.goose" "$OUT/dense-mma.goose"
sed '/^fn main()/,$d' "$ROOT/goose/cuda/goose-cuda-flash-attention/value-prefill-device.goose" > "$OUT/flash-mma.goose"
cat "$ROOT/goose/cuda/standalone-flash-mma.goose" >> "$OUT/flash-mma.goose"
sed '/^fn load_mask(/,$d; s/^fn load_kv(/export fn fg_flash_load_kv(/' \
    "$ROOT/goose/cuda/goose-cuda-flash-attention/loaders-device.goose" > "$OUT/flash-load.goose"
cp "$ROOT/goose/cuda/standalone-flash-stage.goose" "$OUT/flash-stage.goose"
sed -n '/^fn scale_min(/,/^}/p' "$ROOT/goose/cuda/goose-cuda-dequant-q4k/device.goose" >> "$OUT/mmq.goose"
sed -n '/^fn q4_mmq_nibbles(/,/^}/p' "$ROOT/goose/cuda/goose-mmq-load-q4k/device.goose" >> "$OUT/mmq.goose"
sed -n '/^fn vsub4(/,/^}/p; /^fn q6_mmq_bytes(/,/^}/p' \
    "$ROOT/goose/cuda/goose-mmq-load-q6k/device.goose" >> "$OUT/mmq.goose"
for module in q4k q6k matmul q4-attention gdn gdn-mma q4-set-rows vector rms-reference rope ssm-conv routing transform fwht moe-weighted mmq-quant mmq dense-mma flash-mma flash-load flash-stage; do
    "$GOOSE" --standalone -o "$OUT/$module.c" --header "$OUT/$module.h" "$OUT/$module.goose"
    python3 - "$OUT" "$module" <<'PY'
from pathlib import Path
import sys

out, module = Path(sys.argv[1]), sys.argv[2]
source = (out / f"{module}.c").read_text()
fragment = source.split("/* ---- prototypes ---- */", 1)[1].split("static void gs_init_globals(void)", 1)[0]
if "gs_sp" in fragment:
    raise SystemExit(f"{module} uses the host stack ABI")
if module in ("q4-attention", "gdn", "rope", "ssm-conv", "fwht", "routing"):
    types = source.split("/* ---- types ---- */", 1)[1].split("/* ---- data ---- */", 1)[0]
    excluded = ("gs_rhdr;", "gs_rref;", "gs_pref;", "sl_u8;")
    if module == "routing":
        excluded += ("a2_f32;", "a8_f32;")
    types = "\n".join(line for line in types.splitlines()
                      if not line.endswith(excluded))
    fragment = types + "\n" + fragment
if module in ("gdn-mma", "flash-mma", "rms-reference"):
    types = source.split("/* ---- types ---- */", 1)[1].split("/* ---- data ---- */", 1)[0]
    types = "\n".join(line for line in types.splitlines()
                      if not line.endswith(("gs_rhdr;", "gs_rref;", "gs_pref;", "sl_u8;", "a8_f32;")))
    if module == "flash-mma":
        import re

        def prefix(text):
            return re.sub(r"\b(?:F[24]_g|U[24]_g|a\d+_(?:f32|u32))\b", lambda m: "Flash_" + m[0], text)

        types, fragment = prefix(types), prefix(fragment)
    (out / f"{module}-types.h").write_text(types)
(out / f"{module}-device.h").write_text(fragment)
PY
done
"$CLANG" -target nvptx64-nvidia-cuda -Xclang -target-cpu -Xclang sm_70 \
    -nocudalib -O2 -x cl -cl-std=CL1.2 -I "$OUT" -S \
    "$ROOT/integration/goose-standalone/cuda-device.cl" -o "$OUT/goose-cuda.ptx"
"$CLANG" -target nvptx64-nvidia-cuda -Xclang -target-cpu -Xclang sm_80 \
    -nocudalib -O2 -DGOOSE_CUDA_MMQ_IMAGE -x cl -cl-std=CL1.2 -I "$OUT" -S \
    "$ROOT/integration/goose-standalone/cuda-device.cl" -o "$OUT/goose-cuda-mmq.ptx"
python3 - "$OUT" <<'PY'
from pathlib import Path
import sys

out = Path(sys.argv[1])
def image(stem, symbol):
    ptx = (out / (stem + ".ptx")).read_bytes() + b"\0"
    lines = [", ".join(str(byte) for byte in ptx[i:i + 24]) for i in range(0, len(ptx), 24)]
    return "static const unsigned char " + symbol + "[] = {\n" + ",\n".join(lines) + "\n};\n"

(out / "goose-cuda-image.h").write_text(image("goose-cuda", "goose_cuda_image")
                                        + image("goose-cuda-mmq", "goose_cuda_mmq_image"))
PY
