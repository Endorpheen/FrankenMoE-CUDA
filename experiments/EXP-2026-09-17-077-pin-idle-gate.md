# EXP-2026-09-17-077: idle-gated pin worker (PRE-PIN3b)

Status: `MODEL-FREE DONE` and `LIVE A/B DONE - NULL` (2026-09-17).
Kind: `prototype-offline` plus a live model A/B.
Hypothesis: `PRE-PIN3b` in `experiments/HYPOTHESES-2026-09-15-runtime-speed.md`.
Base: `experiments/EXP-2026-09-16-076-pin-async.md` and its archive `results/archive/EXP-2026-09-16-076/`.
Isolated tree: `work/llama.cpp-exp077-pin-async2`. Build: `build/exp077-pin-async2`.
Full implementation and gate record: `results/archive/EXP-2026-09-17-077/REPORT.md`.
This card summarizes and does not duplicate every table.

## Hypothesis

EXP-076 moved registration off the request thread, but released the worker when the FIRST
`set_tensor_async_pinned_ring` call returned, i.e. in the middle of the first prefill. The
worker then pinned about 24 GiB while the first requests were still running and fought them
for NVMe and CPU: the EXP-076 long session measured prompt 1 at `-65.3 percent` and prompt 2
at `-18.8 percent`, while prompts 3 to 12 were faster in 10/10 (median `+15.0 percent`).

PRE-PIN3b tests whether holding the worker back until the request stream goes quiet (idle
gate) and making it gentler (smaller batches, longer sleeps) removes the first-request
regression while the later-request win stays.

## Mechanism

The change lives in the isolated tree only (exact diff in `pinsrc-idlegate.patch`, sha256
`0e46ddf45aa105e54225c3c10ca6435abb0e8b81326ab44e522083f45c79f46b`), in
`ggml/src/ggml-cuda/common.cuh` and `ggml/src/ggml-cuda/ggml-cuda.cu`. Everything from
EXP-076 is kept: `GGML_EXPERT_PIN_ASYNC` default ON (`0` reproduces EXP-075), no registration
on the request thread under async, a request is either fully direct or fully through the ring,
the physical-region clamp, and the worker joined before any `cudaHostUnregister`.

Idle gate (new). The worker needs two conditions before it starts a batch: the EXP-076
first-dispatch guard, so it never starts before the request path was exercised; and the idle
gate `now - last_enqueue_ts >= GGML_EXPERT_PIN_IDLE_MS` (default 750 ms), so it does not
compete with an active request stream. `ggml_cuda_expert_pin_enqueue_locked` stamps
`expert_pin_last_enqueue` on every enqueue (and `expert_pin_first_enqueue` on the first one)
under the pin mutex. A deferral is counted once per arrival at the gate, not per wakeup, and
`ggml_cuda_expert_pin_try_async` skips `notify_one` while the worker is in the idle wait so a
long prefill does not cost one wakeup per enqueued range. `GGML_EXPERT_PIN_IDLE_MS=0` disables
the gate and restores the EXP-076 start behaviour.

Gentler throttle (new defaults, both env-overridable and read once through cached helpers, so a
live A/B can sweep them without a rebuild):

| knob | EXP-076 | EXP-077 default | env override |
| --- | ---: | ---: | --- |
| batch cap | 64 MiB | 4 MiB | `GGML_EXPERT_PIN_BATCH_MB` |
| inter-batch sleep | 20 ms | 100 ms | `GGML_EXPERT_PIN_BATCH_SLEEP_MS` |

New counters: `idle deferrals` (times the idle gate held a ready batch back) and
`first batch delay` (from the first enqueue to the start of the first batch). Both are logged
on a dedicated worker line and appended to the release summary, whose field count grew from 17
to 19; the harness parses the longest format first, so the EXP-075 and EXP-076 prefixes still
parse.

## Model-free gate

Full detail in `results/archive/EXP-2026-09-17-077/REPORT.md`, section 4. Anchors:

