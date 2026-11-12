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

## Refinement: where the GCC 15 objects come from

`runtime/reconstructed/build.py` explains the mix: the generated modules go through `gcc-13`/`g++-13` at `-O2`
(lines 16 and 63), while `bridge.c` is compiled with plain `gcc -O3` and the shared object is linked with
`/usr/bin/c++` (lines 100 and 107) - both of which resolve to GCC 15.2.0 on this machine. So the GCC 15 objects are the
bridge and the link step, not the per-call modules. That weakens the compiler-mix lead for the hot path: `bridge.c` is
not called per token, and a linker version cannot slow a loop.

What remains to test, in order: (1) the `-O2` used for the modules against `-O3`, now that the diagnostic build proves
the same port runs 58% faster with a pure GCC 13.4.0 library; (2) the actual module bodies on the hot path, since every
env-gated group turned out to be mandatory and could not be bisected at runtime.

## All Goose graph switches excluded; the difference is in the build itself

Disabling each Goose decision group inside exp371 changes nothing (all within noise of 1624):
`MODEL_GRAPH` 1605.0, `OUTPUT_REORDER` 1591.8, `TRANSFER_POLICY` 1572.7, `SCHED_DECISIONS` 1621.9,
`FUSED_OPS_RESOLVE` 1657.5, `HYBRID_MEMORY` 1613.9, `HYBRID_CONTEXT` 1636.9, both hybrid 1631.8. Every one keeps the
content check green.

Combined with the earlier group tests, this means the 58% gap is not produced by any decision the modules make at
runtime. The remaining difference is in the compiled objects themselves: the `changed` list in `build.py`
(llama-graph/context/sampler/vocab/...) or the flint object set. The next step is therefore a rebuild bisection - link
the exp244 object set, then swap in our objects one file at a time - instead of more flag experiments.

## The regression is not in the Goose code at all

Two decisive measurements:

1. A **pure native** `libllama` built here from the flint objects (no Goose source, 44 objects, linked with the same
   rpath) measures **1636.0 / 26.04** - the same class as our full port (1624.1 / 23.64). The ported code is therefore
   not what costs prefill.
2. `lrpath` inspection explains the "magic" exp244 result: `libllama-exp244` carries an rpath to
   `llama.cpp-flint-cuda/build-cuda/bin` and loads **that** ggml stack, not our staging one. All four libraries differ:
   libggml, libggml-base, libggml-cpu, libggml-cuda (1e52e0dc/aa5274b5/b4ec8b3a/68681579 against our
   9a9ea86a/e213fa19/ab6f61ea/219af975).

So the 2563 figure was never "the old library is faster because it lacks the port" - it was a different ggml stack
(and a different llama body) loaded through rpath.

Consequence: the remaining speed work has to target **our staging ggml stack**, not libllama and not the Goose modules.
A first datapoint: full port on the flint ggml stack measures 1787.7 / 25.99 (+10% prefill, +10% decode over 1624.1 /
23.64), content equal. Note the rule: those libraries are native, so they cannot be shipped as the accepted
optimisation; their job is to tell us what to look for in ours.

## Correction of the record (history kept)

The section "Stage 2, accepted change 1" above is **wrong and withdrawn**. Igor does not accept replacing the current
libllama with the older EXP244 as an optimisation: that is a rollback of ported work, and the mere fact that both
libraries contain Goose code does not prove that every later ported section is preserved (214 modules against 307).
The 2563-2574 / 25.9 numbers stay in the table and in the dashboard as a **diagnostic control only**. They are never to
be reported as a recovery of the full path and never as an accepted optimisation. The commit that carried the
`#accepted` tag for that switch (063de29) is superseded by this correction.

The working candidate is the full port: staging symlinks point at `libllama-exp371` (307 modules), verified again after
this correction, and every measurement from here on uses it. Rollback copies of both libraries are kept, nothing was
deleted, and no counters were changed.

Direction stays as the measurements established it: the port itself is free (native libllama 1636.0/26.04 against full
port 1624.1/23.64), the gap sits in the ggml stack and in the llama body, and the next step is the concrete cause inside
the full candidate - starting with an object-level comparison of our staging ggml against the flint build.

## The ggml stack is innocent; the regression is inside our libllama body

The separating measurement: the exp244 libllama run **on our own staging ggml stack** measures **2574.9 / 24.6**, content
equal. So neither the ggml stack nor the CUDA/CPU libraries cost anything - the same ggml that gives our full port 1624.1
gives the exp244 body 2574.9. The difference is in the llama translation units themselves.

That leaves two candidates inside our candidate, and both are ours, not Igor's port:
1. the `changed` list in `build.py` - llama-graph/context/sampler/vocab/kv-cache/memory/model-loader/unicode copied from
   `runtime/reconstructed/src`;
2. the link step (`/usr/bin/c++`, `-O3`, no LTO) against the flint object set built by CMake.

