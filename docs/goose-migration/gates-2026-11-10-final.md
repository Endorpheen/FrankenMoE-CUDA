# Full gate run, end of session — 25 fixtures, 1,248,346 checks, 0 failures

    python3 scripts/run_all_gates.py
    25 fixtures, 1248346 checks, 0 failures

The suite has grown from 17 fixtures / 914,112 checks at the start of this stretch to 25 / 1,248,346. Every
fixture rebuilds from the **committed** generated sources, not from regenerating the `.goose`, because the
committed artefacts are what the chain ships.

## What the gate suite now covers that it did not before

| stage | what it checks |
|---|---|
| EXP227 | `get_n_kv` padding arithmetic, 11 edge cases + 5000 random |
| EXP228 | `llama_kv_cell_ext`, **exhaustive** over all 2^6 boolean combinations |
| EXP229 | graph-input index fill, 6 named cases built to catch two specific orderings |
| EXP230 | `set_input_k_shift`, ragged stream widths and empty cells |
| EXP231 | recurrent `seq_add`, the negative-`p1` and exclusive-bound edges |
| EXP232 | state-write range grouping, the discontinuity rule |
| EXP233 | KV scan helpers, OR-accumulation and off-by-one |
| EXP235 | MMQ Q4_K inner product, 30000 random against a native transcription |

## What this does NOT cover, stated plainly

- **Chain composition.** Green fixtures do not prove a module is in the runtime. That is
  `check_witnesses.py --baseline` plus `nm -u` on the objects, both used for every stage this session.
- **The CUDA kernels.** They are verified by byte-compare against the native kernel on GPU and by exact
  Apex runs, not by these fixtures. EXP235's GPU differential (`worst_rel=0` on 65536 elements) is the
  newest and strongest of those.
- **Anything about speed.** No performance claim rests on this file.

## Why it is worth re-running rather than trusting the last run

Two of the mutations I used this session initially "passed" because my `sed` pattern silently did not
match, and one integration wrote correct output from the wrong code path because it forgot to skip the
native loop. Both classes are invisible to a fixture that is not executed, so the number above is only
meaningful because it is produced by running every one of them now.
