# EXP-2026-09-15-065: decode host-serialization readback (device-to-host, DEC-1)

Status: `REJECTED` (built + model-free gate PASS; the live decode A/B was run on 2026-09-16 and the candidate was rejected: no measurable effect, inside noise, below the 2 percent bar).
Kind: `performance` (candidate; measured null in the live decode A/B of 2026-09-16).

This card records the implementation and the model-free gate for DEC-1, and the
live decode A/B that followed, in `experiments/HYPOTHESES-2026-09-15-runtime-speed.md`.
The build and gate artifacts are under `results/archive/EXP-2026-09-16-065/`; the gate
ran on 2026-09-16, and the live decode A/B ran on the same day with its raw data under
`results/archive/EXP-2026-09-16-065/ab/`.

## Hypothesis

The scheduler's full-stream drain on the generic split-input copy fallback
(`ggml/src/ggml-backend.cpp`, the `cpy_tensor_async` fallback around line 1745) is
removable for device-to-host readback on the decode path. Expected effect, from the
design note `experiments/designs/exp057/D1-host-serialization.md`: roughly 1 to 4
percent of the decode step, central about 2 percent (at or below the 2 percent
acceptance bar).

## Candidate identity

- Source tree `work/llama.cpp-exp065-hostser`, a copy of the accepted runtime tree
  `work/llama.cpp-exp046-default`.
- Build `build/exp065-hostser`.
- Server binary `build/exp065-hostser/bin/llama-server`, sha256
  `ddbe62ddebc474f5e0a503d49219953d1561d48c64abaa8c508da4c906f05b78`.

A recursive diff of the two source trees reports exactly one differing source file,
`ggml/src/ggml-backend.cpp` (the `.git` index also differs, which is not a source
file). No other runtime file differs.

## Exact change

Only `ggml/src/ggml-backend.cpp` differs from the accepted tree. In the
`cpy_tensor_async` fallback, the original two-host-block sequence

```
ggml_backend_synchronize(input_backend);
if (sched->events[split_backend_id][sched->cur_copy] != NULL) {
    ggml_backend_event_synchronize(sched->events[split_backend_id][sched->cur_copy]);
} else {
    ggml_backend_synchronize(split_backend);
}
ggml_backend_tensor_copy(input, input_cpy);
```

is replaced for the device-to-host case by an async read on the source stream followed
by a single wait:

```
ggml_backend_tensor_get_async(input_backend, input, input_cpy->data, 0, ggml_nbytes(input));
ggml_backend_synchronize(input_backend);
```

The branch is gated by the environment variable `GGML_SCHED_READBACK_ASYNC`, default
off unless set; the exact predicate is `v != nullptr && strcmp(v, "0") != 0`, so `=1`
enables it and `=0` also disables it. It engages only when the source buffer is not
host, the destination buffer is host, and `input_backend->iface.get_tensor_async !=
nullptr`. In every other case the original path runs unchanged.

## Scope note

The design note's Candidate A (allocate scheduler events for `n_copies == 1`) and the
CPU-to-host upload async path were NOT implemented. This card covers the minimal
device-to-host readback variant only.

## Model-free gate

The depth-2 harness `test-exp041-ring.cpp` from
`results/archive/EXP-2026-09-07-041/tests/` was compiled against the exp065 tree
headers and linked to `build/exp065-hostser/bin` (harness binary
`/tmp/test-exp065-ring2`).

- `sched` output sha256
  `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510`, byte-identical
  BOTH with `GGML_SCHED_READBACK_ASYNC` unset and set to `1`. This is the accepted
  EXP-038/040/041/043/062/064 reference sha.
- The ring branch passes 9 checks with 0 failures.

Harness/mirror note: running the depth-3 harness `test-exp062-ring` against the exp065
libraries produces spurious ring-check failures, because its mirror arrays are `[3]`
while exp065 is depth 2 (two pinned slots). That is a harness/mirror mismatch, not a
candidate regression, and it is why the depth-2 harness was used.

## Explicit non-claims

- No model run and no performance number.
- The expected 1 to 4 percent is unmeasured; this card makes no speed claim.

## Next step

A live decode A/B against the accepted runtime (flag off versus on, same binary)
requires Igor's approval under gate G4 before it is run.

## Run protocol (literal commands, already executed; do not re-run from this card)

