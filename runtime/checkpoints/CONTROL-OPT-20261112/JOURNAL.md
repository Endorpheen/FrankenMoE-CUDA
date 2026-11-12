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

## Method applied to the remaining patches: most do not fit this profile

Swept the other historical patches for the same kind of lost win. Checked for their switches in our build:
`GGML_EXPERT_PINNED_RING`, `GGML_MOE_EXPERT_BOUND`, `GGML_EXPERT_OVERLAP` - all absent, so none is active.

But reading the conditions shows why they are not the missing 9%: `expert-h2d-compute-overlap.patch` only engages when
`sched->callback_copy` exists and the split copies expert weights to the device (`GGML_OP_MUL_MAT_ID`, src[2] rows >= 32);
`pinned-ring` is a host->device staging path for those same copies. Our profile is pure `--cpu-moe`, so expert tensors
stay in host memory and there is nothing to overlap or stage. Enabling them together with `--moe-cache-mib` would make
them relevant, and moe-cache already changes the exact output - so that combination stays behind the correctness
decision, not behind a technical blocker.

Conclusion for this stage: the memory branch is closed (VRAM 3190, RSS 21319). The remaining prefill gap to the exp244
body is a version difference in the llama body, and the only measured route to prefill above ~2400/decode above ~24 in
this profile runs through the device expert cache, which is a correctness trade-off for Igor to decide.

## Expert cache: cause of the mismatch established, variant preserved

Re-measured on the current build (EXP087 + EXP101 + pinning), matching the argv exactly and adding one flag:

| variant | prefill | decode | RSS MiB | VRAM MiB | content |
|---|---|---|---|---|---|
| accepted | 2306 | 24.3 | 21319 | 3190 | e68df05b |
| moe-cache 512 | 2331.7 | 22.38 | 21320 | 3706 | differs |
| moe-cache 1024 | 2327.0 | 29.22 | 21320 | 4218 | differs |
| moe-cache 2048 | 2348.3 | **34.03** | 21320 | 5242 | differs |
| moe-cache 4096 | OOM | - | - | - | - |

EXP087/EXP101 did not merely add headroom: the same 2048 cache now costs 5242 MiB instead of 8456, so 34.03 tok/s
decode fits comfortably while the total stays below the pre-optimisation 6408.

Cause of `content_equal=false`, established and **not** waived: it is a logit shift, not a fault. At position 20 the
accepted run ranks " line" first (-0.917) with " sentence" second (-1.244); with the cache " sentence" leads (-0.796) and
" line" is second (-0.943). Both texts are coherent and on topic; the sampler simply takes a neighbouring branch at
`temperature=1.0`. Repeats: without cache `e68df05b` twice; with cache `9d90a010` twice - each stable.

A false trail is recorded too: an earlier comparison suggested ref and cache produced identical text, but the script
inherited the cache flag into both arms of the loop. The measured truth is above; the wrong reading is noted so it is not
repeated.

Variant saved with command, numbers, cause and rollback in
`runtime/checkpoints/CONTROL-OPT-20261112/variant-moe-cache/README.md`. It stays a diagnostic possibility until the
correctness criterion decision is made explicitly.

## Prefill: the profile plateau, measured rather than assumed

One change at a time on the accepted build (EXP087 + EXP101 + pinning), argv otherwise identical:

| variant | prefill | decode | VRAM | content |
|---|---|---|---|---|
| accepted | 2306 | 24.3 | 3190 | e68df05b |
| `--fa off` | 2296.3 | 24.50 | 3190 | equal |
| ubatch/batch 8192 | 2315.4 | 24.05 | 3190 | equal |
| ubatch/batch 16384 | 2310.6 | 24.08 | 3190 | equal |
| tb 32 + cpu-mask-batch ffffffff | 2309.5 | 24.05 | 3190 | equal |

All inside noise. Together with the earlier switch sweep (batch/KV/graph/CUDA quantizer) and the device-cache result
(prefill 2348 while decode jumps to 34), this says prefill on this profile is not limited by anything we can toggle: the
remaining headroom sits in the 32K prompt itself over a 35B Q4_K model with experts on the host and a 4070's compute.
Reaching 3000 would mean changing the profile (context, quantisation, hardware), not tuning it - that is recorded as the
honest limit rather than claimed progress.