| gate | result |
| --- | --- |
| 19 hash lists (async pass 1 ring+enqueue, async pass 2 all-direct, async off, register-fail, ring only, three stress iterations, saturation, concurrency with idle 0, idle mode, baseline) | all `5bbf98f3f18049105f5fe7e758a6bda033c67d2fc1accf3bf5b7b528e90dcde8` |
| scheduler invariant (async ON, async OFF, split off, register-fail) | `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510` in all four modes |
| idle-gate case (new) | 8 PASS / 0 FAIL |
| teardown | `joined 1`, `bytes_still_pinned 0`, `VmSwap 0` |

The new idle case reported 8 PASS / 0 FAIL including `worker issued no batch while the request
stream was active`, `first batch waited at least the idle threshold` and `post-drain pass
served every range directly`. Measured idle behaviour: the first batch opened after 2538 ms
(stream plus the 750 ms window) with one idle deferral; after the stream went quiet the worker
drained 299 batches and the second pass was fully direct.

Patch `pinsrc-idlegate.patch` sha256
`0e46ddf45aa105e54225c3c10ca6435abb0e8b81326ab44e522083f45c79f46b`. Build
`build/exp077-pin-async2`, `libggml-cuda.so.0.22.0` sha256
`5edac915efe5cafd52ecaf63b2c467a4eff9d47e9e8cc25fbb292722ac50d5d5`; the copied build
directory was relinked and its RUNPATH verified self-referential, so the executable loads its
own `libggml-cuda.so.0.22.0`.

## Live A/B

Protocol: one binary for both arms (`build/exp077-pin-async2`), the only variable is
`GGML_EXPERT_PIN_SOURCE` (A = `0`, ring; B = `1`, async with the idle gate). Port 8087, one
fresh server per run, 12 different prompts per run. Eight runs in the order A1 B1, B2 A2,
A3 B3, B4 A4, so each arm has two first-position and two second-position runs and the
page-cache warm-up cancels. All 96 requests returned 200. Raw data:
`results/archive/EXP-2026-09-17-077-long/ab/`.

| point | median paired gain | per-pair gains |
| --- | ---: | --- |
| prompt 1 | +5.0% | +49.8, +6.5, +3.5, -27.9 |
| prompt 2 | +5.2% | +31.5, +4.2, +6.1, -5.5 |
| prompts 3 to 12 | +0.6% (median of the per-prompt medians) | - |
| session total | -1.6% (A 48410 ms / B 47616 ms) | - |

Prompt 1 stays noisy: the first ring run paid a cold NVMe read, 12013 ms against about 4500 ms
in the other runs, which is what leaves the `-27.9` per-pair entry. Session total is the sum of
the 12 per-prompt means per arm.

## Verdict

`NULL`. Prompts 1 and 2 land within a few percent of the ring arm, prompts 3 to 12 give a
`+0.6 percent` median, and the session total is `-1.6 percent`, inside noise. No request-level
or session-level gain survives.

Why it is a null, from the counters. Over the whole session the worker registered only about
500 to 610 intervals and about 1.2 GiB, because the shipped throttle (4 MiB per batch, 100 ms
sleep) caps it near 40 MB/s while a session needs on the order of 24 GiB and roughly 1 GB/s to
keep up. The session therefore recorded about 2300 direct ranges against about 207000 fallback
ranges: almost every request was served by the ring, so the async arm behaved like the ring
arm for the whole run. The idle gate itself worked as designed (first batch 4.4 to 6.2 s in,
one idle deferral per run, the worker stayed silent while prompts were running), so it removed
the competition with the first requests AND the benefit at the same time. The gate and the
throttle were introduced together; this A/B cannot separate them, and the counters point at
the throttle as the thing that starved the worker.

## Not covered

- PRE-PIN3c is the next step: keep the idle gate, remove the throttle (64 MiB batches, 0 ms
  sleep, both still env-overridable) so the pinning happens only inside the quiet windows
  between prompts, where nobody is waiting. It is being implemented as
  `EXP-2026-09-17-078` in another session and is in progress, not a result.
- No decode measurement in this A/B; only prefill `prompt_ms` was recorded.
- The idle threshold 750 ms and the batch-size/sleep pair are still bounded guesses; the
  PRE-PIN3c sweep is what removes the throttle, not the threshold.
- The expiry of the budget cap over a long session was not re-measured here; the fallback
  counters above are the direct evidence that most ranges never got registered.
