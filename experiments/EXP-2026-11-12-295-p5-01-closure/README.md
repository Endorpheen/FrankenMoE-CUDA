# EXP295: P5-01 closure evidence

Criterion: "Every cell find/copy/remove/keep/shift/div/sequence operation checked for interleaving, multiple sequences
and full-capacity failures."

## Live

One accepted-profile run carries fifteen KV witnesses: `FG199K_KV_CELLS`, `_NKV`, `_SEQCP`, `_SEQRM`, `_SHIFT`, `_SLOT`,
`_UPDATE`, plus `FG205G_KV_SLOT`, `FG206D_KV_CP`, `FG209I_KV_SEQPOS`, `FG227N_KV_N_KV`, `FG228E_KV_CELL_EXT`,
`FG229I_KV_INPUT_IDXS`, `FG233C_KV_SCAN`, `FG392S_KV_FIND_SLOT`.

## Differential gate, re-run now

`tests/fixture.cpp` drives the module and a native reference over random interleavings of place, rm, seq_rm, seq_keep and
seq_count on a shared arena, comparing the return value and the full cell state after every step. Fresh build and run:

    FG199K_FIXTURE checks=74123 failures=0
    EXP199_LOCAL_GATES_PASS

This is the check for `keep`, `div` (range division through `pos_div`/`norm_range`), `remove` and the sequence operations
under multiple sequences and interleaving. The gate is a differential against the native reference, so any divergence in
the module fails it; the mutation runs that established this sensitivity are recorded in the earlier EXP199/289 sessions.

## Wired this pass

`pos_in` and `pos_div` now call the module when the switch is on (EXP294). They are dormant on Apex -- no shift or
division happens with a 32K prompt in a 262144-token context -- and the accepted profile, the state size and the state
round trip are unchanged.

## Deliberate boundary: seq_keep

The live `seq_keep` remains native: a single bitset test plus index maintenance. The module's version is arena-based and
mutates its own copy of the cell, so wiring it per cell would mean marshalling the sequence words in and out on every
iteration -- cost on the request path for a decision the differential gate already shows equal. The module form is
therefore gated, not wired, and that is a documented choice rather than an omission.

## Full capacity

`find_slot` returns `-1` when the cache is full; EXP390 gates that case with 53 703 checks and 8 mutations. Keep and div
cannot increase occupancy, so capacity failure does not apply to them.
