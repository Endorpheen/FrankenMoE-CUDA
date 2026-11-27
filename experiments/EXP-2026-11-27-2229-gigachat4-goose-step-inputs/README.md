# EXP-2229: Fill GigaChat4 step inputs in Goose

The GigaChat4 request adapter now writes positions, row indices, output IDs
and the causal mask directly into their destination tensors. The Goose loop
fills the visible and masked portions independently. The adapter still
copies the input tokens and provides typed views of GGML data.

The differential probe matched all destinations for six shapes. Linux
GCC 15 at O2 measured 40.510 and 41.986 ms for the C mask loop at
4096 by 32768 versus 29.389 and 30.152 ms for Goose in the same A-B-B-A
block. At one row by 32768, the result was 10–11 versus about 7 us.
The probe C reference writes positions and indices in place. In the
former runtime path these small arrays were filled on the stack and copied
after the mask loop. Linux and Windows linked with the new GigaChat4 unit.
Full inference has not been measured with this change.

```bash
out=$(mktemp -d)
source=integration/goose-standalone/gigachat4/step-inputs.goose
probe=experiments/EXP-2026-11-27-2229-gigachat4-goose-step-inputs/probe.c
"$GOOSE_BIN" --standalone -o "$out/step.c" --header "$out/step.h" "$source"
gcc-15 -O2 -D_GNU_SOURCE "$probe" "$out/step.c" -I"$out" -lm -o "$out/probe"
"$out/probe"
```
