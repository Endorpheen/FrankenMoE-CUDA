#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
goose=/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose
"$goose" --standalone -o inkling-online.c --header inkling-online.h inkling-online.goose
python3 - <<'INNER'
from pathlib import Path
s=Path("inkling-online.c").read_text()
body=s.split("/* ---- prototypes ---- */",1)[1].split("/* ---- host ---- */",1)[0]
assert "gs_sp" not in body and "i2_online_adjust_key_g" in body
types=s.split("/* ---- types ---- */",1)[1].split("/* ---- data ---- */",1)[0]
types="\n".join(line for line in types.splitlines() if not line.endswith(("gs_rhdr;","gs_rref;","gs_pref;","sl_u8;")))
Path("inkling-online-device.h").write_text(types+"\n"+body)
INNER
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off -nocudainc -nocudalib -Xclang -finclude-default-header -include device-types.h -Xclang -mlink-builtin-bitcode -Xclang /usr/local/cuda/nvvm/libdevice/libdevice.10.bc -S flash-mma.cl -o flash-native.ptx
/usr/local/cuda/bin/ptxas -v -arch=sm_89 flash-native.ptx -o flash-native.cubin > ptxas.log 2>&1
xxd -i flash-native.cubin > flash-native-image.h
root=/home/end0/AI/FrankenMoE-CUDA-main
native=../flash-prefill/native-runtime
frozen=../../../checkpoint-apex-exp1880-before-inkling/build-inputs
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration -ffunction-sections -fdata-sections
 -I . -I ../inkling-grouped-moe-mmq -I ../inkling-resident-moe -I ../inkling-attention-device-input
 -I ../inkling-q4-kv -I ../inkling-cuda-tanh -I ../inkling-completion -I ../../slru-prototype -I "$native"
 -I "$root/vendor/flint/ggml/include" -I "$root/vendor/flint/ggml/src" -I "$root/integration/goose-standalone" -I "$root/integration/m1/host")
gcc-13 "${flags[@]}" -I "$frozen" -c cuda-driver.c -o cuda-driver.o
cp --reflink=auto ../inkling-cumulative-shared-attention/libfranken-inference.a .
ar r libfranken-inference.a cuda-driver.o
gcc-13 "${flags[@]}" -Wl,--gc-sections ../inkling-completion/infer-main.c numeric.o libfranken-inference.a -lm -ldl -pthread -o frankenmoe-infer
