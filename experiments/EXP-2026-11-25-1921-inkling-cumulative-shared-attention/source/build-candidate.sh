#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
root=/home/end0/AI/FrankenMoE-CUDA-main
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration -ffunction-sections -fdata-sections
 -I . -I ../inkling-resident-moe -I ../inkling-completion -I "$root/vendor/flint/ggml/include" -I "$root/vendor/flint/ggml/src"
 -I "$root/integration/goose-standalone" -I "$root/integration/m1/host")
gcc-13 "${flags[@]}" -c inkling-backend.c -o inkling-backend.o
cp --reflink=auto ../inkling-prefill-postconv-batch/libfranken-inference.a .
ar r libfranken-inference.a inkling-backend.o
gcc-13 "${flags[@]}" -Wl,--gc-sections ../inkling-completion/infer-main.c numeric.o libfranken-inference.a  -lm -ldl -pthread -o frankenmoe-infer
