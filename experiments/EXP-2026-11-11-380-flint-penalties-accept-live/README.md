# EXP380: penalty bookkeeping -- gated, integrated, unreachable through this server

## Verdict

**GATED, INTEGRATED, NOT VALIDATED.** No live witness; the point is not counted as closed.

## Transferred

| op | rule |
|---|---|
| 0 | a disabled window (`penalty_last_n == 0`) records nothing |
| 1 | the oldest token leaves the ring once it is full |
| 2 | a counter that reaches zero is erased from the map |

## Gate

| check | result |
|---|---|
| 92 draws over windows 0..4, ring sizes 0..8, counters -1..3 | 92 checks / 0 failures |
| mutations | **6/6 caught** |

Mutations: skip never fires, skip always fires, evict strictly after a full ring, evict before the ring
is full, drop only below zero, drop only exactly zero.

## Why no witness

`llama-server` does not build a `penalties` sampler: it tracks the sampled tokens itself
(`server-context.cpp` comment at the point where it remembers them) and never calls
`llama_sampler_init_penalties`. The default chain in `common/common.h` does list penalties, but the
server does not use that chain.

So `accept` is never invoked on this server, and the profile cannot exercise the port. Recorded rather
than forced.

## Run

| check | result |
|---|---|
| completion before / after restore | `e68df05b…` both |
| `FG380S_PENALTY_ACCEPT` | **0 occurrences** |
| `FG373S` / `FG345W` / `FG241U` | live |