What the profile does deliver, with the full port intact: prefill ~2306, decode 24.3 at VRAM 3190 / RAM 21319 (from
1624 / 23.6 / 6408 / 22856 at the start), and 34.03 decode at VRAM 5242 in the experimental cache variant.

## Prefill scaling curve: steady-state ceiling ~2360, not attention

Same build and argv, only the prompt length changed (the eval metric reports prompt tokens per second):

| prompt | prefill tok/s | decode tok/s | VRAM |
|---|---|---|---|
| 4K | 2009 | 27.0 | 2878 (earlier point) |
| 8K | 2141.1 | 27.14 | 2878 |
| 16K | 2315.2 | 25.48 | 2982 |
| 32K | 2306 | 24.3 | 3190 |

Throughput *rises* with length and flattens, so there is no quadratic attention penalty dominating the range - it is a
fixed per-graph cost (start-up, growth, first-batch effects) over a steady-state rate. Fitting the two largest points
gives roughly 0.00042 s/token, i.e. a steady-state ceiling near 2360 tok/s with about 0.37 s of fixed overhead.

That makes 3000 tok/s on this profile a profile question, not a tuning one: the steady state is already close to the
measured ceiling and the fixed part is under 3% at 32K. Recorded so the plateau is backed by a curve, not by a guess.

## Reproduction: the accepted build is preserved outside /tmp

The accepted stack is now kept in `runtime/checkpoints/CONTROL-OPT-20261112/artifact/` with `SHA256SUMS`, so it no
longer exists only in the staging directory or in `/tmp` backups:

| file | sha256 (prefix) |
|---|---|
| libllama-exp371.so.0.7.0 (full 307-module port, pinning, EXP087, EXP101) | 8cce13c85f193300 |
| libggml-base.so.0.27.0 (EXP087 alloc/backend) | b469d1c07c7625e7 |
| libggml-cuda.so.0.27.0 (EXP101 lazy VMM) | 9bdf9a6dae566d72 |

209 MiB total, which is the three libraries and nothing else - not a copy of the whole tree. The binaries are not
committed (too large for the repository); `SHA256SUMS` plus the patches named above reproduce them.

## Cache ladder: the cheapest way to hold decode 30+

Measured on the accepted build, one flag only:

| moe-cache MiB | prefill | decode | VRAM MiB | vs accepted VRAM |
|---|---|---|---|---|
| 0 (accepted) | 2310 | 24.1 | 3190 | - |
| 512 | 2332 | 22.38 | 3706 | +516 |
| 1024 | 2327 | 29.22 | 4218 | - |
| **1280** | 2334.2 | **30.63** | **4476** | +1286 |
| 1536 | 2344.0 | 31.74 | 4730 | +1540 |
| 2048 | 2348.3 | 34.03 | 5242 | +2052 |
| 4096 | OOM | - | - | - |

So the answer to "30+ decode with less VRAM" is 1280 MiB: **30.63 tok/s at 4476 MiB**, 766 MiB cheaper than the 2048
setting that first showed 32.81. 1024 misses the target at 29.22, which is why 1280 is the smallest usable point, and
the ladder is monotonic in both decode and VRAM - no anomaly to chase.

Every cache point still fails the exact-content check for the reason established earlier (CPU kernels vs GPU kernels,
same storage type). The variant therefore remains experimental; nothing was enabled by default.

## Cache baseline extended: 4096 MiB fits now and reaches 40.78 decode

With EXP087/EXP101 in place the cache can grow to a size that previously aborted. Measured on the accepted build:

| moe-cache MiB | prefill | decode | VRAM MiB | note |
|---|---|---|---|---|
| 0 (control only) | 2310 | 24.1 | 3190 | correctness control |
| 1280 | 2334.2 | 30.63 | 4476 | smallest 30+ point |
| 2048 | 2348.3 | 34.03 | 5242 | previous baseline |
| 3072 | 2335.0 | 37.79 | 6266 | |
| **4096** | 2324.3 | **40.78** | 7292 | largest that starts |
| 6144 | OOM | - | - | fails to start |
| 8192 | OOM | - | - | fails to start |

