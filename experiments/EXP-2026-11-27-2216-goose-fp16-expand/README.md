# EXP-2216: Expand FP16 in Goose

The universal CPU path called a C adapter before GGML could expand a half.
The candidate computes the result in Goose and uses the existing bitcast
adapter for the resulting binary32 representation. The scalar reference
path in the probe calls the original `fg143_half` adapter from EXP-2215.

With Fried Goose v0.3.0-preview-keelwing and GCC 13, all 65,536 binary16
patterns matched the GGML row reference bit for bit. The A-B-B-A block on
normal inputs measured 2.514, 1.855, 1.845, and 2.432 ns/value. The kernel
adds no persistent allocation or GPU memory. No full model run was performed.

Use the compiler from the universal CPU build and select a temporary output
directory. Set `EXP2215_GRAPH_OBJECT` to the reference graph-execute object
built by build-gooseinfer-inkling-host.sh at c9e8fb55 and `REFERENCE_COMMON`
to the directory containing the common GGML objects from that build.

```bash
probe=experiments/EXP-2026-11-27-2216-goose-fp16-expand
out=$(mktemp -d)
cat integration/goose-standalone/universal/cpu/codec/half.goose \
    "$probe/probe-entry.goose" >"$out/bench.goose"
"$GOOSE_BIN" --standalone -o "$out/bench.c" --header "$out/bench.h" "$out/bench.goose"
gcc-13 -O1 -D_GNU_SOURCE -ffunction-sections -fdata-sections \
    -I"$out" -Ivendor/flint/ggml/include -Wl,--gc-sections \
    "$probe/probe.c" "$out/bench.c" "$EXP2215_GRAPH_OBJECT" \
    "$REFERENCE_COMMON"/{ggml,ggml-system}.o -lm -ldl -pthread -o "$out/probe"
"$out/probe"
```

The retained GGML text decreased by 84 bytes. The scalar entry point was
removed by the linker. The row conversion remains reachable.
