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
