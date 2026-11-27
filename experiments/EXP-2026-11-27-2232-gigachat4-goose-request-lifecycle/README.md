# EXP-2232: Allocate and clear GigaChat4 request state in Goose

The Goose request lifecycle computes the arena budget and creates the state
tensors. It also clears host state and propagates the device reset result.
The C adapter owns the GGML context and supplies typed views of the state
arrays. Invalid layer kinds are rejected before constructing descriptors.

The isolated Linux probe checks four arena budgets, all 66 state tensor
descriptors, recurrent aliasing, bytewise clearing and the GPU reset error
path through a stub. Linux and Windows universal links completed. Inference
performance and peak memory are pending.

```bash
out=$(mktemp -d)
src=integration/goose-standalone/gigachat4
probe=experiments/EXP-2026-11-27-2232-gigachat4-goose-request-lifecycle/probe.c
sed -n '/^extern "ggml_new_tensor_4d"/p' "$src/dsa-indexer.goose" >"$out/probe.goose"
cat "$src/request-lifecycle.goose" >>"$out/probe.goose"
"$GOOSE_BIN" --standalone -o "$out/probe-gen.c" --header "$out/probe.h" "$out/probe.goose"
gcc-15 -O2 -I"$out" -Ivendor/flint/ggml/include -Wl,--gc-sections \
    "$probe" "$out/probe-gen.c" "$COMMON_CORE"/{ggml,ggml-system}.o -lm -pthread -o "$out/probe"
"$out/probe"
```

Set `COMMON_CORE` to the Linux output of `build-gooseinfer-common-core.sh`.
