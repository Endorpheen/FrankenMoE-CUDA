#!/usr/bin/env bash
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$R/builds/flint-acc/bin
D=$(dirname "$0")
BIN=/tmp/five_doors
g++-13 -O2 -std=c++17 -w "$D/diff.cpp" -I "$R/vendor/flint/include" -I "$R/vendor/flint/ggml/include" \
    -L "$LIB" -lllama -Wl,-rpath,"$LIB" -o "$BIN" || exit 1
ON="GGML_GOOSE_SAMPLER_CHAIN=1 GGML_GOOSE_SAMPLER_PENALTIES=1 GGML_GOOSE_SAMPLER_TYPICAL=1 GGML_GOOSE_SAMPLER_XTC=1"
OFF="GGML_GOOSE_SAMPLER_CHAIN=0 GGML_GOOSE_SAMPLER_PENALTIES=0 GGML_GOOSE_SAMPLER_TYPICAL=0 GGML_GOOSE_SAMPLER_XTC=0"
env $ON "$BIN" /tmp/five_goose.txt >/dev/null 2>/tmp/five_goose.log || exit 1
env $OFF "$BIN" /tmp/five_native.txt >/dev/null 2>&1 || exit 1
fails=0
for arm in penalties typical xtc mirostat adaptive; do
    g=$(awk -v a="$arm" '$1==a {on=1; print; next} /^[a-z]/ {on=0} on' /tmp/five_goose.txt | md5sum)
    n=$(awk -v a="$arm" '$1==a {on=1; print; next} /^[a-z]/ {on=0} on' /tmp/five_native.txt | md5sum)
    if [ "$g" = "$n" ]; then echo "$arm: identical"; else
        echo "$arm: MISMATCH"; fails=$((fails+1))
        diff <(awk -v a="$arm" '$1==a {on=1; print; next} /^[a-z]/ {on=0} on' /tmp/five_goose.txt) \
             <(awk -v a="$arm" '$1==a {on=1; print; next} /^[a-z]/ {on=0} on' /tmp/five_native.txt) | head -4
    fi
done
exit $([ $fails -eq 0 ] && echo 0 || echo 1)
