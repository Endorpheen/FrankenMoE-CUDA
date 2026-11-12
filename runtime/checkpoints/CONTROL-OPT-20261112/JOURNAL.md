# Optimisation journal (small Apex path)

## Stage 1: control fixed

`config.json` records: git `799a77e`, compiler v61 `b87af17a`, artifacts and their SHA, build and run commands, the
model, the request, and the correctness expectation (`e68df05b`, equal probabilities, restore ok).

## Stage 2, finding 1: the regression is in libllama, and it is large

All measurements on the identical argv (`runtime/exp244/results-apex-or244-or-goose/argv.json`, 32K prompt / 128
decode), same machine state, browser load present and noted:

| libraries | prefill tok/s | decode tok/s | VRAM (own) MiB | content equal |
|---|---|---|---|---|
| current staging (libllama-exp371 + goose cpu) | 1588.7 | 21.93 | 6408 | yes |
| current staging + historical cpu (`*.inherited`, 63ad4984) | 1588.1 | 22.83 | 6408 | yes |
| **libllama-exp244 (2bc628a1) + historical cpu** | **2567.7** | **25.47** | **4590** | yes |
| libllama-exp244 + current cpu | 2570.0 | 25.35 | 4590 | yes |

So the CPU library contributes nothing to the loss, and libllama alone accounts for **+62% prefill, +16% decode and
-1818 MiB VRAM**. Both libllama variants are Goose builds (214 and 307 `fg` symbols), so this is not "native vs Goose" -
it is the newer libllama line being slower on the same profile.

Disabling suspected module groups inside exp371 did not recover anything (all within noise):
`GRAPH_INPUT_PLAN` 1586.1, `HYBRID_DELEGATION` 1547.6, and the batch/detok/sampler group 1598.5.

Also measured against the accepted EXP244 record (2500.1 / 24.51): the exp244 library still reproduces its own class
of numbers, so 2500-2570 is a reproducible baseline on this machine, not a historical artefact.

Next: find which code difference between the two libllama lines costs 62% of prefill, without reverting ported work to
C++.

## Stage 2, accepted change 1: staging uses the exp244 libllama

Switching the `libllama.so*` symlinks in `stage-run` from `exp371` to `exp244` (both Goose builds; nothing reverted to
C++) and keeping every other library as-is:

| metric | before | after | delta |
|---|---|---|---|
| prefill tok/s | 1588.7 | **2563.2** | +61.3% |
| decode tok/s | 21.93 | **26.09** | +19.0% |
| peak RSS MiB | 22855.8 | **21309.6** | -1546 |
| VRAM own MiB | 6408 | **4590** | -1818 |
| correctness | `e68df05b`, probs equal, restore ok | same | unchanged |

Note on the earlier 1616 measurement: only `libllama.so.0.7.0` had been repointed while `libllama.so.0` (the SONAME the
loader actually opens) still pointed at exp371. Both symlinks must be changed together - that mistake cost one false
measurement and is recorded so it is not repeated.

Rollback: `stage-run/libllama.so*` back to `libllama-exp371.so.0.7.0` (a copy is kept as
`libllama-exp371-optrollback.so.0.7.0`).

## Stage 3: memory

`--no-kv-offload` moves the KV cache to RAM: VRAM 4590 -> 3194 (-1396) but prefill 1913 (-25%) and decode 3.77 (-85%),
with the content check failing. So the KV cache must stay on the GPU for this profile, and 4590 MiB is close to the
floor here. Against the 6408 MiB starting point the accepted change already saved 1818 MiB of VRAM and 1546 MiB of RSS.

## Stage 4: final A/B -> B/A on identical argv

Two repeats each, alternating order, same machine (browser load present and noted; an interfering GPU load from other
users was cleared before this series):

| run | prefill tok/s | decode tok/s | RSS MiB | VRAM own MiB | content equal |
|---|---|---|---|---|---|
| A1 (control, exp371) | 1621.1 | 22.72 | 22856 | 6408 | yes |
| B1 (result, exp244) | 2574.2 | 25.91 | 21318 | 4590 | yes |
| A2 (control, exp371) | 1613.2 | 21.47 | 22856 | 6408 | yes |
| B2 (result, exp244) | 2568.2 | 24.77 | 21317 | 4590 | yes |

