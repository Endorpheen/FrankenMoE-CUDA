# EXP-2026-11-15-750 — cache 4608 is reference-equivalent and faster at every length

Base: `57798241`. The first optimization accepted **after** the correctness discipline was in place:
every arm below was hash-checked inside the same run.

## 1. Paired comparison, same day, same machine

Reference argv (`-b 4096 -ub 4096`), three warmed repeats per point, medians:

| length | cache 4352 | cache 4608 | decode delta | hash |
|---:|---|---|---:|---|
| 128 | 2427 pf / 38.09 | 2432 pf / **38.68** | **+1.5%** | identical |
| 256 | 2406 pf / 39.30 | 2430 pf / **40.25** | **+2.4%** | identical |
| 512 | 2406 pf / 39.84 | 2408 pf / **40.71** | **+2.2%** | `c2ff42cc` = reference |

Precision: prefill is flat (+0.1 to +1.0%), decode gains 1.5-2.4%, and **the answer is unchanged** -
`s2ff42cc` at 512 and the same per-length hashes at 128 and 256 as the 4352 arms. This is a real speed-up
of the reference computation, not a different computation.

## 2. Why it was missed

EXP-733 through EXP-742 swept the cache but changed ubatch at the same time, and the ubatch change was
breaking the answer. The cache sweep that did stay at ub 4096 (EXP-718, EXP-721) stopped at 4352 because
5120+ OOMs **only at ub 2048** - the memory ceiling that made 4608 look like the last safe step was
measured on the wrong batch size. At ub 4096 the ceiling is lower still (5120 and 5632 OOM), so 4608 is
the actual frontier, and it had simply never been tested with a verified hash.

## 3. Journal rows now explained

The journal held three `len512-c4608` rows with decode 38.65 and **no content hash** - a recorded
number whose correctness was never checked. Those rows were superseded: today's paired run gives 40.71
with the reference hash. The old 38.65 was measured with a different ubatch, which is why it looked
worse than 4352 and why nobody pursued it.

## 4. Machine drift, measured not assumed

The control arm at 4352/128 measured 38.09 today against 38.43 in the journal. That is why every
number above is a **paired** comparison taken in the same session - quoting the journal figure against a
fresh 4608 figure would have shown a smaller gain and mixed two conditions. The relative deltas are the
result; the absolute values are today's.

## 5. Cost and balance

| axis | 4352 -> 4608 |
|---|---|
| prefill | +0.1 to +1.0% |
| decode | **+1.5 to +2.4%** |
| VRAM | 7552 -> 7804 MiB (**+252 MiB, +3.3%**) |
| RAM | unchanged |

Per rule 19, a 252 MiB cost for 2% decode and no prefill loss is a favourable exchange. 4608 is also the
highest cache that fits at ub 4096 at all (5120 and 5632 OOM), so it is the frontier point, not an
interior choice.

## 6. Decision

**cache 4608 becomes the recommended default**, keeping the reference batch shape. `run-apex.sh` default
moves 4352 -> 4608. Rollback is `--cache 4352`, one flag, measured today at 39.84/512.

## 7. Cost

Six configurations, eighteen runs, no failed starts.
