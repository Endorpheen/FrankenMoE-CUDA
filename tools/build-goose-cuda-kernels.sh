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
    "$ROOT/goose/cuda/goose-cuda-flash-attention/staging-device.goose" > "$OUT/q4-attention.goose"
cat "$ROOT/goose/cuda/standalone-q4-attention.goose" >> "$OUT/q4-attention.goose"
sed '/^fn main()/,$d; s/^fn gdn(/export fn gdn(/' \
    "$ROOT/goose/cuda/goose-cuda-gdn/scalar-device.goose" > "$OUT/gdn.goose"
sed -n '/^extern "fg176r_ld_f32"/p; /^extern "fg176r_st_f16"/p; /^extern "fg176r_st_u8"/p;
        /^extern "fg176r_fabs"/p; /^extern "fg176r_i8trunc"/p;
        /^fn fg176r_quant_q4_0(/,/^}/p' \
    "$ROOT/goose/cuda/goose-cuda-set-rows/device.goose" |
    sed 's/^fn fg176r_quant_q4_0(/export fn fg176r_quant_q4_0(/' > "$OUT/q4-set-rows.goose"
cp "$ROOT/goose/cuda/standalone-vector.goose" "$OUT/vector.goose"
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
cp "$ROOT/goose/cuda/standalone-transform.goose" "$OUT/transform.goose"
sed '/^fn main()/,$d; s/^fn fg180q_mmq(/export fn fg180q_mmq(/' \
    "$ROOT/goose/cuda/goose-cuda-mmq-q8-1/device.goose" > "$OUT/mmq-quant.goose"
cp "$ROOT/goose/cuda/standalone-mmq.goose" "$OUT/mmq.goose"
sed -n '/^fn scale_min(/,/^}/p' "$ROOT/goose/cuda/goose-cuda-dequant-q4k/device.goose" >> "$OUT/mmq.goose"
sed -n '/^fn q4_mmq_nibbles(/,/^}/p' "$ROOT/goose/cuda/goose-mmq-load-q4k/device.goose" >> "$OUT/mmq.goose"
sed -n '/^fn vsub4(/,/^}/p; /^fn q6_mmq_bytes(/,/^}/p' \
    "$ROOT/goose/cuda/goose-mmq-load-q6k/device.goose" >> "$OUT/mmq.goose"
for module in q4k q6k matmul q4-attention gdn q4-set-rows vector rope ssm-conv routing transform mmq-quant mmq; do
    "$GOOSE" --standalone -o "$OUT/$module.c" --header "$OUT/$module.h" "$OUT/$module.goose"
    python3 - "$OUT" "$module" <<'PY'
from pathlib import Path
import sys

out, module = Path(sys.argv[1]), sys.argv[2]
source = (out / f"{module}.c").read_text()
fragment = source.split("/* ---- prototypes ---- */", 1)[1].split("static void gs_init_globals(void)", 1)[0]
if "gs_sp" in fragment:
    raise SystemExit(f"{module} uses the host stack ABI")
if module in ("q4-attention", "gdn", "rope", "ssm-conv"):
    types = source.split("/* ---- types ---- */", 1)[1].split("/* ---- data ---- */", 1)[0]
    types = "\n".join(line for line in types.splitlines()
                      if not line.endswith(("gs_rhdr;", "gs_rref;", "gs_pref;", "sl_u8;")))
    fragment = types + "\n" + fragment
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
