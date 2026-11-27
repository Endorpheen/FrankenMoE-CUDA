# EXP-2233: Inkling prefill traversal and graph lifecycle

The candidate moves prefill edge selection and visit ordering into Goose.
It also moves the common graph execution and publication sequence into
Goose. The platform adapter provides affinity, CUDA counters and logging.

The differential CPU probe compared 4,718,592 combinations of physical
layer, pass, sliding mode and policy flags with the previous edge rules.
Both universal targets linked with the reduced GGML source. The SM89 CUDA
image for i2_flash_prefill_all_softmax is byte-identical to the control
after removing eleven entry points excluded by the image manifest.

The native source inventory dropped from 39,651 to 37,815 physical lines
across direct host and GPU sources and included files. This includes
unused GGML helpers as well as CUDA source cleanup. No model inference or
peak RAM and VRAM measurements were performed.

```bash
out=$(mktemp -d)
src=integration/goose-standalone/universal/inkling/numeric.goose
sed -n '/^const I2_RETAIN_OUTPUT:/,/^extern "inkling_loop_storage"/p' "$src" |
    head -n -1 >"$out/edge-probe.goose"
"$GOOSE_BIN" --standalone -o "$out/edge-probe-gen.c" --header "$out/edge-probe.h" "$out/edge-probe.goose"
gcc-13 -O2 -I"$out" experiments/EXP-2026-11-27-2233-inkling-graph-prefill/probe.c \
    "$out/edge-probe-gen.c" -lm -o "$out/edge-probe"
"$out/edge-probe"
```
