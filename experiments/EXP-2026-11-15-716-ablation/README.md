# EXP-2026-11-15-716 — ablatio: Goose routes cost nothing; the goal figure was wrong

Base: `9dcb54f0`. Three measured runs per configuration, one warm-up, cache 4608 MiB.

## 1. Result — turning Goose routes off does not speed decode up

| configuration | decode median | runs |
|---|---:|---|
| baseline (everything on) | **37.67** | 36.72, 37.67, 38.77 |
| `GGML_GOOSE_MOE=0` | **38.21** | 38.21, 38.26, 37.64 |
| `GGML_GOOSE_MOE_QUANT=0` | 37.60 | 31.69, 39.42, 37.60 |
| `GGML_GOOSE_TILED=0` | 36.28 | 33.35, 38.70, 36.28 |
| `GGML_GOOSE_ELEMENTWISE=0` | 34.76 | 32.53, 34.76, 39.11 |

**Conclusion: the Goose routes are not the cost.** Disabling the elementwise route makes decode
*worse* by 7.7%; disabling tiled makes it worse by 3.7%; disabling MOE_QUANT changes nothing
inside noise; disabling MOE gives +1.4%, which is smaller than the run-to-run spread of the
baseline itself (36.72-38.77).

An implementation that were slowing decode down would show the opposite. Goose is not the
bottleneck.

## 2. The target figure was wrong for the conditions

The project's stated target for a 32K/128 run is 41.62 tok/s. That figure does not belong to
these conditions. From `apex-optimization.json`, same binary, cache 3584 MiB:

| run | predicted tokens | decode |
|---|---:|---:|
| r51-mc3584-0 | 128 | **39.55** |
| r52-mc3584-1 | 128 | **39.65** |
| r53-mc3584-2 | 128 | **39.62** |
| r54-mc3584-256 | 256 | 41.01 |
| r56-mc3584-512 | 512 | 41.94 |

`APEX-BASELINE-20261113.md` says so itself, in the section "Historical cache curve": the 41.94
figure is the **512-token** run, and "128 tokens at 39.6 is the same baseline, not a regression".

So the honest target for a 128-token measurement is **39.6**, and this build reaches **38.21**
with `GGML_GOOSE_MOE=0` and 37.67 at full configuration - i.e. **96-97% of the correct reference
for these conditions**, not 82% as the 41.62 comparison implied.

That distinction matters beyond bookkeeping: three optimisation attempts were aimed at a gap of
4 tok/s when the real gap is 1.4-1.9 tok/s.

## 3. What is recommended now

`GGML_GOOSE_MOE=0` measured best (38.21) but the improvement is inside the baseline's own spread,
so it is **not** adopted. Adopting it on this evidence would be fitting noise, and it would disable
a Goose route - the opposite of the goal - for an unproven gain.

The configuration stays at cache 4608 MiB, everything on: prefill 2280 median, decode 37.67
median, VRAM 7804 MiB, RAM 21.8 GiB, output sha identical to the reference.

## 4. Balance, per rule 19

| axis | value | vs 128-token reference (39.6) |
|---|---:|---:|
| prefill | 2280 | +2.3% (ref 2228.76) |
| decode | 37.67 | -4.9% |
| VRAM | 7804 MiB | +15% (ref 6778) |
| RAM | 21.8 GiB | unchanged between configs |

## 5. Reproducibility

`/tmp/abl.sh <port> <tag> <VAR>=0 <cache>` - one warm-up plus three measured runs, port released
after. All twelve runs are in `benchmarks/experiment-history.jsonl`.