Decode scales with cache size up to the memory limit (24.1 -> 30.63 -> 34.03 -> 37.79 -> 40.78) while prefill stays flat
in the same 2324-2348 band, so the cache buys decode only - it is not a prefill lever and must not be sold as one.

The 4096 point uses 7292 of 12282 MiB VRAM, leaving room for the desktop, and remains inside the exact-check caveat
(compute-engine rounding). No default was changed.

## Cache baseline: no interaction with the remaining switches, and its own scaling

On the 2048 cache baseline, the two switches that were still untested in combination:

| variant | prefill | decode | VRAM |
|---|---|---|---|
| cache 2048 (baseline) | 2348.3 | 34.03 | 5242 |
| cache 2048 + ubatch/batch 8192 | 2351.7 | 34.08 | 5242 |
| cache 2048 + tb 32 / mask ffffffff | 2342.0 | 34.14 | 5242 |
| cache 2048, 16K prompt | 2367.4 | 37.66 | 5050 |

No interaction: both switches stay inside noise, so the cache does not open a new tuning axis. The 16K point is
informative in a different way - decode rises to 37.66 because a shorter context leaves a larger effective share of the
cache per step, while prefill stays in the same band. That confirms the cache is a decode lever whose value depends on
how much unique expert traffic the run produces, not on prompt length directly.

State of the two points, unchanged and reproducible:
- optimisation baseline: cache 2048 -> 2348 prefill / 34.03 decode / 5242 MiB; largest that starts is 4096 -> 40.78 at
  7292 MiB.
- correctness control only: no cache -> 2310 / 24.1 / 3190 MiB with the exact `e68df05b`.

## Rejected: hybrid placement on top of the cache

`--n-cpu-moe 36` (4 expert layers on the GPU) together with cache 2048:

| variant | prefill | decode | VRAM | content |
|---|---|---|---|---|
| cache 2048 (baseline) | 2348.3 | 34.03 | 5242 | differs |
| cache 4096 (largest that starts) | 2324.3 | 40.78 | 7292 | differs |
| n-cpu-moe 36 + cache 2048 | 2404.8 | 36.79 | 7234 | differs |

It spends nearly the same VRAM as cache 4096 (7234 against 7292) for a worse decode (36.79 against 40.78) and only +2.4%
prefill, which is inside the noise band we have measured repeatedly. Not adopted: the cache is the cheaper way to buy
the same memory, and stacking placement on top of it just adds a second rounding change for nothing.

So the search space for this profile is now genuinely explored: the cache is the decode lever, prefill is flat, and the
no-cache path stays the correctness control.

## Pinning still carries the exact path after EXP087/EXP101

Re-checked on the current build so the earlier +43% is not assumed to have survived the allocator changes:

| switch | prefill | decode | RSS | VRAM | content |
|---|---|---|---|---|---|
| pin on (accepted) | 2311.1 | 23.78 | 21319 | 3190 | e68df05b |
| pin off | 1641.1 | 23.97 | 21319 | 3190 | e68df05b |

+40.8% prefill, RSS and VRAM unchanged, exact output in both arms. So the pinning win is independent of EXP087/EXP101
rather than double-counted, and the exact path depends on it entirely.

`--mlock` is not an alternative here: it fails to start (memlock limit), so host-buffer registration remains the only
mechanism that keeps expert pages resident. That also explains why the exact path cannot be pushed further by page
residency - it is already pinned; the remaining prefill cost is compute and traffic, not faults.

## Launch overhead is not the prefill cost either

`GGML_CUDA_DISABLE_GRAPHS=1` on the accepted build:

| variant | prefill | decode | VRAM | content |
|---|---|---|---|---|
| graphs on (accepted) | 2311.1 | 23.78 | 3190 | e68df05b |
| graphs off | 2319.7 | 22.76 | 3176 | e68df05b |

Prefill is unchanged (inside noise) while decode drops, so graph launch is not what holds prefill back - it is the
opposite, graphs are earning their keep in decode. That closes the "sync/launch overhead" hypothesis, which was the last
cheap explanation available: with pinning already removing faults, the remaining prefill time is arithmetic and weight
traffic on both engines.

