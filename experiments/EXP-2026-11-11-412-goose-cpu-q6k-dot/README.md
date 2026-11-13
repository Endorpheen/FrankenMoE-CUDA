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

## Minimal case says the module is wrong in the offset term, and the reference is not what I assumed

With `ql = qh = 0`, all `scales = 1`, `bsums = 16`, `x.d = 0x3c00`, `y.d = 1.0`:

    avx2 = -0.109756291   generic = -0.109756291

Both references agree with each other and are nowhere near the `-512` my module returns, so the disagreement is
in the offset/scale handling itself, not in accumulation. My assumption that this case must give
`-32 * 16 * scale` was wrong; the factor is set by how the reference folds `d` and the `-32` compensation.

Next step is the same method that settled Q4_K: an intrinsic transcription of this exact kernel in the test,
then diff `sumi`, `sums`/`acc` member by member against the module on the same block, instead of reasoning about
the formula.

## Scope confirmed by measurement

The scope note (`igor-apex-only-required-paths-15595bcf18c2`) says only the agreed Apex path matters. That is now
measured rather than assumed: first-call witnesses in a real CPU-profile run show exactly
`ggml_vec_dot_q4_K_q8_K` and `ggml_vec_dot_q6_K_q8_K`, with `q8_0`, `q2_K`, `q3_K`, `q5_K`, `f32`, `f16` and
`bf16` never called. Recorded in `docs/goose-migration/apex-cpu-math-inventory.md`.

So Q4_K (accepted) plus Q6_K (this item) complete CPU_MATH for this plan, and the 64 Q6_K tensors are one kernel.

## Scalar transcription progress

Built a scalar transcription of the AVX2 kernel next to the module and compared all three: transcription,
dispatched kernel and module, on identical data (nb = 1, 2, 7, 16).

First finding: `get_scale_shuffle(i)` returns **two** values per 16-byte vector — bytes 0-7 give `scales[2i]`,
bytes 8-15 give `scales[2i+1]` — so a 6-bit group uses one scale for lanes 0-3 and another for lanes 4-7. My
first version used a single scale per group, which was wrong.

After that correction the transcription moved but still disagrees with the reference (for example -212572.688
vs -455389.5), while `_generic` and the dispatched kernel agree to within one ulp. So one more mapping detail is
still wrong; the transcription is now the tool to find it, and the module will be aligned to it once the
transcription matches.

Not accepted, not integrated.

## Two defects found, gate now clean 240/240

1. **Block layout.** `block_q6_K` is `ql[128], qh[64], scales[16], d` — the super-block scale is **last**, not
   first. Everything was shifted, and on the isolated test the kernel returned 0 for every single-scale input
   because the scale read landed on zero bytes. With the correct offsets (`ql@0, qh@128, scales@192, d@208`)
   the single-scale probe returns -496 as it must.

2. **Two scales per 6-bit group.** `get_scale_shuffle` gives `scales[2i]` to lanes 0-3 and `scales[2i+1]` to
   lanes 4-7, so the module has to pick the scale per half.

The scalar transcription, built as an instrument, matches the dispatched kernel exactly (0 of 20 differing per
nb). The module was then aligned to it:

    FG412_ADAPTOR cases=240 failures=0

240 of 240 bit-exact for nb = 1, 2, 3, 7, 8, 16.

## Gate and mutations are clean; the two-kernel library is not

- gate: **240/240 bit-exact**, nb = 1, 2, 3, 7, 8, 16;
- mutations: **6/6 caught** (d offset, scales offset, half selection, offset shift, sub sign, qh shift).

Integration, however, is not right yet. `libggml-cpu` relinked with both Goose kernels returns `"!!!!"` /
`2f74420af441bf2d` instead of `" Paris, a city"` / `d13cf4cf221fa5fd`. Crucially, turning **either** kernel off
(`GGML_GOOSE_CPU_Q4K_DOT=0` or `GGML_GOOSE_CPU_Q6K_DOT=0`) still returns `"!!!!"`, so the fault is in the
relinked build itself, not in either kernel: the native path through the renamed symbols is already broken.

That is a build/link problem to fix next, and it is exactly why the A -> B -> A method insists on restoring A
before claiming anything: the module is correct against its reference, the assembled library is not yet correct.
