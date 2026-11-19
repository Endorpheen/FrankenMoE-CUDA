# P6-10 closure audit: eleven items, their evidence, and what stays open

Written after the detokeniser work, because the item list was last audited before it. Each row is either an
experiment card, a gate that runs, or an explicit statement that it does not.

| # | item | evidence | status |
|---|---|---|---|
| 1 | Mirostat | identical on the selected tokens in the door comparison; the port clamps `k` at 1e6 where native reaches 1.03e9 and converts with undefined behaviour | closed, with the clamp recorded as a deliberate difference |
| 2 | Adaptive-P | identical except one probability by one ULP on 21 of 100 rounds, where native trusts a `sorted` flag its own transform invalidates | closed on the acceptance profile; the residual is recorded, not hidden |
| 3 | filter order | the profile chain identical over 120 rounds; DRY-bearing chain identical over 100 rounds with a real accept history | closed |
| 4 | RNG stream | 8000 selected tokens identical; fixed-seed reproducibility confirmed across reruns | closed |
| 5 | grammars, constrained generation | prefilter live on a constrained request; the matcher's 1.7 million candidate comparison; the codepoint window now 64, one piece in forty calls still fills it | closed as a measured port; the matcher stays opt-in because its cost is 11.7x, stated as a cost decision |
| 6 | fixed-seed reproducibility | same seed, same tokens, three reruns | closed |
| 7 | chain wired into the shipped library | 1500 top-p applications and 500 selections per lifecycle, identical in three independent runs | closed |
| 8 | ABI with `fg386s_pick` | `tools/check-abi-pairs.py`: `fg386s_pick` agrees on its five parameters, `fg386s_enabled` on none | closed, and a gate rather than a one-off check |
| 9 | proof of real calls | `FG171_PENALTIES` reached on a profile that uses it (EXP-2026-11-19-1362); the chain counters above | closed |
| 10 | the full chain on the real model | rounds identical on the real model, plus the acceptance runs on every accepted stage | closed |
| 11 | removal of the replaced C++ samplers | plan in `integration/p6-10-removal/PLAN.md` | **open, and blocked by rule 15**: no removal before Igor's acceptance |

## What "open" means here

Ten of eleven are closed. The eleventh is not waiting on more measurement: it is waiting on the acceptance that rule 15
puts before any deletion. Calling P6-10 done while the replaced bodies are still in place would be the mistake rule 15
exists to prevent, and calling it blocked would be the opposite mistake - the port is finished, the deletion is not
permitted yet.

## The two deviations that are not defects

- the mirostat `k` clamp, because copying undefined behaviour into Goose would be worse than differing;
- the adaptive-P ULP, because the native path reads a flag its own transform invalidated.

Both are off the acceptance profile, both are recorded here rather than in a commit message, and neither changes the
acceptance hash.