Practical conclusion for this profile: the exact path sits at ~2311-2320 prefill / 23.8-24.3 decode / 3190 MiB with
e68df05b; the cache path is the only measured way past 30 decode, and the balance point requested by Igor is cache 3072
(37.79 / 6266) on the same 32K load. 3000 prefill is not reachable by a switch on this profile, and that is now backed by
a sweep rather than an assumption.

## Compute-type variants: one is neutral, one breaks correctness

This build has no `GGML_CUDA_FORCE_MMQ`/`FORCE_CUBLAS` (removed upstream), so the only compute switch left is the cuBLAS
type:

| variant | prefill | decode | content |
|---|---|---|---|
| default | 2311.1 | 23.78 | e68df05b |
| `GGML_CUDA_CUBLAS_COMPUTE_TYPE=f32` | 2320.5 | 23.53 | e68df05b |
| `GGML_CUDA_CUBLAS_COMPUTE_TYPE=fp16` | 2308.1 | 23.98 | **differs** |

f32 is neutral (noise) and safe; fp16 buys 0.2 tok/s decode and breaks the exact check, so it is rejected outright - this
is the one case where a slower-and-exact option is trivially the right one.

That exhausts the compute-engine switches available on this profile. Prefill stands at ~2311-2320 with the exact output,
and every remaining lever is a memory/speed trade that has already been measured and recorded.

## Balance point 3072: stable, and better on longer generations

| run | prefill | decode | VRAM | note |
|---|---|---|---|---|
| cache 3072 (first) | 2335.0 | 37.79 | 6266 | 128 tokens generated |
| cache 3072 (repeat) | 2315.3 | 37.80 | 6266 | same, confirms stability |
| cache 3072, 256 tokens | 2318.8 | **39.10** | 6266 | longer generation |

Repeat lands within 0.01 tok/s of the first measurement, so this is not a lucky sample. With a longer generation decode
rises to 39.1: the cache warms as more experts become resident, which is the expected shape and also the reason a
short-run number understates steady-state decode.

Recommendation on the agreed 32K load therefore stands on three runs: cache 3072 -> 37.8-39.1 decode at 6266 MiB, about
-3 tok/s and -1 GiB against cache 4096, well above the 30 tok/s floor, prefill unchanged, exactness caveat unchanged.

## Golden rule accepted; cache warm-up measured across the ladder

Igor approved balanced optimisation and chose cache 3072 as the experimental base. Recorded as the operating rule:
judge the combined result over prefill, decode, RAM and VRAM on one representative profile; reasoned trade-offs are
allowed; correctness and the full port are not negotiable.

Warm-up effect, same 32K prompt, 256 tokens generated instead of 128:

| cache MiB | decode at 128 | decode at 256 | VRAM | prefill |
|---|---|---|---|---|
| 1280 | 30.63 | **31.58** | 4476 | 2327.5 |
| 2048 | 34.03 | **35.35** | 5242 | 2334.1 |
| 3072 (base) | 37.79 | **39.10** | 6266 | 2318.8 |

Every point improves with generation length as more experts become resident, so short runs understate steady state
uniformly - the ranking is unchanged, which is what matters for choosing a base. 1280 remains the cheapest way to hold
the 30 tok/s floor (31.58), and 3072 remains the balance point.

Variants kept for different trade-offs: 1280 (cheapest 30+), 2048 (mid), 3072 (base), 4096 (max decode), no cache
(correctness control, exact e68df05b).

## Short-generation check: no cliff at 64 tokens

The useful question before recommending the cache was whether a short generation starves the warm-up and drops below the
30 tok/s floor. Measured at 64 tokens:

| cache MiB | decode 64 | decode 128 | decode 256 | VRAM |
|---|---|---|---|---|
| 1280 | 30.66 | 30.63 | 31.58 | 4476 |
| 3072 | 37.23 | 37.79 | 39.10 | 6266 |

No cliff: 1280 sits just above the floor and 3072 barely moves, so the cache does not need a long run to pay off. The
practical statement is therefore "30+ from the first 64 tokens", not "30+ after warm-up", which is the more useful
guarantee for interactive use.

That closes the applicability question for the cache. Remaining caveat is unchanged and not waived: the cached path is
not bit-identical to the exact path, cause established (GPU-side expert evaluation), quality measured level on two
prompts.

## Balance point stability under the real desktop load

Three more repeats with the browser and compositor running, which is the actual working condition on this machine:

