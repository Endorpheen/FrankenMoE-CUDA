# P10-12 — closing G1, G2, G3 and G6: tallies, prefill plan, trace order, moe-predict

Date: 2026-11-15. Base commit: `1b3f4a1a`. The shared tree has since moved to `ec50a32d`, but
`runtime/core/src/engine/session.cpp` and `runtime/cli/main.cpp` are byte-identical between
the two (`git diff --stat 1b3f4a1a..HEAD --` both files is empty), so the extracted oracle is
the base-commit text. Verdict: **ACCEPTED candidate**, four slices, model-free. Work done in
`/tmp/opencode/sub-650/EXP-2026-11-15-649-p10-12-tallies-prefill-trace/`; the shared tree,
the production runtime and other agents' files are untouched. No model, GPU, server or
benchmark was started.

## 1. What this closes

The P10-12 audit (`EXP-2026-11-15-647`, `REPORT.md` §4-§5) listed seven residual regions in
`engine/session.cpp` and `cli/main.cpp` and recommended the first two slices in this order:
G1 (the tallies), then G6 (`moe-predict`), then G2/G3 (prefill plan and trace framing) if they
fit. All four fit, because all four are decisions with no model, no GPU and no llama API in
them. After this experiment the residual map is:

| gap | state |
|---|---|
| G1 `GenTally`/`PrefillTally` | **closed** — `tl_` (S1) |
| G2 prefill chunk plan | **closed** — `pf_` (S2) |
| G3 trace framing order | **closed** — `tr_` (S3) |
| G6 `moe-predict` report | **closed** — `mp_` (S4) |
| G4 MTP capture warm-up | open — larger, touches the vocabulary; P9-08 remainder 4 |
| G5 sampler chain construction | open — llama API call; substrate |
| G7 CLI driver loop beyond the line protocol | open — queue/threads are substrate, the `--ppl` argument projection is not |
| G8, G15 | audited primitive / reconstruction, not modules |
| G9-G14 | owned by P10-05, P10-07, P10-09, P10-02 |

## 2. Method

The carrier files cannot be linked: `session.cpp` pulls llama.cpp, the router hook, the three
trace sinks and the expert source, and half of the regions live in an anonymous namespace or
inside `generate()`. So the oracle is extracted, not retyped:
`tests/native_extract.py` copies the exact byte ranges of the base commit into
`build/native_*.{h,inc}` using exact line anchors, and each fixture `#include`s them with
stubs for the services the port deliberately does not own. A moved anchor fails the gate
build.

- S1 `tl_`: `GenTally` (157-251), `PrefillTally` (259-285), the summary blocks (1934-1935,
  1945-1952, 1967-1968).
- S2 `pf_`: the prompt loop header and chunk line (1541-1542).
- S3 `tr_`: `trace_begin` (1485-1496) and `trace_flush` (1497-1530).
- S4 `mp_`: `print_predict_report` (540-588) in `cli/main.cpp`.

The four slices are one atomic commit's worth of work: each has its own module, its own gate
and its own twelve mutations, but they close one audit recommendation and share one ledger
change.

## 3. Results

```
FG649_TL cases= 28 values= 847 failures=0    GenTally / PrefillTally / summary averages
FG649_PF cases= 16 values= 432 failures=0    prefill chunk plan
FG649_TR cases= 42 values= 433 failures=0    trace framing order
FG649_MP cases= 15 values=1064 failures=0    moe-predict report
```

101 cases / 2776 compared values, identical at `-O1` and `-O0` (`results/gate-o1.log`,
`results/gate-o0.log`). Doubles are compared bit-exactly; integers exactly.

Mutations: 12 per slice in two halves of six, the module regenerated at `-O0`:

```
FG649_MUT half=1 caught=24/24  tl=6/6 pf=6/6 tr=6/6 mp=6/6
FG649_MUT half=2 caught=24/24  tl=6/6 pf=6/6 tr=6/6 mp=6/6
```

## 4. What the gates found in the port

Three findings, all fixed rather than relabelled, and all of the kind the audit predicted
("a wrong delta is visible in one number"):

1. `mp_pct` divided two `i64` operands. This language's `/` on two integers is integer
   division, so a per-layer column printed `0.0` while the aggregate printed `75.0` — the
   module's own aggregate helper used a different operand shape and stayed correct. Fixed by
   declaring both operands `f64`; the comment in the module records why.
2. A `pf` mutation that advances the loop one short of the batch is an infinite loop for
   `n_batch == 1`, not a caught mutation. Replaced by "advance two batches", which the gate
   catches on every prompt longer than one batch.
3. The `mp` warning threshold mutation (0.999 → 0.998) survived because the corpus only had
   0.999 and 1.0. A control at 0.9985 was added — it is the interval between the two
   predicates.

The remaining native quirks are carried, not normalised: the compute residual clamps at 0,
`cache_hit_pct` is `-1.0` (not 0) when there were no lookups, the I/O take happens even when
the frame is empty, the phase reaches the drop policy before any trace call, and the compute
interval closes before the first row leaves.

## 5. Compatibility and assumptions

- `goose check` per module: 3 / 1 / 4 / 12 specializations, no errors, no warnings. The four
  sources concatenated typecheck as one unit (35 declarations, 20 specializations), so the
  composition step will not see duplicate symbols.
- Prefixes `tl_`, `pf_`, `tr_`, `mp_` are free in the active tree; no file-name collision.
- `n_batch >= 1` is assumed (config validator) and guarded, because the native loop would not
  terminate otherwise and the gate cannot compare against it.
- Byte deltas stay below 2^63; the wrap case is compared as a signed value.
- `%.1f` stays libc through `mp_fmt_f1`, the same concession `mt_fmt_g6` makes.
- `mp_report`'s buffer is sized by the caller (≈40 bytes per row).

## 6. Exact integration step

`README.md` §"Exact integration step" has the copy commands, the `MODULES` insertion after
`goose/misc/kv-turn-state.goose`, the note that no `RENAMES` entry is needed, and the call-site
replacements in `session.cpp` and `cli/main.cpp`. The syscalls, `llama_*` calls and the sinks
do not move.

## 7. Limitations

- The gates compare the port against the carrier's text, not the carrier under a model. A
  decision that only misbehaves with real router statistics (for example a delta that is
  correct for synthetic counters but wrong for a warm session) would not be visible here; the
  corpus does cover cumulative counters across several tokens, a counter that goes backwards
  and a dense token between two streaming ones.
- `pio` is stubbed in the `tl` gate: the `/proc` columns belong to `pr_` and are not compared.
  `fault_bytes` is stubbed at `sysconf(_SC_PAGESIZE)` and asserted equal to it.
- The S3 gate compares the ordering plan and the stamps, not the row contents; the row
  encoders are `mt_`'s (`metrics/metrics-traces.goose`).
- Coverage here means "these four regions have a module with a gate", not "the carrier is
  dead". Until the integration step lands, both files still compile into the production build.
