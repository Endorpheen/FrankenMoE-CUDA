#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
device=../../../exp1887-attention-q4-loads/device
source="$device/glue/cuda-device.cl"
# Extract the frozen bindings used by the Goose routing unit.
awk 'NR < 117 {if (NR < 2 || NR > 8) print; next}
    (/^static (void|float|int32_t|int64_t|double) (fg_device_barrier|fg_device_global_load|fg_device_global_store|fg_device_shfl|fg_attn_word|fg_attn_exp|fg_attn_neg_inf|fg_rope_float|fg_rope_position|fg248d_fail|fg_vector_max|fg_vector_shuffle_f64|fg283d_ld_f32|fg283d_st_i32|fg283d_sload_i32|fg283d_sstore_i32|fg283d_sync|fg283d_fail|fg169t_shuffle|fg169t_ishuffle)\(/) { on=1 }
    on { print }
    NR >= 117 && (/^}/ || (/\{/ && /}/)) { on=0 }' "$source" > route-unit.cl
printf '#define UINT64_C(x) x##UL\n#define INT64_C(x) x##L\n#include "routing-device.h"\n' >> route-unit.cl
cat route-mailbox.cl >> route-unit.cl
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off -nocudainc -nocudalib -include "$device/device-prelude.cl" -Xclang -finclude-default-header -I "$device" -S route-unit.cl -o route-mailbox.ptx
/usr/local/cuda/bin/ptxas -v -arch=sm_89 route-mailbox.ptx -o route-mailbox.cubin > ptxas.log 2>&1
xxd -i route-mailbox.cubin > inkling-route-mailbox-image.h
