# Open question: a print that must fire does not, in the same block as one that does

State on 2026-11-18, recorded instead of guessed at, because the evidence contradicts itself.

## The evidence

One run, `FG_ACCEPT_DIAG=1`, `--spec-type ngram-simple --spec-ngram-simple-size-n 3
--spec-ngram-simple-size-m 8`, 24 tokens:

```
FG_SEQ_RM type=2 (0=no, so speculative is disabled)
FG_ACCEPT_BRANCH synth=0 replay=0 reject=1 draft=8
FG_ACCEPT_Q draft_q=0
FG_PREFIX_ENTER draft=8 result=9 fn=0x7e6497dafc90
```

`FG_PREFIX_ENTER` is printed by a block I added three lines above the guarded block, in the **same
function** (`common_sampler_sample_and_accept_n`, the `idxs` overload). It reports a non-null function
pointer. The next statement is `if (const auto fn = fg_sp_prefix_resolve()) {`, and inside that block,
at its end:

```c
if (c <= 5 || (c % 200) == 0) {
    fprintf(stderr, "FG_SP_PREFIX goose calls=%llu disagreements=%llu\n", ...);
}
```

`FG_SP_PREFIX` appears **once** in `libllama-common.so` (`strings | grep -c` = 1), the library is the
freshly built one (04:08:35), and the same run loads it - proven by `FG_PREFIX_ENTER` existing only in
the new code. So the print is compiled in, reached in program order, and does not appear.

## What was checked

- the source at lines 756 and 781 is exactly as above, in one function;
- `fg_sp_prefix_resolve()` is called three times in the file (definition, entry print, the `if`);
  function-local static, so the pointer cannot differ between the two calls in one run;
- the log contains every other `FG*` line from that run, so stderr is captured;
- the branch is reached (`draft=8`), and the function is entered (`draft=8 result=9`).

## Why it is written down rather than chased further

Three iterations have now gone into instrumentation on this path. What is unanswered is a
contradiction between source order and output, not a property of the ported decision - and the decision
itself is gated independently (5006 cases, including the six boundaries that separate it from plausible
rewrites). The next attempt should start by disassembling the function or by replacing the conditional
print with an unconditional one, not by reading the source again.
