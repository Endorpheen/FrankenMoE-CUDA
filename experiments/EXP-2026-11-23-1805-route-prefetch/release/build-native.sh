#!/usr/bin/env bash
# Build the Linux and Windows chat hosts on the cumulative EXP-1805 runtime.
# Linux links the composed Goose unit, the chat CLI and web host against the
# accepted EXP-1805 libfranken-inference.a (byte-reproduced checkpoint).
# Windows follows build/chat-alpha-tool-recovery/build-native.sh: it reuses the
# windows-alpha-2 MinGW objects whose Linux counterparts are byte-identical in
# the EXP-1721 and EXP-1786 archives, and rebuilds the members that changed
# after EXP-1721 from three-way merged sources (speed line + Windows guards).
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=/home/end0/AI/claude-apex-100/repo
alpha=/home/end0/.codex/worktrees/frankenmoe-windows-alpha/FrankenMoE-CUDA
inputs=/home/end0/.codex/worktrees/pure-goose-inkling-mini/FrankenMoE-CUDA/build/exp1786-split-planes/build-inputs
checkpoint=/home/end0/AI/claude-apex-100/checkpoints/exp1805-route-prefetch
history=/home/end0/.codex/worktrees/pure-goose-inkling-mini/FrankenMoE-CUDA/build/exp1786-source-repro/history
planner=/home/end0/.codex/worktrees/pure-goose-inkling-mini/FrankenMoE-CUDA/build/exp1744-slru-one-touch/planner.c
merge="$here/merge"
unit="$here/linux"
src="$root/integration/goose-standalone"
common=(-O1 -std=c11 -D_GNU_SOURCE -ffunction-sections -fdata-sections -I "$unit"
        -I "$src" -I "$root/integration/m1/host" -I "$root/vendor/flint/ggml/include"
        -I "$root/vendor/flint/ggml/src" -I "$root/vendor/flint/ggml/src/ggml-cpu")
case "${1:?platform}" in
linux)
    out="$here/linux"
    (cd "$checkpoint" && sha256sum -c --quiet SHA256SUMS)
    gcc-13 "${common[@]}" -w -c "$unit/main.c" -o "$out/main-linux.o"
    gcc-13 "${common[@]}" -DFG_CHAT_ALPHA -c "$src/cli/infer-main.c" -o "$out/infer-linux.o"
    gcc-13 "${common[@]}" -c "$src/windows/web.c" -o "$out/web-linux.o"
    gcc-13 -Wl,--gc-sections "$out"/{infer-linux,main-linux,web-linux}.o "$checkpoint/libfranken-inference.a" \
        -lm -ldl -pthread -o "$out/frankenmoe-server"
    sha256sum "$out/frankenmoe-server"
    ;;
windows)
    base="$alpha/build/windows-alpha-2"
    stage="$here/windows"
    mkdir -p "$stage/inc"
    cc=x86_64-w64-mingw32-gcc-posix
    flags=("${common[@]}" -D_WIN32_WINNT=0x0a00 -DWIN32_LEAN_AND_MEAN)
    # Image headers of the cumulative CUDA driver, then the prefetch kernel image.
    cp "$inputs"/*-image.h "$inputs"/goose-cuda-image.h "$inputs"/cpu.h "$stage/inc/"
    cp "$checkpoint/src/pred-image.h" "$stage/inc/"
    cp "$merge/merged.c" "$stage/inc/cuda-driver.c"
    for obj in cpu graph-bindings libc-forwards slru-shim goose-host ggml ggml-quants ggml-system \
               probability probability-pipeline platform; do
        cp -a "$base/$obj.o" "$stage/$obj.o"
    done
    "$cc" "${flags[@]}" -w -c "$unit/main.c" -o "$stage/main-windows.o"
    "$cc" "${flags[@]}" -I "$stage/inc" -I "$inputs" -c "$stage/inc/cuda-driver.c" -o "$stage/cuda-driver.o"
    "$cc" "${flags[@]}" -I "$inputs" -c "$merge/graph-execute.merged.c" -o "$stage/graph-execute.o"
    "$cc" "${flags[@]}" -I "$inputs" -c "$merge/goose-host-llm.merged.c" -o "$stage/model-host.o"
    "$cc" "${flags[@]}" -c "$history/sampler-scan.c" -o "$stage/sampler-scan.o"
    "$cc" "${flags[@]}" -w -Dgoose_init=exp1744_init -Dgg_init_globals=exp1744_init_globals \
        -Dec_prepare=exp1744_prepare_unused -Dec_prepare_compact=exp1744_compact_unused \
        -Dec_reset=exp1744_reset_unused -Dslru_plan=exp1744_slru_unused -c "$planner" -o "$stage/planner-link.o"
    "$cc" "${flags[@]}" -DFG_CHAT_ALPHA -c "$src/cli/infer-main.c" -o "$stage/infer-main.o"
    "$cc" "${flags[@]}" -c "$src/windows/web.c" -o "$stage/web.o"
    "$cc" -municode -static -Wl,--gc-sections \
        "$stage"/{infer-main,main-windows,cpu,graph-execute,cuda-driver,graph-bindings,model-host,libc-forwards,slru-shim,goose-host,ggml,ggml-quants,ggml-system,probability,probability-pipeline,platform,web,sampler-scan,planner-link}.o \
        -lws2_32 -lpsapi -lwinpthread -o "$stage/frankenmoe-server.exe"
    cp -a "$base/frankenmoe-launcher.exe" "$stage/"
    sha256sum "$stage/frankenmoe-server.exe"
    ;;
*) exit 2 ;;
esac
