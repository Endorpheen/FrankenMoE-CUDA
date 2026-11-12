# The MMQ route ships in the runtime, opted out — two defects, one correction to my own records

While checking whether the ne2 fix could simply be measured on the current chain, I found that the
Goose MMQ Q8_1 code is **already inside the active DSO**, and that it is switched off in a way that
contradicts the project's hard rule.

## What is actually in `libggml-cuda-pinned.so.0.27.0` of exp220

    fg180q_kernel<0>                              (local, Goose, direct)
    fg180q_kernel<1>                              (local, Goose, scatter)
    fg180q_native_quantize_mmq_q8_1_cuda          (local, renamed native — the rollback)
    quantize_mmq_q8_1_cuda                        (exported, the dispatcher)

The dispatcher:

    static bool fg180q_goose(const char* flag){ return flag && strcmp(flag, "1") == 0; }
    const char* flag = getenv("GGML_GOOSE_CUDA_MMQ_Q8_1");
    if (!fg180q_goose(flag)) { fg180q_native_...(...); return; }

Verified in the binary too: the exported function calls `getenv` and compares the first byte to
`'1'` (0x31).

## Defect 1 — this is opt-in, which rule 10 forbids

The rule: "Accepted improvements must be enabled by default in new builds. A new feature's env/CLI
flag exists only to disable the feature, never to opt in." `== "1"` is exactly the forbidden shape.
Every other module in this repository uses `!(f && strcmp(f,"0")==0)` — default on, `0` disables.
The switch audit in `switch-hygiene.md` covered 18 modules and did **not** catch this one, because
that audit enumerated `bridge.c` files and this dispatcher is in `cuda-runtime.cu`.

Effect: the Goose MMQ is dormant in every Apex run. That is also why no `FG180Q_CUDA_MMQ_Q8_1`
witness appears in the logs — `fg180q_report` is registered with `atexit` only after the first Goose
call, so its silence means "never taken", not "instrumentation broken".

## Defect 2 — the fixture mislabels the default

    const bool native_arm = getenv(...) && strcmp(getenv(...), "0") == 0;

so `arm` reads "goose" whenever the variable is *unset*, while the dispatcher in that case runs
**native**. The label is right only for the explicit `"0"` and `"1"` values.

Consequence for my own records: any run of `./gpu-fixture` without the variable printed
`arm=goose` while executing native. The timed runs and the byte-compare in EXP222/EXP223 used
explicit `=1` and `=0`, so those results stand — but the bare-invocation runs I did while looking for
the witness were native-vs-native and prove nothing. I am recording that rather than leaving the
impression that "arm=goose" ever confirms the Goose path.

## Correction to the ledger

`P3-09` says the MMQ feeder is "not promoted". In effect that is right — it does not execute — but the
code is **present and dormant** in the shipped DSO, which the ledger does not say. A reader would
conclude there is nothing to enable, and would not know a rule-violating opt-in switch is sitting in
the artifact.

## What should happen, in order

1. Decide the switch direction for the current chain. Rule 10 says default-on; but default-on means
   every Apex run takes the 11.31% slower path, so the honest sequence is finish the performance work
   first, then flip and delete the opt-in.
2. Take the `ne2` fix (EXP222, −3.69%) into the same change so the flip costs as little as possible.
3. Fix the fixture's label so an unset variable reports what the dispatcher will actually do.
4. Re-measure with the interleaved protocol on the current lineage before any flip.

Until step 1 happens, the correct statement is: **MMQ Q8_1 is shipped in the runtime, disabled by an
opt-in switch that contradicts rule 10, and never measured on the current lineage.**