| run | prefill | decode | VRAM |
|---|---|---|---|
| v1 | 2330.9 | 37.83 | 6266 |
| v2 | 2346.2 | 37.90 | 6266 |
| v3 | 2328.6 | 37.69 | 6266 |

Together with the earlier 37.79 and 37.80, five runs span 37.69-39.10 with the 128-token ones inside 37.69-37.90, i.e.
+/-0.3%. VRAM is pinned at 6266 in every run, so no eviction drift under background GPU load. Prefill spans
2315-2346, also inside its noise band.

Conclusion: the 3072 base is reproducible under normal desktop load, not only in a quiet window - which is the condition
that matters for daily use. No new optimisation was claimed here; this is a stability record.

## RAM: bounded by the resident model, not by a cache setting

On the 3072 base, `--cache-ram 2048` instead of 0:

| variant | prefill | decode | RSS | VRAM |
|---|---|---|---|---|
| cache-ram 0 (base) | 2346.2 | 37.90 | 21320 | 6266 |
| cache-ram 2048 | 2339.9 | 37.50 | **21320** | 6266 |

RSS is identical to the megabyte, and it is the same with pinning on or off (measured earlier: 21319 both ways). So the
21.3 GiB is the resident 35B Q4_K model itself - mapped pages that the run touches - not a cache we can shrink, and not
a side effect of pinning. Lowering RAM on this profile means a different quantisation, which is a profile decision, not a
tuning knob.

That closes the memory stage honestly: VRAM was cut 6408 -> 3190 exact / 6266 balanced by real mechanisms, RAM is
model-bound, and no further RAM reduction is available inside this configuration.

## Hybrid placement on the cache base: does not fit, and the fitting variant loses

Two attempts to buy prefill by moving expert layers to the GPU while keeping a cache:

| variant | prefill | decode | VRAM | verdict |
|---|---|---|---|---|
| cache 3072 (base) | 2346.2 | 37.90 | 6266 | reference |
| n-cpu-moe 34 + cache 3072 | OOM | - | - | does not fit in 12282 MiB |
| n-cpu-moe 36 + cache 2048 | 2404.8 | 36.79 | 7234 | worse decode at nearly the same memory |

The one that fits spends 7234 MiB to gain 2.5% prefill (inside noise) and lose 1.1 tok/s decode against the base. On the
balanced rule that is a clear loss, so the combination is closed: on this card, capacity that could hold expert layers
is better spent on the device cache, which raises decode instead of marginally nudging prefill.

Prefill remains ~2311-2346 with every lever now measured - switches, placement, batch shape, launch overhead, compute
type and memory residency. Raising it further is a code-level change, not a configuration one.

## Upstream comparison exposes a decode gap; first candidate rejected on repeat

The upstream control does more than rank the product - it revealed a real weakness on our side:

| configuration | decode (128 tokens) |
|---|---|
| upstream dd6b945b, no cache | 29.62 |
| native libllama on our ggml (no Goose code) | 26.66 |
| exp244 body (214 modules) | 26.13 |
| our full port (307 modules, exact path) | 23.78 |

So ~10% of decode is lost between the native body and our full port, and it is not explained by any switch tried so far.
The first candidate - detokenisation decisions, which run once per token - looked promising at 24.75 against 23.78, but
the repeat inverted it (off 23.81, on 24.31), so it was noise and the candidate is rejected. Sampler modules off: 23.74,
also noise. This is recorded because accepting the first number would have been a false lead and a false claim.

Remaining candidate for the gap: the copied `changed` sources themselves (unconditional parts, not the `enabled()`
branches) or the link step. Deciding that needs a per-file rebuild bisection of the `changed` list, which is the next
step and is a code experiment, not a flag test.

## Bisection step 1: llama-sampler.cpp is not the decode gap

Swapped our `SRC/llama-sampler.cpp` (124 differing lines) for the flint original, rebuilt, measured three times:

| build | decode | content |
|---|---|---|
| ours (full port) | 23.78 | e68df05b |
| sampler = flint | 24.69 | equal |
| sampler = flint (repeat) | 24.65 | equal |
| sampler = flint (repeat) | 23.84 | equal |

