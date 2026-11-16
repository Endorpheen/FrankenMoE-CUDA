# EXP-2026-11-16-757 — the protected fraction, and why the two-segment design is worth 15 points

Base: `a68bd4d9`. TRANSFER-BUDGET closed off bandwidth and copies as the limit and pointed at the hit rate.
This sweep asks whether the SLRU's protected fraction is one of the knobs that moves it.

## 1. The curve (hit rate and upload are deterministic counters, not timings)

All at 4992 MiB cache, 61440 plans, reference argv:

| protected fraction | hit rate | upload |
|---|---:|---:|
| pure LRU (never demote) | **69.3%** | 278004 MiB |
| 1/2 (native, shipped) | 83.7% | 149006 MiB |
| 3/4 | 84.7% | 139581 MiB |
| 7/8 | **84.8%** | 139062 MiB |

**The two-segment design is worth ~15 points of hit rate.** Pure LRU drops from 83.7% to 69.3% and decode
from 39.95 to **33.54** - a 16% throughput loss from the policy alone. That is the clearest evidence yet
that this policy matters, and it is the reason EXP-756 was worth doing.

Beyond 3/4 the curve is flat: 84.7% -> 84.8%, upload 139581 -> 139062 MiB. So 3/4 captures the gain and 7/8
adds ~0.1 point.

## 2. Matched decode comparison

| length | 1/2 | 7/8 | delta |
|---:|---:|---:|---:|
| 128 | 34.60 (32.47-38.69) | 39.42 (32.77-40.39) | +13.9% |
| 256 | 37.63 (37.26-38.18) | 38.38 (38.13-39.00) | +2.0% |
| 512 | 39.95 (39.27-40.97) | 40.44 (38.62-40.51) | +1.2% |

**The 128 figure is not trustworthy**: the two ranges overlap almost completely (32.47-38.69 vs
32.77-40.39) and 1/2's samples span 6 tok/s. 256 and 512 are tighter and agree at +1-2%.

Answers identical at every length in both arms - as they must be, since eviction only chooses cache versus
host for byte-identical weights.

## 3. Why this is not shipped yet

The differential gate certifies that the Goose policy **implements the native policy**. Changing the
fraction to 7/8 deliberately breaks that equivalence, and the gate then correctly fails. Shipping 7/8 means
either updating the gate's reference to 7/8 (and losing the "equals native" proof) or keeping the native
reference as a provenance check and adding a second one for the chosen policy.

That is a decision about what the gate is for, not a measurement, and it should be made explicitly rather
than by quietly editing the expectation - which is exactly the mistake S15-GATE-20261116 recorded.

**So the shipped policy stays at 1/2**, the gate stays green (verified 100/100 after restoring it), and 7/8
is recorded as a measured candidate with its numbers.

## 4. What the next step would be

1. Decide the gate's role: provenance (native) or specification (chosen policy).
2. If specification: update `tests/gate.py`'s reference along with the module, re-run, then take 7/8 and
   re-measure all three lengths with paired repeats for a real verdict.
3. Expected gain from the deterministic counters: hit rate +1.1 points, upload -6.7%. Decode gain is inside
   the noise at 256 and 512, so the honest expectation is small.

## 5. Resolved: the fraction is a parameter, and the gate checks both claims

The open question was whether the gate means provenance (equals native) or specification (implements the
chosen policy). **It should mean both, and they are different checks - so the fraction stopped being a
constant.**

`slru_plan` now takes `prot_limit` as an argument. `fg756s_prot_num()` / `fg756s_prot_den()` supply it,
defaulting to **7/8** and reading `GGML_GOOSE_SLRU_PROT=num/den` so `1/2` restores native parity exactly.
The gate gained `--prot`, and its Python reference uses the same fraction.

Both arms verified after the change:

```
gate --prot 1/2   100/100 cases match     provenance: Goose == native
gate --prot 7/8   100/100 cases match     specification: Goose == chosen policy
```

So neither claim is traded away. Changing the policy no longer breaks certification - it moves the
specification arm and leaves the provenance arm untouched.

## 6. Accepted at 7/8

Matched, same day, 4992 MiB, reference argv:

| | 1/2 (native) | 7/8 (chosen) |
|---|---:|---:|
| decode @512 | 39.81 (39.27-42.01) | **40.43 (40.26-40.43)** |
| hit rate | 83.7% | **84.8%** |
| upload / 61440 plans | 149006 MiB | **139062 MiB** (-6.7%) |
| answer hash | `c2ff42cc1219` | `c2ff42cc1219` |

The ranges do not overlap at 512 and 7/8 is tighter, which is the first decode difference this session that
is not explained by noise. The deterministic counters move the same way. Answers identical, as they must be.

Reproduction check: with the parameter removed and the fraction hardcoded, the counters were
`h=418933 m=75163 up=139061.5`; through the parameter they are the same to the last digit.

Rollback: `GGML_GOOSE_SLRU_PROT=1/2` restores the native policy, and `GGML_GOOSE_MOE_SLRU=0` restores the
native code.
