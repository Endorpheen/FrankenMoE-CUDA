# The decision Igor asked for: keep the speed, take all the features

His criterion, in his words: keep the decode speed and the profile figures, and have everything that was
added integrated safely. Provenance does not matter to him; the numbers do.

## Two runnable sets, no build in between

| | A - the accepted numbers | B - everything integrated |
|---|---|---|
| path | `builds/checkpoint-20261119-EXP1476-loadable` | `builds/flint-acc/run-candidate` |
| `libllama.so.0` | `0ffd98c2` | `bea5d6af` |
| size | 5 758 048 | 5 788 128 |
| defined symbols | 6085 | 6090 |
| unit doors | **198** | **200** |
| doors only in B | - | `json_parse`, `json_dump` |
| accepted components | **20/20** | **20/20** |
| self-contained | **ok** | **ok** |
| runs | exit 0, build 2609 | exit 0, build 2683 |

Both load, both pass the component check, and B is the newer build with the two JSON doors A does not have.
Neither duplicate globals nor a missing library stands between them and a measurement.

B is the working library that had unknown provenance - which is exactly why Igor's criterion settles it: if
B holds the speed on the fixed profile, it becomes the baseline with the features in it; if it does not, A
stays and nothing is lost.

## What is needed: two measured runs, same profile, paired

```
# A - the accepted artefact, re-measured today
APEX_SERVER_DIR=$PWD/builds/checkpoint-20261119-EXP1476-loadable \
  bash tools/apex-test.sh dec-a-1476 512 4096 4096 4992

# B - everything integrated
APEX_SERVER_DIR=$PWD/builds/flint-acc/run-candidate \
  bash tools/apex-test.sh dec-b-features 512 4096 4096 4992
```

Goal: does B keep decode 62.86 / prefill 2940.9 (the EXP1476 medians) inside run-to-run spread.
Duration: two runs, each readiness plus one 512-token generation, a few minutes each, one at a time under
the `apex-test.sh` lock. Number of runs: **two**, plus repeats only if the spread is wide enough to need
them.

## The decision rule, fixed before the numbers arrive

| outcome | decision |
|---|---|
| B's decode and prefill within the spread of A, response hash identical | **B becomes the baseline**: speed kept, features integrated, A kept as the historical control |
| B slower on decode | A stays the baseline; the difference is attributed to a component and B is rebuilt without it, component by component |
| B fails to serve correctly | B is rejected outright, A stays, and the failure is diagnosed before anything else |

Both report all four axes - prefill, decode, VRAM, RAM - and the response hash, because a decode figure
alone has never been enough (rule 20).

## What this does not decide

It does not answer whether B can be rebuilt tomorrow: B's producing command is the same unknown as A's. That
question stays in `REPRODUCTION-VERDICT-20261119.md` and is not made worse by this measurement - it is a
separate risk, stated separately.