Since disabling every Goose decision group changed nothing, the cost is most likely in differential code that is not
guarded by an `enabled()` switch - i.e. in the copied sources themselves or in how they are compiled. Next step: build
the same `changed` list straight from the flint tree (no copied sources) and compare, then swap the list back one file
at a time.

## Root cause: the expert-pinning mechanism is missing from the working candidate

Symbol-level comparison of the three libraries settled it: `llama_model::pin_mmap_experts` and the
`llama_mmap_expert_pin` helpers are present in `libllama-exp244` and **absent** in both our full port (`exp371`) and in
the native build from the flint objects. That is the whole asymmetry - it is not the Goose modules, not the ggml stack,
and not the compiler.

The mechanism lives in `LlamaServer/franken-exp090-deepseek41/source/src/llama-model.cpp` (also `exp091-pinned-moe-tail`)
and is gated by `GGML_MMAP_EXPERT_PIN` (on by default). It walks `tensors_by_name`, keeps only tensors with
`ne[2] == n_expert` that live in a host/mmap buffer, merges the ranges to page boundaries, and registers them as host
buffers with the GPU backend. With `--cpu-moe` the expert pages then stay resident and the CPU no longer takes faults on
every expert read; our candidate silently lost that, which matches +58% prefill and 1818 MiB of VRAM at once.

This is a restoration of lost work, not a rollback: the fix is to port `pin_mmap_experts` plus its helpers into our SRC
and rebuild, keeping all 307 Goose modules. `GGML_MMAP_EXPERT_PIN=0` will remain the disable switch, per the default-on
rule.

Next: port the mechanism (llama-model.h/.cpp, llama-context.cpp call site, llama-model-loader pieces) and measure.

## Accepted: expert mmap pinning restored into the full 307-module candidate

Ported `llama_model::pin_mmap_experts`, `llama_mmap_expert_pin` and the `llama-context` call site into our SRC, added
`llama-model.cpp` to the `changed` list, and rebuilt. All 307 Goose modules stay in the library - nothing was reverted to
C++ fallback, and `GGML_MMAP_EXPERT_PIN=0` remains the disable switch per the default-on rule.

| measurement | prefill tok/s | decode tok/s | RSS MiB | VRAM MiB | correctness |
|---|---|---|---|---|---|
| before (full port, no pinning) | 1624.1 | 23.64 | 22856 | 6408 | e68df05b |
| pinning off (`GGML_MMAP_EXPERT_PIN=0`) | 1619.6 | 22.91 | - | - | equal |
| **pinning on (accepted)** | **2337.3** | 23.14 | 22857 | 6408 | e68df05b |

The paired on/off comparison on identical argv isolates the effect at **+44% prefill**; decode is inside the noise band.
The narrow condition set in the ported call site (2 backends, flash-attn, single sequence, no op-offload change) matches
the agreed profile exactly.

Rollback: `build/libllama-exp371-prepin.so.0.7.0` keeps the pre-pinning library.

## Pinning: paired confirmation

Repeated on/off with the switch, identical argv:

| order | pin on | pin off |
|---|---|---|
| 1 | 2369.8 / 23.56 | 1655.6 / 23.96 |
| 2 | 2361.2 / 24.54 | 1645.9 / 24.01 |

So the accepted effect is +43-44% prefill and holds across repeats; decode stays in its usual band. Note: the
`registered ... mapped expert ranges` INFO line does not appear in the captured log even though the switch demonstrably
changes throughput, so the log line is not usable as evidence here - the paired measurement is. This is recorded so the
absence of that line is not misread later as "the mechanism did nothing".

## Decode: not the Goose CPU kernels, not mmap

Two checks on the accepted build:
- `GGML_GOOSE_CPU_Q4K_DOT=0` -> 2378.1 / 24.48 and `GGML_GOOSE_CPU_Q6K_DOT=0` -> 2361.4 / 23.75, i.e. inside noise of
  2365 / 23.9. The ported CPU dots are therefore not what holds decode back.
- `--no-mmap` does not complete on this machine (the 35B model no longer fits without mmap); decode cannot be tested
  that way here and the idea is dropped rather than forced.

So decode in this profile is bounded by the `--cpu-moe` arrangement itself (expert GEMV over host memory at 8 threads),
not by ported code. The one variant that raised decode (hybrid `--n-cpu-moe 34`, 27.76) was already rejected because it
changes the numbers on the content check.

## Method reused: the remaining ~9% is not in the decision modules

Following Igor's method (compare old fast and current implementations, one hypothesis, on/off control on the same
build), each remaining module family was switched off individually on the accepted build:

| switch off | prefill | decode |
|---|---|---|
| BATCH_CONTAINER_LAYOUT | 2376.6 | 23.74 |
| BATCH_CLASSIFY | 2372.7 | 23.67 |
| KV_SET | 2372.1 | 24.36 |
| KV_READ_SINFO | 2379.6 | 24.07 |
| (reference, all on) | ~2365 | ~23.9 |