The third repeat lands back in the noise band, and our own baseline produced 24.31 in one repeat, so the apparent +3.8%
is not established. The original file was restored immediately, the build is back to 36 modules and measures
2320.1 / 24.11 with the exact output - nothing was left in a half-bisected state.

Recorded as a negative result: the decode gap is real (native 26.66, exp244 body 26.13, ours 23.78) but it is not in this
file. Next files to bisect, one at a time: `llama-vocab.cpp`, `llama-batch.cpp`, then the `changed` list's graph/context
entries, each with a repeat because single runs in this range are clearly not decisive.

## Baseline correction: decode is 24.66, not 23.78

Before chasing the decode gap further I measured the accepted build three times in a row:

| run | prefill | decode |
|---|---|---|
| 1 | 2345.1 | 24.73 |
| 2 | 2348.1 | 24.44 |
| 3 | 2336.7 | 24.82 |
| mean | 2343.3 | **24.66** |

The 23.78 used earlier was a single low sample, and the "10% decode gap" against native (26.66) and the exp244 body
(26.13) therefore shrinks to about 5.5-7.5%. That matters for how the gap is described: it is real but modest, and the
earlier figure overstated it.

The lesson is recorded rather than quietly fixed: for decode on this profile the run-to-run spread (~0.4 tok/s) is large
enough that single measurements must not be used for comparisons - this is exactly the mistake already caught twice today
(detok and sampler). All later comparisons in this journal use repeats, and earlier single-run decode numbers should be
read as approximate.

## Bisection steps 2-3: batch and vocab are not the gap either

Three runs per variant, against the corrected baseline mean of 24.66:

| variant | decode runs | mean | verdict |
|---|---|---|---|
| baseline (full port) | 24.73 / 24.44 / 24.82 | 24.66 | reference |
| llama-batch.cpp = flint | 24.50 / 24.73 / 24.32 | 24.52 | no effect |
| llama-vocab.cpp = flint | 25.13 / 24.73 / 24.92 | 24.93 | inside spread (baseline alone spans 24.44-24.82) |

Both files were restored immediately after measuring; the build is back to the full 36-module configuration. Neither
file carries the remaining ~6%.

What this rules out is more useful than what it finds: the gap is not a single copied translation unit. It is either a
combination, or a build-level effect (the modules are compiled by gcc-13 at -O2 while the flint objects come from CMake,
and the link uses /usr/bin/c++), or a body-version difference. Distinguishing these needs profiling rather than more
swaps, and on this machine profiling is limited (perf_event_paranoid, no attach).

Practical consequence: ~6% decode is not worth risky restructuring of the ported path. It is recorded as a known,
bounded difference with the measurements to back it, and the effort stays on the balanced configuration where the cache
delivers 37.7-39.3 decode.

## The decode spread is ~1.2 tok/s, so this bisection cannot resolve 6%

Compiled the modules at -O3 instead of -O2 to test the build-level hypothesis, three runs: 23.51 / 23.54 / 24.06
(mean 23.70) against a baseline mean of 24.66. Looked like a regression, so I reverted to -O2 and re-measured: 23.02 /
24.19 / 23.87 (library size back to the exact baseline 5533896). The reverted build should be identical to the baseline,
yet it now reads 23.7 on average.

Conclusion, and it is a methodological one: decode on this profile varies by up to ~1.2 tok/s between runs, so:
- the "-O3 is worse" reading is **not** established either, and is withdrawn as a claim;
- no single-file or flag bisection can resolve a 6% (about 1.5 tok/s) gap with this noise;
- the earlier baseline mean of 24.66 was a favourable series, not a fixed constant.

What stands: the correct configuration is restored (-O2 for modules, 36 modules, baseline library size, content equal in
every run), and the balance point with cache 3072 is measured over many runs and remains 37.7-39.3. The exact-path
decode should be quoted as "about 24 tok/s, spread 23.0-24.8", not as a single figure.

Further decode bisection on this machine is not worth the measurement error without a profiler; that is recorded as a
stopped line, not as a solved one.

## Cache 3584: better decode per MiB than either neighbour

Three runs, stable to 0.1 tok/s:

| cache MiB | decode | VRAM | delta from previous |
|---|---|---|---|
| 3072 (base) | 37.79 | 6266 | - |
| **3584** | **39.61** (39.55/39.65/39.62) | 6778 | +1.82 tok/s for +512 MiB |
| 4096 | 40.78 | 7292 | +1.17 tok/s for +514 MiB |