Accepted change: prefill 1617 -> 2571 (+59%), decode 22.1 -> 25.3 (+15%, spread 24.8-25.9), RSS -1538 MiB,
VRAM -1818 MiB, correctness unchanged at `e68df05b`.

Limits of the measurement: browser and desktop GPU activity is part of the baseline; the target of 3000 tok/s prefill and
30 tok/s decode is **not** reached yet; the series is a short paired one, the clean long series is still to be run.

## Rejected: hybrid expert placement

Replacing `--cpu-moe` with `--n-cpu-moe 34` (6 expert layers on the GPU) gives prefill 2651.1 (+3%) and decode 27.76
(+9%) at the cost of VRAM 4590 -> 7578 (+2988), and the content check fails. `--n-cpu-moe 30` runs out of memory.
Different device placement changes the rounding, so the output is no longer the accepted `e68df05b`. Since the
correctness criterion is not negotiable, this variant is **rejected** and nothing was enabled by default.

## Where the remaining ~17% is not

Checked and excluded, all within noise of 2570/25.5: ubatch 2048 and 8192; batch threads 32 and CPU mask ffffffff;
`--poll 1`; KV types f16 and q8_0; disabling the GoQ4_K/GoQ6_K CPU dots, the CUDA Q6_K matvec, the MMQ Q8_1
quantizer or the pinned ring; `GRAPH_INPUT_PLAN`, `HYBRID_DELEGATION`, and the batch/detok/sampler group. A 4K prompt
gives *lower* tok/s (2009) than a 32K prompt, so this is a fixed per-graph overhead plus steady-state throughput, not
attention scaling.

## Observation: --ctx-checkpoints 0 changes the output (not accepted)

With `--ctx-checkpoints 0` instead of 32, RSS drops by only 140 MiB (21318 -> 21178) and the content check fails, twice,
reproducibly (2588.2/26.05 and 2587.2/25.34). The request uses `cache_prompt: false`, so a context checkpoint should not
be able to change the completion. The accepted profile keeps 32 and stays correct, so nothing was changed; this is
recorded as an anomaly of the accepted path that needs a separate look, together with the same class of question as the
placement test above: which parameters are allowed to change the numbers and which are supposed to be exact.

## Correction: the exp244 libllama switch is diagnostic, not an accepted optimisation

The reporting contract states it plainly: a restored older library is a diagnostic result until preservation of the
complete port is checked, and a reduced port must not be labelled as accepted full-path optimisation. Checked: exp244
libllama carries 214 `fg` symbols, exp371 carries 307. The extra 34 modules (fg345w, fg367s, fg373s, fg375s/376s,
fg377s, fg380s-397s, fg398s, fg401s-408s and fg212s) are the later ported decisions - KV, batch container, detok,
scheduler, weight placement, hybrid. Switching to exp244 therefore *drops* ported work, which is exactly what must not
be accepted.

So the numbers stay as diagnostics: they prove the regression lives in the exp371 libllama line and that the CPU
library is innocent, but they are not the optimisation. The accepted optimisation has to reach the same speed with all
307 modules present.

Commit 063de29 keeps its numbers but its `#accepted` tag is superseded by this correction; the dashboard feed marks the
switch as diagnostic, and the staging symlinks still point at exp244 so measurements stay comparable while the cause is
hunted - that is a working state, not a shipped default.

## Full port restored in staging; the compiler-mix clue

Staging is back on `libllama-exp371` (307 fg modules), measured again as the working baseline: **1624.1 / 23.64**,
RSS 22856.5, VRAM 6408, content equal. So the diagnostic set (2563-2574 / 25.9) is +58% prefill against the full port,
and the gap is real, not a measurement artefact.

`.comment` gives the first concrete clue: `libllama-exp244` is built entirely by GCC 13.4.0, while `libllama-exp371`
contains objects from **both** GCC 15.2.0 and 13.4.0. The `.text` sizes are nearly equal (861.8 KB vs 857.9 KB), so this
is not code bloat - it is a mixed-provenance build, and a module compiled by a different compiler is the prime suspect
for a 58% prefill loss. Env-level bisection cannot test this: every KV group I tried is mandatory (the server segfaults
when they are disabled), so the next step is a rebuild with one compiler and one optimisation level, not more flag
fiddling.

KV must stay on the GPU: `--no-kv-offload` reaches VRAM 3194 but decode collapses to 3.77.
