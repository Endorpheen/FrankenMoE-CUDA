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
cat "$ROOT/goose/cuda/"{standalone-q4-attention,standalone-mla-launch}.goose >> "$OUT/q4-attention.goose"
sed '/^fn main()/,$d; s/^fn \(gdn\|gdn_decode\)(/export fn \1(/' \
    "$ROOT/goose/cuda/goose-cuda-gdn/scalar-device.goose" > "$OUT/gdn.goose"
sed '/^fn main()/,$d; s/^fn \(gdn_v32\|gdn_v128\)(/export fn \1(/' \
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
        /^extern "fg170s_lc"/p; /^extern "fg170s_sync"/p; /^extern "fg170s_barrier"/p;
        /^fn silu(/,/^}/p; /^fn ssm_conv_serial(/,/^}/p; /^fn ssm_conv_decode(/,/^}/p;
        /^fn ssm_conv_long(/,/^}/p; /^fn ssm_conv_direct(/,/^}/p; /^fn ssm_conv_save_tail(/,/^}/p' \
    "$ROOT/goose/cuda/goose-cuda-ssm-convolution/device.goose" |
    sed 's/^fn ssm_conv_/export fn ssm_conv_/; s/\bsilu\b/ssm_silu/g' > "$OUT/ssm-conv.goose"
cat >> "$OUT/ssm-conv.goose" <<'GOOSE_SOURCE'
extern "fg170s_shared_load" fn conv_shared_load(address: u64, index: u64) -> f32;
extern "fg170s_shared_store" fn conv_shared_store(address: u64, index: u64, value: f32);
extern "fg170s_fma" fn ssm_long_acc(x: f32, weight: f32, sum: f32) -> f32;
extern "fg_device_reciprocal" fn ssm_long_reciprocal(value: f32) -> f32;
fn ssm_long_silu(x: f32) -> f32 { return x * ssm_long_reciprocal(1.0 + expf(-x)); }
GOOSE_SOURCE
cat "$ROOT/goose/cuda/standalone-ssm-launch.goose" >> "$OUT/ssm-conv.goose"
cp "$ROOT/goose/cuda/standalone-routing.goose" "$OUT/routing.goose"
sed -n '/^fn warp_max(/,/^}/p' "$ROOT/goose/chat/goose-cuda-softmax/device.goose" >> "$OUT/routing.goose"
sed -n '/^fn cv_warp_sum(/,/^}/p' "$ROOT/goose/cuda/standalone-vector.goose" >> "$OUT/routing.goose"
sed '/^fn argsort(x/,$d; /^extern "fg283d_smem"/d' "$ROOT/goose/cuda/goose-cuda-argsort/device.goose" >> "$OUT/routing.goose"
sed -n '/^extern "fg169t_shuffle"/p; /^extern "fg169t_ishuffle"/p; /^struct RouteWinner /p;
        /^fn topk_winner(/,/^}/p' "$ROOT/goose/cuda/goose-cuda-reductions/topk/device-e8.goose" >> "$OUT/routing.goose"
sed '/^fn transpose(/,$d' "$ROOT/goose/cuda/cpy-transpose/device.goose" > "$OUT/transform.goose"
cat "$ROOT/goose/cuda/standalone-transform.goose" >> "$OUT/transform.goose"
sed '/^fn main()/,$d; s/^fn fg183f_row(/export fn fg183f_row(/' \
    "$ROOT/goose/cuda/goose-cuda-fwht/device.goose" > "$OUT/fwht.goose"
cat "$ROOT/goose/cuda/standalone-fwht-q4.goose" >> "$OUT/fwht.goose"
sed '/^fn main()/,$d; s/^fn fg179w_reduce(/export fn fg179w_reduce(/' \
    "$ROOT/goose/cuda/goose-cuda-moe-weighted/device.goose" > "$OUT/moe-weighted.goose"
