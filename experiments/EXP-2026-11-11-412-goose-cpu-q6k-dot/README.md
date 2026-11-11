# EXP412: Q6_K x Q8_K dot (in progress)

## Hypothesis

The next kernel on the CPU path after Q4_K is Q6_K x Q8_K, with the same acceptance route as EXP411: gate against
the library's dispatched AVX2 kernel, mutations, then a real run on the CPU bench where native and Goose
libraries must produce identical output.

## AVX2 algorithm (from arch/x86/quants.c)

- `d = y.d * f16(x.d)`;
- `q8sclsub[k] = (bsums[2k]*scales[2k] + bsums[2k+1]*scales[2k+1]) << 5` — the "-32 offset" term, folded into
  bsums instead of subtracting 32 per element;
- two 128-element groups; in each, four 6-bit groups built from `ql`, `qh`, `maddubs`, `madd_epi16` with the
  signed scale, four consecutive elements per lane;
- `sumi -= q8sclsub`, then `acc = fma(d, sumi, acc)` and `*s = hsum_float_8(acc)`.

## State

Module and gate are written and build. Gate result: **240/240 fail** — the implementation is not correct yet.

Diagnostic already done: on all-zero `ql`/`qh` with `scales[0] = 1` and `bsums = 16`, the module returns `-512`
while **both** the dispatched kernel and `_generic` return `0`, so the reference disagrees with the module on the
offset term itself, not on the accumulation. Next step is to reconcile that case before touching anything else.

Not accepted, not integrated.
