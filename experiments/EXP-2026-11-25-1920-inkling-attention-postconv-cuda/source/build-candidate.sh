#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off -nocudainc -nocudalib -Xclang -finclude-default-header -S attention-postconv.cl -o attention-postconv.ptx
/usr/local/cuda/bin/ptxas -v -arch=sm_89 attention-postconv.ptx -o attention-postconv.cubin > ptxas.log 2>&1
xxd -i attention-postconv.cubin > attention-postconv-image.h
root=/home/end0/AI/FrankenMoE-CUDA-main
native=../flash-prefill/native-runtime
frozen=../../../checkpoint-apex-exp1880-before-inkling/build-inputs
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration -ffunction-sections -fdata-sections
 -I . -I ../inkling-grouped-moe-mmq -I ../inkling-resident-moe -I ../inkling-attention-device-input
 -I ../inkling-q4-kv -I ../inkling-cuda-tanh -I ../inkling-completion -I ../../slru-prototype -I "$native"
 -I "$root/vendor/flint/ggml/include" -I "$root/vendor/flint/ggml/src"
 -I "$root/integration/goose-standalone" -I "$root/integration/m1/host")
gcc-13 "${flags[@]}" -I "$frozen" -c cuda-driver.c -o cuda-driver.o
gcc-13 "${flags[@]}" -c graph-execute.c -o graph-execute.o
gcc-13 "${flags[@]}" -c inkling-backend.c -o inkling-backend.o
cp --reflink=auto ../inkling-prefill-postconv-batch/libfranken-inference.a .
ar r libfranken-inference.a cuda-driver.o inkling-backend.o graph-execute.o
gcc-13 "${flags[@]}" -Wl,--gc-sections ../inkling-completion/infer-main.c numeric.o libfranken-inference.a  -lm -ldl -pthread -o frankenmoe-infer