```bash
cd /home/end0/AI/FrankenMoE-CUDA

# 1. Build the accepted-runtime (EXP-041) depth-2 harness against the exp065 tree.
g++ -std=c++17 -O1 -o /tmp/test-exp065-ring2 \
    results/archive/EXP-2026-09-07-041/tests/test-exp041-ring.cpp \
    -I work/llama.cpp-exp065-hostser/ggml/include \
    -I work/llama.cpp-exp065-hostser/ggml/src \
    -I /usr/local/cuda-12.6/include \
    -L build/exp065-hostser/bin -Wl,-rpath,build/exp065-hostser/bin \
    -l:libggml-cuda.so -lggml -l:libggml-base.so \
    -lcudart -L/usr/local/cuda-12.6/lib64

# 2. sched mode, GGML_SCHED_READBACK_ASYNC unset (off).
/tmp/test-exp065-ring2 sched /tmp/exp065-off.bin

# 3. sched mode, GGML_SCHED_READBACK_ASYNC=1 (on).
GGML_SCHED_READBACK_ASYNC=1 /tmp/test-exp065-ring2 sched /tmp/exp065-on.bin

# 4. ring branch at the default-on ring; expect 9 checks, 0 failures.
/tmp/test-exp065-ring2 ring

# 5. Byte equality of the two sched outputs.
sha256sum /tmp/exp065-off.bin /tmp/exp065-on.bin
```

## Raw data

- `/tmp/test-exp065-ring2`: the compiled depth-2 harness.
- `/tmp/exp065-off.bin`: sched output with the flag unset.
- `/tmp/exp065-on.bin`: sched output with the flag set to `1`.
- `sha256sum /tmp/exp065-off.bin /tmp/exp065-on.bin` is
  `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510` for both files.

The build and the full gate record for the same candidate also live under
`results/archive/EXP-2026-09-16-065/` (`REPORT.md`, `EXP-065-ggml-backend.patch`,
`tests/run-model-free-exp065.sh`, `tests/run-model-free-exp065-ring.sh`,
`tests/log-*.txt`).

## Live decode A/B (2026-09-16)

Verdict: **REJECTED**. No measurable effect: the result is inside noise and far
below the >= 2 percent acceptance bar.

### Protocol

- One binary used for BOTH arms: `build/exp065-hostser/bin/llama-server`, sha256
  `ddbe62ddebc474f5e0a503d49219953d1561d48c64abaa8c508da4c906f05b78`.
- The only variable is the environment variable `GGML_SCHED_READBACK_ASYNC`:
  arm A = `0` (flag off), arm B = `1` (flag on).
- Port 8087; one fresh server per run; one warmup `request-short.json` followed by
  one measured `request-decode256.json`.
- Five pairs, order A/B, B/A, A/B, B/A, A/B.
- One warm run was executed first and is excluded from the table below.

### Measured decode256 `predicted_per_second`

| pair | A (flag off) | B (flag on) | paired gain (B vs A) |
| ---: | ---: | ---: | ---: |
| 1 | 18.5616 | 18.5519 | -0.052% |
| 2 | 19.0279 | 18.4201 | -3.194% |
| 3 | 18.6679 | 18.9597 | +1.563% |
| 4 | 18.9149 | 19.0439 | +0.682% |
| 5 | 19.0849 | 19.0666 | -0.096% |

- Paired gains (B vs A, percent): -0.052, -3.194, +1.563, +0.682, -0.096.
- Median paired gain: **-0.052%**; B wins 2/5.
- Arm medians: A **18.9149** / B **18.9597**, a difference of **+0.237%**.

### Correctness

The completion content sha256
`9270353f0601d5d660a61ee72b94ece46c5d7ed425321da8e88bd09f1abd9757` is identical
in all 10 measured runs.

### Interpretation

The paired median is negative and B wins only 2 of 5 pairs. The arm-median
difference of +0.237% is positive but negligible, and both readings sit inside
run-to-run noise and far below the >= 2 percent bar. The design note's own
estimate (1 to 4 percent, central about 2 percent, and at or below the bar) is not
realized at the measured workload. No change is delivered; the flag stays default
off.

### Raw data

`results/archive/EXP-2026-09-16-065/ab/`: `A1..A5` and `B1..B5` decode256 JSON plus
the paired warmup files, and the excluded `warm-*` run.