All inside noise, content equal everywhere. Combined with the earlier group tests this closes the "a decision module is
the cost" branch: no single ported decision carries the remaining gap between our 2365 and the exp244-body 2575 on the
same ggml. So the next prefill work is not in module switches at all - it is in the graph/quantizer path.

Memory side unchanged so far: pinning keeps experts pageable in RAM (RSS 22857, VRAM 6408). Any change now has to be
judged on all four metrics, per the balanced-target instruction.

## moe-cache: the decode target is reachable, but not with the exact-check criterion

The profile never uses `--moe-cache-mib`, and the build has the whole mechanism (`llama-moe-cache.cpp`, 23 symbols).
Enabled addressolly:

| variant | prefill | decode | RSS MiB | VRAM MiB | content equal |
|---|---|---|---|---|---|
| accepted (pinning) | ~2365 | 23.9 | 22857 | 6408 | yes |
| `--moe-cache-mib 2048` | 2409.6 | **32.81** | 22858 | 8456 | **no** |
| `--moe-cache-mib 4096` | fails | - | - | - | - |

So decode 30+ is available on this machine, at +2048 MiB VRAM and with a different numerical result. The content check
compares against the accepted `e68df05b`, and device-side expert caching changes rounding, exactly like the rejected
`--n-cpu-moe 34` case. Per the standing rule the criterion is not weakened, so this is **not accepted**; it is recorded
as the strongest known lead for the decode target and as a decision for Igor: keep bit-exact output and 23.9 decode, or
accept a numerically different (but still valid) path to 32.8.

`4096` exceeds the free VRAM and fails to start - noted so it is not retried blindly.

## Restored EXP087: lazy prompt compute reservation (VRAM -1978, RSS -1539)

Igor's historical pointer checked: EXP087 `c3c52dc` (lazy prompt compute reservation + bounded host growth, switch
`GGML_LAZY_PP_RESERVE`) was **absent** from our candidate and present in the exp244 body - which is why our VRAM sat at
6408, the exact pre-EXP087 figure. Applied `patches/lazy-pp-reservation.patch` to our `SRC/llama-context.cpp` and to the
flint `ggml` alloc/backend, rebuilt `libggml-base` and `libllama`.

| switch | prefill | decode | RSS MiB | VRAM MiB | correctness |
|---|---|---|---|---|---|
| reservation off (`GGML_LAZY_PP_RESERVE=0`) | 2377.5 | 24.06 | 22857 | 6408 | e68df05b |
| **reservation on (restored)** | 2313.1 | 23.93 | **21318** | **4430** | e68df05b |

So VRAM -1978 MiB and RSS -1539 MiB with speed inside noise and the exact output unchanged - the historical figures
(6404 -> 4428) reproduce on this machine. Nothing was rolled back: the full 307-module port and the restored pinning
both stay. Backups: `/tmp/libggml-base.pre087` and `/tmp/llama-context.cpp.pre087`.

Next mechanism from the same history: EXP101 lazy KV mapping via CUDA VMM (switch `GGML_KV_LAZY_VMM`), historically
4428 -> 3188.

## Restored EXP101: lazy KV mapping via CUDA VMM (VRAM 4430 -> 3190)

Second historical mechanism from the same pointer. `GGML_KV_LAZY_VMM` was absent from every candidate we had. Applied
`patches/kv-lazy-vmm.patch` (CUDA lazy buffer type + `llama-kv-cache` use), rebuilt `libggml-cuda` and `libllama`.

| switch | prefill | decode | RSS MiB | VRAM MiB | correctness |
|---|---|---|---|---|---|
| `GGML_KV_LAZY_VMM=0` | 2304.4 | 24.04 | 21318 | 4430 | e68df05b |
| **lazy VMM on (restored)** | 2305.9 | 24.26 | 21319 | **3190** | e68df05b |

-1240 MiB, exactly the historical 4428 -> 3188, with speed and the exact output unchanged. The full 307-module port,
the restored pinning and EXP087 all stay. Backups: `/tmp/libggml-cuda.pre101`, `/tmp/llama-kv-cache.cpp.pre101`,
`/tmp/llama-kv-cache.h.pre101`.

## Memory: both historical wins reproduce, and the target is beaten

| stage | RSS MiB | VRAM MiB |
|---|---|---|
| start of optimisation | 22857 | 6408 |
| + EXP087 lazy compute reservation | 21318 | 4430 |
| + EXP101 lazy KV VMM | 21319 | **3190** |

That is -1538 MiB RAM and -3218 MiB VRAM (-50%) against the start, with prefill ~2310-2365, decode 23.9-24.3 and the
accepted `e68df05b` unchanged. The historical ~3400 MiB reference was an orientation, not a limit: 3190 is below it on
the same profile.

Caveat recorded per the source: EXP101 is a saved candidate, not a universal production acceptance - the broad
compatibility/failure-mode matrix is incomplete, and a map failure aborts. Both switches (`GGML_LAZY_PP_RESERVE`,
`GGML_KV_LAZY_VMM`) exist to disable, and both are on by default.
