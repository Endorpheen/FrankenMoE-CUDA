# EXP374: disable contract on the reconstructed tree

The three routes added on the reconstructed tree can be switched off without changing the answer.

## Verdict

**CONTRACT HOLDS.**

## Setup

    GGML_GOOSE_KV_STATE_WRITE=0 GGML_GOOSE_KV_STATE_CLEAR=0 GGML_GOOSE_KV_READ_SINFO=0

on the reconstructed DSO, with the accepted runtime's own switches left alone.

## Result

| check | result |
|---|---|
| completion before save | `e68df05b…` |
| save | 387 989 920 bytes |
| restore (successful) | `n_restored` |
| completion after restore | `e68df05b…`, probabilities equal |
| `FG373S` / `FG345W` / `FG367S` | **0** occurrences |
| `FG241U` / `FG237S` / `FG227N` (accepted) | live |

So the new routes behave like the old ones: a build that enables them by default returns to the native
behaviour on request, the answer never depends on them, and the accepted routes are untouched by the
switch.

## Why it matters here

This tree is the only candidate where the accepted routes and ours run together, so it is exactly where
a bad switch (one that disabled a neighbour, or one that changed behaviour instead of removing it)
would have shown up. Neither happened.
