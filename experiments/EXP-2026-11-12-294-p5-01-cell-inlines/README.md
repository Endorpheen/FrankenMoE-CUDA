# EXP294: cell-level pos_in and pos_div wired to the module

P5-01's inventory found that `llama_kv_cells` keeps `pos_in`, `pos_div` and `seq_keep` as inline methods and that the
first two were still native while the module exports their counterparts.

## What changed

- `pos_in(i, p0, p1)` calls `fg199k_pos_in` when the lower bound is normalised (`p0 >= 0`) and the switch is on; the
  module traps on a negative lower bound, so the unnormalised case keeps the native expression.
- `pos_div(i, d)` asks `fg199k_pos_div` for the new position and shift (the module returns both, computed as
  `pos/d` and `shift + (pos - pos/d)`), then applies them and maintains the per-sequence index natively as before.

Three TUs were recompiled against the patched header and substituted in the relinked `libllama`.

## Gates

| check | result |
|---|---|
| accepted profile | `e68df05b...`, 128 tokens -- unchanged |
| state file, 12-token prompt | 131 975 288 bytes, exactly as in EXP293 |
| self restore of that file | `n_restored = 12`, continuation `1da0f499...` -- exact |

## Liveness, stated honestly

The wiring is **dormant on the accepted profile**. The module's `pos_in` counter is 2 359 296 in both EXP293 and EXP294,
byte for byte identical, and `FG199K_KV_SHIFT add=0` shows no position shift happens at all: a 32K prompt inside a 262 144
token context never enters the shift, division or range-removal paths where these inlines run. The call sites exist
(`llama-kv-cache.cpp:609/664/783/832`), but on Apex they are not reached.

So this experiment demonstrates compile-level correctness and regression safety, not live traffic. Proving the branch is
taken needs a scenario that shifts or divides positions, or a direct unit gate; neither is in this run.

`seq_keep` remains native and is the remaining P5-01 wire: the module's version mutates its own arena, so it needs the
cell's sequence words marshalled in and out (or a decision-only entry point) rather than a one-line call.