sed '/^fn main()/,$d; s/^fn fg180q_mmq(/export fn fg180q_mmq(/;
    s/^fn mmq_reciprocal(.*$/extern "fg_device_reciprocal" fn mmq_reciprocal(value: f32) -> f32;/' \
    "$ROOT/goose/cuda/goose-cuda-mmq-q8-1/device.goose" > "$OUT/mmq-quant.goose"
cat "$ROOT/goose/cuda/standalone-mmq-glu.goose" >> "$OUT/mmq-quant.goose"
cp "$ROOT/goose/cuda/standalone-mmq.goose" "$OUT/mmq.goose"
sed -n '/^extern "fg185q_\(ld_\|dp4a\|shfl_xor"\)/p;
    /^fn warp_sum(/,/^}/p; /^fn impl_vmmq(/,/^}/p; /^fn dot_row(/,/^}/p; /^fn fg185q_partial(/,/^}/p' \
    "$ROOT/goose/cuda/goose-cuda-mmvq-q4k/device.goose" |
    sed 's/^fn \(warp_sum\|fg185q_partial\)(/export fn \1(/; s/\bdot_row\b/mmvq_q4_dot_row/g' > "$OUT/mmvq-q4.goose"
sed -n '/^extern "fg186q_\(ld_\|dp4a\)/p;
    /^fn ld_b2(/,/^}/p; /^fn impl_mmvq(/,/^}/p; /^fn dot_row(/,/^}/p; /^fn fg186q_partial(/,/^}/p' \
    "$ROOT/goose/cuda/goose-cuda-mmvq-q6k/device.goose" |
    sed 's/^fn fg186q_partial(/export fn fg186q_partial(/; s/\bdot_row\b/mmvq_q6_dot_row/g' > "$OUT/mmvq-q6.goose"
sed '/^fn main()/,$d; s/^fn fg178q_quant(/export fn fg178q_quant(/;
    s/\bFastDiv\b/MmvqFastDiv/g; s/\b\(ensure\|red_max\|red_sum\)\b/mmvq_pack_\1/g' \
    "$ROOT/goose/cuda/goose-cuda-quantize/device.goose" > "$OUT/mmvq-pack.goose"
cat "$ROOT/goose/cuda/standalone-mmvq-pack-launch.goose" >> "$OUT/mmvq-pack.goose"
cat "$ROOT/goose/cuda/"standalone-mmvq{,-launch}.goose > "$OUT/mmvq.goose"
cp "$ROOT/goose/cuda/standalone-dense-mma.goose" "$OUT/dense-mma.goose"
sed '/^fn main()/,$d' "$ROOT/goose/cuda/goose-cuda-flash-attention/value-prefill-device.goose" > "$OUT/flash-mma.goose"
cat "$ROOT/goose/cuda/standalone-flash-mma.goose" >> "$OUT/flash-mma.goose"
sed -n '/^fn ftz_thresh(/,/^}/p; /^extern "fg168f_\(ld_f32\|st_f32\|sync\|lc\|expf\|fmaxf\)"/p;
        /^fn fixup_uniform(/,/^}/p; /^fn fixup_general(/,/^}/p' \
    "$ROOT/goose/cuda/goose-cuda-flash-attention/mask-fixups-device.goose" |
    sed 's/^fn fixup_/export fn flash_fixup_/;
         s/\b\(ftz_thresh\|ld_f32\|st_f32\|pdl_sync\|lc\|expf\|fmaxf\)\b/flash_fix_\1/g' >> "$OUT/flash-mma.goose"
sed '/^fn fattn_vec_2(/,/^fn combine(/ { /^fn combine(/!d; }
    /^fn main()/,$d
    s/^fn \(fattn_vec_1\|combine\|pack_query\)(/export fn \1(/
    s/\b\(negmax\|absf\|wr_sum\|wr_max\|alibi_slope\|q4_word\|vec_dot_kq4\|dequant_v4\|quant_q8\|quant_q8_head\|pack_query\|fattn_vec_1\|combine\)\b/fv_\1/g
    s/\bV4\b/VecFlash4/g' \
    "$ROOT/goose/cuda/goose-cuda-flash-attention/vector-device.goose" > "$OUT/flash-vec.goose"
cat "$ROOT/goose/cuda/standalone-flash-vec-launch.goose" >> "$OUT/flash-vec.goose"
sed '/^fn load_mask(/,$d; s/^fn load_kv(/export fn fg_flash_load_kv(/' \
    "$ROOT/goose/cuda/goose-cuda-flash-attention/loaders-device.goose" > "$OUT/flash-load.goose"
cp "$ROOT/goose/cuda/standalone-flash-stage.goose" "$OUT/flash-stage.goose"
sed -n '/^fn scale_min(/,/^}/ { s/^fn scale_min(/fn mmq_scale_min(/; p; }' \
    "$ROOT/goose/cuda/goose-cuda-dequant-q4k/device.goose" >> "$OUT/mmq.goose"
sed -n '/^fn q4_mmq_nibbles(/,/^}/p' "$ROOT/goose/cuda/goose-mmq-load-q4k/device.goose" >> "$OUT/mmq.goose"
sed -n '/^fn vsub4(/,/^}/p; /^fn q6_mmq_bytes(/,/^}/p' \
    "$ROOT/goose/cuda/goose-mmq-load-q6k/device.goose" >> "$OUT/mmq.goose"
cat "$ROOT/goose/cuda/standalone-mmq-launch.goose" >> "$OUT/mmq.goose"
cat "$ROOT/goose/cuda/standalone-mmq-pack-launch.goose" >> "$OUT/mmq-quant.goose"
for module in gdn gdn-mma q4-set-rows flash-mma; do
    cat "$ROOT/goose/cuda/standalone-$module-launch.goose" >> "$OUT/$module.goose"
done
for module in q4k q6k matmul q4-attention gdn gdn-mma q4-set-rows vector rms-reference rope ssm-conv routing transform fwht moe-weighted mmq-quant mmq mmvq-q4 mmvq-q6 mmvq-pack mmvq dense-mma flash-mma flash-vec flash-load flash-stage; do
    "$GOOSE" --standalone -o "$OUT/$module.c" --header "$OUT/$module.h" "$OUT/$module.goose"
    python3 - "$OUT" "$module" <<'PY'
from pathlib import Path
import sys

out, module = Path(sys.argv[1]), sys.argv[2]
source = (out / f"{module}.c").read_text()
fragment = source.split("/* ---- prototypes ---- */", 1)[1].split("static void gs_init_globals(void)", 1)[0]
if "gs_sp" in fragment:
    raise SystemExit(f"{module} uses the host stack ABI")
if module == "gdn":
    import re

    # Keep launch-shape literals visible to Clang when lowering the device loops.
    fragment = re.sub(r"static void (gdn(?:_body|_decode)?_g\()",
                      r"static __attribute__((always_inline)) void \1", fragment)
    fragment = re.sub(r"^(\s*)(for \(int32_t r_g(?:_\d+)? = 0;)",
                      r"\1#pragma clang loop unroll(full)\n\1\2", fragment, flags=re.MULTILINE)
if module == "vector":
    # Inlining keeps the by-value shape plan in parameter space. Passing its
    # address through a device call would copy the plan to per-thread memory.
    fragment = fragment.replace("static GS_NOINLINE void cv_norm_g(",
                                "static __attribute__((always_inline)) void cv_norm_g(")
if module == "q4-attention":
    fragment = fragment.replace("static void giga_mla_mma_pv_tile_g(",
                                "static __attribute__((always_inline)) void giga_mla_mma_pv_tile_g(")
if module == "q4-attention":
    fragment = fragment.replace("static void giga_mla_mma_pv_indexed_g(",
                                "static __attribute__((always_inline)) void giga_mla_mma_pv_indexed_g(")
if module == "mmq":
    fragment = fragment.replace("static void fg_device_mmq_q4_large_g(",
                                "static __attribute__((always_inline)) void fg_device_mmq_q4_large_g(")
if module in ("q4-attention", "gdn", "rope", "ssm-conv", "fwht", "routing"):
    types = source.split("/* ---- types ---- */", 1)[1].split("/* ---- data ---- */", 1)[0]
    excluded = ("gs_rhdr;", "gs_rref;", "gs_pref;", "sl_u8;")
    if module == "routing":
        excluded += ("a2_f32;", "a8_f32;")
    types = "\n".join(line for line in types.splitlines()
                      if not line.endswith(excluded))
    fragment = types + "\n" + fragment
if module in ("gdn-mma", "flash-mma", "flash-vec", "rms-reference"):
    types = source.split("/* ---- types ---- */", 1)[1].split("/* ---- data ---- */", 1)[0]
    excluded = ("gs_rhdr;", "gs_rref;", "gs_pref;", "sl_u8;")
    if module != "flash-vec":
        excluded += ("a8_f32;",)
    types = "\n".join(line for line in types.splitlines()
                      if not line.endswith(excluded))
    if module == "gdn-mma":
        import re

        def gdn_prefix(text):
            return re.sub(r"\ba(?:4|16)_f32\b", lambda m: "GdnMma_" + m[0], text)

        types, fragment = gdn_prefix(types), gdn_prefix(fragment)
    if module == "flash-mma":
        import re

        def prefix(text):
            return re.sub(r"\b(?:F[24]_g|U[24]_g|a\d+_(?:f32|u32))\b", lambda m: "Flash_" + m[0], text)

        types, fragment = prefix(types), prefix(fragment)
    if module == "flash-vec":
        import re

        def vector_prefix(text):
            return re.sub(r"\ba\d+_(?:f32|i32|u32)\b", lambda m: "FlashVec_" + m[0], text)

        types, fragment = vector_prefix(types), vector_prefix(fragment)
    (out / f"{module}-types.h").write_text(types)
(out / f"{module}-device.h").write_text(fragment)
if module == "mmq-quant":
    types = source.split("/* ---- types ---- */", 1)[1].split("/* ---- data ---- */", 1)[0]
    start = types.index("typedef struct MmqQ8Result_g ")
    end = types.index("};", start) + 2
    (out / "mmq-quant-types.h").write_text(types[start:end] + "\n")
PY
done
sed -i 's/\ba16_f32\b/mla_a16_f32/g' "$OUT/q4-attention-device.h"
"$CLANG" -target nvptx64-nvidia-cuda -Xclang -target-cpu -Xclang sm_70 \
    -nocudalib -O2 -x cl -cl-std=CL1.2 -I "$OUT" -S \
    "$ROOT/integration/goose-standalone/cuda-device.cl" -o "$OUT/goose-cuda.ptx"
"$CLANG" -target nvptx64-nvidia-cuda -Xclang -target-cpu -Xclang sm_80 \
    -nocudalib -O2 -DGOOSE_CUDA_MMQ_IMAGE -x cl -cl-std=CL1.2 -I "$OUT" -S \
    "$ROOT/integration/goose-standalone/cuda-device.cl" -o "$OUT/goose-cuda-mmq.ptx"
python3 - "$OUT" <<'PY'
from pathlib import Path
import re
import sys

out = Path(sys.argv[1])
argument_counts = {}

def image(stem, symbol):
    ptx = (out / (stem + ".ptx")).read_bytes() + b"\0"
    signatures = re.findall(rb"(?m)^\s*(?:\.visible\s+)?\.entry\s+(\w+)\s*\(([^)]*)\)", ptx)
    if not signatures:
        raise ValueError(f"no kernel signatures in {stem}")
    for kernel, params in signatures:
        count = len(re.findall(rb"\.param\b", params))
        if argument_counts.setdefault(kernel.decode("ascii"), count) != count:
            raise ValueError(f"different argument counts for {kernel!r}")
    lines = [", ".join(str(byte) for byte in ptx[i:i + 24]) for i in range(0, len(ptx), 24)]
    return "static const unsigned char " + symbol + "[] = {\n" + ",\n".join(lines) + "\n};\n"

images = image("goose-cuda", "goose_cuda_image") + image("goose-cuda-mmq", "goose_cuda_mmq_image")
counts = "".join(f'    {{"{kernel}", {count}}},\n' for kernel, count in sorted(argument_counts.items()))
(out / "goose-cuda-image.h").write_text(
    images + "#define GOOSE_CUDA_HAS_ARG_COUNTS 1\n"
    "static const struct { const char *name; unsigned count; } goose_cuda_arg_counts[] = {\n"
    + counts + "};\n"
)
PY
