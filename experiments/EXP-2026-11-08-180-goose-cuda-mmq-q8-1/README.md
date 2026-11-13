# EXP180: MMQ Q8_1 quantization in Goose

Hypothesis: transfer the MMQ Q8_1 activation quantizer, both the direct and the
scatter form, to actual Goose.

## Verdict

**REJECTED** — the transfer is byte-exact but the kernel is 12-14% slower per launch
(2065 vs 1819 ns median) and the paired Apex showed a regression, so it is not promoted.
The switch is opt-in: `GGML_GOOSE_CUDA_MMQ_Q8_1=1` enables it, any other value including
unset keeps native.

Full diagnostic: [STATUS.md](./STATUS.md).

## A process defect found later, and fixed

Despite the rejection this module was on by default for several iterations. Its switch
was written opt-OUT, so the Goose path ran unless the flag was explicitly `"0"`, and the
object sat in the active link chain. Every Apex run in that period paid the regression,
and the evidence was in the model log the whole time as `FG180Q ... host=0x23`.

Fixed by inverting the switch to opt-in and rebuilding the chain 181 -> 183 -> 184 -> 185.
The paired figures moved from -1.28% to -0.55% decode and -0.19% prefill, and the witness
disappeared from the log. The rule this established -- a rejected experiment is never
default-on -- is now enforced in code and checked by `docs/goose-migration/coverage-registry.json`.

## Local gate

8 checks pass in both arms, 4 of 4 `block_q8_1_mmq` arrays byte-identical to native,
compute-sanitizer clean under memcheck, racecheck and initcheck.

## Why the transfer is still worth keeping

It is the prerequisite for the MMQ family, which is ~40% of GPU time. The blocker is the
code-generation gap measured in EXP186: a Goose kernel that recomputes a full byte address
per access compiles to roughly 2.5x the native instruction count. When that is solved
compiler-side, this module can be re-measured and enabled without new work.

## Fixture arm label corrected (2026-11-10)

The fixture's arm label disagreed with the dispatcher and is now derived from it.

Before, the label was `native_arm = getenv(...) && strcmp(getenv(...), "0") == 0`, so an **unset**
variable printed `arm=goose` while `fg180q_goose()` — which requires `"1"` — ran **native**. A bare
`./gpu-fixture` therefore looked like it exercised the Goose path when it did not. I hit this myself
while looking for the witness and briefly read the output as confirmation.

Now it mirrors the dispatcher exactly:

    static bool fg180q_goose(const char* flag){ return flag && strcmp(flag, "1") == 0; }

and the three cases behave as the label claims:

    unset -> arm=native      (was: arm=goose)
    =0    -> arm=native
    =1    -> arm=goose

Re-verified with the corrected binary against the active `exp220` DSO: both arms
`checks=8 failures=0`, output dumps `4 files, 0 differing` — so the route is still byte-exact and the
label now matches what actually ran.

This is a test-accuracy fix, not a weakening: the previous label could only ever make a native run look
like a Goose one, never the reverse.
