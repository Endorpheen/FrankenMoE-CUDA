# EXP-2026-11-15-755 — the cache ceiling is 5040, but 4992 stays: no gain, less margin

Base: `f9837c6f`. Probing whether the 4992 ceiling claimed in EXP-754 is exact.

## 1. The ceiling is slightly higher than 4992

At the full context 262144:

| cache | result |
|---:|---|
| 4992 | runs |
| 5024 | 41.85 @512 |
| 5040 | 41.66 / 41.87 @512, 39.47 @128, 41.32 @256 (one OOM at 256 on the first attempt, passed on retry) |
| **5056** | **OOM** |

So EXP-754's "5056+ OOM" was right, and 4992 was not the literal last step - 5024 and 5040 also fit.

## 2. But 5040 gives nothing

Matched control at the same context, reference hash at every length:

| length | cache 4992 | cache 5040 | delta |
|---:|---:|---:|---:|
| 128 | 39.49 | 39.47 | **-0.1%** |
| 256 | 41.01 | 41.32 | +0.8% |
| 512 | 41.61 | 41.76 | +0.4% |

**Within noise, and 5040 OOM'd once at 256 while 4992 never has.** A configuration that is not faster
and fails intermittently is worse, not better.

## 3. A methodological note worth recording

My first comparison put 5040's 128/256 numbers next to 4992's **ctx 65536** numbers and showed +2.0% and
+2.7%. Those were different contexts. The matched control above reverses the conclusion. This is the
third time this session a comparison mixed two conditions - the pattern is always the same: one arm was
measured earlier under different conditions, and the table did not say so.

The rule that catches it is cheap: **every comparison table must be produced from arms measured in the
same batch, or it must carry the condition in the column header.** Both errors this session would have
been visible immediately.

## 4. Decision

**Cache 4992 stays the default.** The ceiling is 5040, but 4992 is statistically equal in speed and has
never OOM'd, so the extra 48 MiB buys nothing and costs stability.

No changes committed beyond this record.

## 5. Cost

Six configurations, twenty-one runs, two OOM starts.
