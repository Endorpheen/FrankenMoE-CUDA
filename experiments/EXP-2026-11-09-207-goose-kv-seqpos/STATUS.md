# EXP207: per-sequence position index — REJECTED on performance

## Verdict

**REJECTED.** The module is functionally correct and its gate is green, but replacing the native
ordered set with a linear scan costs **59% of decode**: 26.46 -> 10.76 tok/s. Not promoted.

## Evidence

    gate:                    FG207P_KV_SEQPOS calls=99874
                             FG207P_FIXTURE checks=99874 failures=0
    live, module ON:         prefill 2323.78  decode 10.76   tokens exact
    live, module OFF:        prefill 2558.20  decode 26.46   tokens exact
    accepted checkpoint:     EXP206, 2554.17 / 26.08

Tokens and all 128 probabilities are exact in both arms, so this is purely a cost problem, not a
correctness one.

## A measurement defect I have to own

My first "control" was not a control. `run207.py:19` strips every `GGML_GOOSE_*` variable from the
environment before launching the arm, so `GGML_GOOSE_KV_SEQPOS=0` never reached the process and
both runs had the module ON. I read the resulting 10.76 as a wiring failure and blamed an ODR
layout mismatch from EXP206.

That attribution was wrong. The real control (a patched runner copy that forwards the switch,
`/tmp/run207p.py`) gives 26.46 with the module off, which isolates the cost to the scan itself.
Recorded so the same shortcut is not repeated: a switch that the harness strips is not a control.

## Why the scan is the wrong shape

`seq_pos_min` / `seq_pos_max` are called from `can_use_cell` inside the slot-search walk
(`llama-kv-cache.cpp:1080`), that is, once per tested cell on the hot path. Native answers them in
O(1) from `std::set` (`begin()->first` / `rbegin()->first`). A scan over all cells is O(cells), and
the Apex cache is 262144 cells, so the walk becomes quadratic in the worst case.

The two consumers that would justify the ordered structure are `seq_pos_get`
(`llama-memory-hybrid-idx.cpp:414`, `upper_bound`) and the min/max pair. Hybrid-idx is not
instantiated in this profile, so a port needs only min/max -- but it needs them in O(1).

## What a viable next attempt looks like

Either keep the native `std::set` and port only the decisions around it, or maintain per-sequence
min/max incrementally alongside the cell writes (O(1) update on pos_set/rm/pos_add/pos_div, O(1)
query) and validate it against the set. The second is the real port; the scan is not.

`active.json` untouched. EXP206 remains the last accepted runtime.
