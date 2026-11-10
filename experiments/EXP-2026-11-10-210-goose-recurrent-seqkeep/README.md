# EXP210: recurrent-memory sequence prune in Goose — correct, NOT reachable in this profile

`llama_memory_recurrent::seq_keep` (llama-memory-recurrent.cpp:302-332) drops every recurrent state
cell that does not carry a sequence, keeps only one sequence in the cells that do, and moves `head`
to the first freed slot. It is a different subsystem from the KV cell class that EXP199..209 cover:
its own cells, its own tail/head bookkeeping, and a `src` field recording which cell a copy came
from.

## Result

    FG210K_FIXTURE checks=16410 failures=0

    Apex 2542.56 / 25.94, tokens and all 128 probabilities exact

    FG210K_RECURRENT_SEQKEEP   **absent from the live log**

So the module is correct and the run is clean, but **this path does not execute in the Apex
profile**. `llama_memory_seq_keep` has no caller in this tree outside its own public wrapper, and
the "RAM recurrent prefix saved" lines in the log come from the checkpoint path, not from a
sequence prune.

This is recorded as **not-reachable**, not as a transfer. Per the project rule (port by fact), a
module that does not execute earns no coverage credit, and the file stays as the verified
implementation to switch on if a caller appears.

## Gate

16,410 checks / 0 failures over 400 randomised cell states, mirroring the native loop move by move:
`head` return value, `used` counter, and every cell's `pos`, `tail`, `src` and sequence bits.

One detail the gate forced: native sets `tail = -1` for every cell whose index differs from
`seq_id`, which is keyed on the *cell index* being compared to a *sequence id* — an odd but
deliberate line in the original. The port reproduces it literally rather than "fixing" it.

## Wiring

`integration/llama-memory-recurrent.cpp` marshals the cell fields into the flat arena Goose works
over, calls the module, scatters the decisions back and rebuilds each `std::set` from the returned
bits. Containers stay native; the prune decision is Goose's.

## Switch

`GGML_GOOSE_RECURRENT_SEQKEEP=0` keeps the native loop.
