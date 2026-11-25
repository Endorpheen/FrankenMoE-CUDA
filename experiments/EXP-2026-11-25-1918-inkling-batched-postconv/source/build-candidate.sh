#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
compiler=/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose
"$compiler" --standalone -o "$PWD/numeric.c" --header "$PWD/numeric.h" "$PWD/numeric.goose"
gcc-13 -O1 -std=c11 -ffunction-sections -fdata-sections -Dgoose_init=i2_candidate_init  -Dgg_init_globals=i2_candidate_globals -w -c numeric.c -o numeric.o
root=/home/end0/AI/FrankenMoE-CUDA-main
flags=(-O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration -ffunction-sections -fdata-sections
 -I . -I ../inkling-resident-moe -I ../inkling-completion -I "$root/vendor/flint/ggml/include" -I "$root/vendor/flint/ggml/src"
 -I "$root/integration/goose-standalone" -I "$root/integration/m1/host")
gcc-13 "${flags[@]}" -c inkling-backend.c -o inkling-backend.o
cp --reflink=auto ../inkling-moe-tile16/libfranken-inference.a .
ar r libfranken-inference.a inkling-backend.o
gcc-13 "${flags[@]}" -Wl,--gc-sections ../inkling-completion/infer-main.c numeric.o libfranken-inference.a  -lm -ldl -pthread -o frankenmoe-infer
