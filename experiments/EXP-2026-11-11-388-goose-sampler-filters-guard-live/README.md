# EXP388: DRY / top-n-sigma / XTC guards -- integrated, unreachable on this server

## Verdict

**GATED, INTEGRATED, NOT VALIDATED.** No live witness; the point is not counted as closed.

## Transferred

| op | rule |
|---|---|
| 0 | DRY does nothing at `multiplier == 0` or an empty penalty window |
| 1 | DRY does nothing when `base < 1` |
| 2 | top-n-sigma does nothing at `n <= 0` or fewer than two candidates |
| 3 | XTC does nothing at `probability <= 0` or fewer than two candidates |
| 4 | XTC does nothing when `threshold > 0.5` |

## Gate

| check | result |
|---|---|
| 76 draws over five parameter values × window sizes, all five guards | 76 checks / 0 failures |
| mutations | **5/5 caught** |

## Why no witness

`FG388S_SAMPLER_FILTERS` never fires. The server builds its chain from its own sampler list, not from the
default list in `common/common.h`; `llama_sampler_init_dry` / `_top_n_sigma` / `_xtc` are only created
when their types appear in that list, and this server's list does not contain them. Same category as the
penalties sampler (EXP380): the code is guard-correct but nothing on this server calls it.

Recorded rather than claimed. Reaching these needs a request that names those samplers, which the agreed
profile does not do.