So the ladder is not linear: the step to 3584 buys 3.6 tok/s per GiB, the next step only 2.3. On Igor's balance rule
(about 3 tok/s per 2 GiB is acceptable) the 3584 point is the more efficient buy, and it is also the most stable
measurement in the whole ladder.

The approved base stays 3072 until Igor says otherwise; 3584 is recorded as the measured candidate to move to, with the
numbers behind it, and 4096 stays as the maximum-decoder point.

## 3584 now has the same evidence base as 3072

Two runs per length, same 32K prompt, VRAM 6778 in every run:

| generated tokens | cache 3072 | cache 3584 | delta |
|---|---|---|---|
| 64 | 37.23 | 37.89 (38.12 / 37.65) | +0.66 |
| 128 | 37.79 | 39.61 (39.55 / 39.65 / 39.62) | +1.82 |
| 256 | 39.10 | 41.01 (40.93 / 41.09) | +1.91 |

3584 is ahead at every length, the margin grows with generation length as the cache warms, and its spread is the
tightest in the ladder. Prefill stays 2324-2327 in all runs, so the extra 512 MiB buys decode only.

That makes 3584 the best-evidenced candidate to replace the approved base: same measurements, better numbers, +512 MiB.
It stays a candidate - the base is Igor's call, and the exactness caveat applies equally to both cache points.

## Cache 3584 at 512 tokens: 41.94, no eviction

Two runs at 512 generated tokens: 41.95 and 41.93, VRAM 6778 both times (identical to the 64/128/256 points), prefill
2325.9-2327.6.

Full curve for the candidate, one profile, 32K prompt:

| generated | decode |
|---|---|
| 64 | 37.89 |
| 128 | 39.61 |
| 256 | 41.01 |
| 512 | **41.94** |

Decode rises and flattens near 42, and VRAM never moves, so the cache does not start evicting within a 512-token
generation - the earlier thin margins were warm-up, not pressure. This is the shape a stable configuration should have,
and it makes 3584 the strongest measured option: 42 tok/s at 6778 MiB, versus 24 at 3190 on the exact path.

I did not shrink `-c` to save VRAM: that would change the profile and make the numbers incomparable, which is against the
rules of this comparison.

## 3584 under real background load

Three runs while the desktop was busy; total GPU use peaked at 10715-10779 MiB of 12282, i.e. the card was heavily
loaded by the environment, not by us:

| run | prefill | decode | own VRAM |
|---|---|---|---|
| 1 | 2298.3 | 39.16 | 6778 |
| 2 | 2247.5 | 39.67 | 6778 |
| 3 | 2219.9 | 38.54 | 6778 |
| mean | 2255.2 | **39.12** | 6778 |

Two readings, kept separate on purpose:
- decode holds at 39.12 (single-run spread 38.5-39.7), and own VRAM never exceeds 6778, so the cache does not start
  evicting under background pressure - the same conclusion as for 3072.
- prefill drops to 2255 from 2326 measured under a quieter window. That is a load artefact, not a regression of the
  candidate: the environment occupied up to 10.8 of 12.3 GiB. It is recorded rather than hidden, and the quiet-window
  figure remains the reference for profile comparisons.

## Same-window comparison: the cache does not move prefill

Both configurations measured back to back in one window (desktop idle during the runs, 4-12% GPU utilisation):

| variant | prefill | decode | own VRAM | total GPU use peak |
|---|---|---|---|---|
| exact path | 2220.8 | 23.34 | 3190 | 7103 |
| cache 3584 | 2226.0 | 38.86 | 6778 | 10815 |

Prefill differs by 0.2% while decode differs by 66% in the same window. That settles the question from the other side:
the expert cache is not a prefill lever at all, not even indirectly through memory pressure, and the prefill ceiling
(~2220 here, 2326 in a quieter window) moves with the environment rather than with our configuration.

So the honest summary of prefill on this profile is a band, not a number: roughly 2200-2350 depending on background
load, with the quiet-window figure used for comparisons. Reaching 3000 needs a profile change, and no configuration
lever has moved it in the whole sweep.
