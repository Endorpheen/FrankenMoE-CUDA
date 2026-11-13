# EXP299: the P5-02 fix, validated at runtime before it is built in

EXP298 located the quantized-K abort: `entry.goose` validates the secondary sources without a scalar check, and the C gate
`fg143_try` looks only at the destination type. The module does not port flash attention, so the correct behaviour is to
decline when K or V is block-quantized and let the native implementation run, as the flint line does.

## Diagnosis confirmed by removal

Before changing anything, the module was disabled entirely:

    GGML_GOOSE_CPU_BACKEND=0  ->  q8_0/q8_0 serves, content " over the lazy dog.\n\nThe quick" (1da0f499), zero assertions

So fg143 is the only cause and the native path handles quantized K/V correctly.

## Fix validated by interposition

`fg143_try` is exported (`nm -D` shows `T fg143_try`), so the exact two-line guard was applied through an `LD_PRELOAD` shim
for validation before any rebuild:

    if (t->src[1] && !fg_scalar(t->src[1]->type)) return 0;
    if (t->src[2] && !fg_scalar(t->src[2]->type)) return 0;

| run | result |
|---|---|
| q8_0 / q8_0, shim | serves, `1da0f49942ac7858`, identical to native and to F16, zero assertions |
| q4_0 / q4_0, shim | serves, `1da0f49942ac7858`, identical |
| accepted profile, shim | `e68df05b...`, 128 tokens -- unchanged |

## What this is and is not

This is a validation, not the delivered fix. The shim exists so the change could be proven complete and regression-free
before touching a build that has no ready recipe here. The permanent change is the same two lines in `native-bridge.cpp`
plus a rebuild of `libggml-cpu`, and the acceptance gate for P5-02 is the same three rows above without `LD_PRELOAD`.

Guarding only the secondary sources is deliberate: quantized weights arrive as `src[0]` and are handled by the codec path
that runs before the scalar check, so model weights keep their current treatment. The Apex row is the evidence that this
holds.

## Delivered, not just shimmed

The build recipe was found in `variants/cpu-backend/build/integration-commands.json`: `cpu-bridge` compiles
`native-bridge.cpp` into `cpu-bridge.o`, and `link` produces `libggml-cpu.so.0.27.0`. Both steps were re-run after the two
guards were added to the source:

    if (t->src[1] && !fg143_scalar(t->src[1]->type)) return 0;
    if (t->src[2] && !fg143_scalar(t->src[2]->type)) return 0;

The rebuilt library was first linked to a side file and gated there, then promoted into
`variants/cpu-backend/run/libggml-cpu.so.0.27.0` (old file kept as `.pre-p502`; hashes 71ca8c23... -> f49e3919...).

## Acceptance, without LD_PRELOAD

| run | result |
|---|---|
| q8_0 / q8_0 | serves, `1da0f49942ac7858`, zero assertions |
| q4_0 / q4_0 | serves, `1da0f49942ac7858`, zero assertions |
| accepted profile | `e68df05b...`, 128 tokens |

The q8_0 and q4_0 continuations are identical to each other, to F16 and to the native line, so the quantized layouts now
match rather than merely run.

## Files

- `native-bridge.patch` -- the delivered change (13 lines of diff against the registered source snapshot).
- `gate-shim.c` / `gate-shim.so` -- the pre-build validation used in the previous step; kept as evidence, not used by the
  promoted library.
- `stage-fixed/` -- a stage whose `libggml-cpu` pointed at the side-built file during the gates.
