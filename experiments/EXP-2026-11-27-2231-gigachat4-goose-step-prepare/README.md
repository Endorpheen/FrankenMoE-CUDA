# EXP-2231: Prepare GigaChat4 graph inputs in Goose

Goose validates the step layout, rounds the mask width, and creates input
descriptors in the original order. The C adapter marshals those descriptors
into the existing step structure. Invalid inputs do not allocate descriptors.

The exhaustive small-shape probe passed 58,500 cases including invalid
counts, both sides of the padding boundary and optional output indices.
Both universal binaries linked. Inference speed and peak memory are pending.

```bash
out=$(mktemp -d)
src=integration/goose-standalone/gigachat4
probe=experiments/EXP-2026-11-27-2231-gigachat4-goose-step-prepare/probe.c
sed -n '/^extern "ggml_new_tensor_4d"/p' "$src/dsa-indexer.goose" >"$out/probe.goose"
cat "$src/step-inputs.goose" >>"$out/probe.goose"
"$GOOSE_BIN" --standalone -o "$out/probe-gen.c" --header "$out/probe.h" "$out/probe.goose"
gcc-15 -O2 -I"$out" -Ivendor/flint/ggml/include -Wl,--gc-sections \
    "$probe" "$out/probe-gen.c" "$COMMON_CORE"/{ggml,ggml-system}.o -lm -pthread -o "$out/probe"
"$out/probe"
```

Set `COMMON_CORE` to the Linux output of `build-gooseinfer-common-core.sh`.
