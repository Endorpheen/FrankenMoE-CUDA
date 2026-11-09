# EXP205: KV slot search in Goose (P5-01, the "find" clause)

`llama_kv_cache::find_slot` (llama-kv-cache.cpp:921-1117) is the ring-buffer walk that picks the
cells an ubatch will occupy. EXP199 ported the two decisions it calls (`head_start`,
`can_use_cell`) but not the walk itself: the wraparound, the contiguous versus per-token testing,
and the give-up condition. A wrong walk selects a cell still in use, which corrupts attention
rather than raising an error.

## Result — live

    FG205G_KV_FIND_SLOTS calls=146 tested=32907
    FG204X_KV_SET cells=32907
    2573.78 / 26.2, tokens and all 128 probabilities equal, text sha256 e68df05b

Gate: 18,380 checks / 0 failures against a direct transcription of the native loop, over 6000
random occupancy patterns with sizes 1..24, every head position, both `cont` modes and densities
from 1% to 100%. Two explicit cases: an all-free cache must succeed immediately, and
`n_tokens > size` must report no slot.

## The decision that mattered: the predicate must stay lazy

The first version took a precomputed `can_use` byte array for the whole cache. At the Apex cache
size that is 262,144 cells rebuilt on every ubatch — 2048x the work of native, which tests only
the cells it walks past. That is the same O(cache) mistake that took EXP204 to 8.34 tok/s, so it
was replaced before it ever ran.

The module now calls an extern primitive for exactly the cells it tests. The live run shows the
effect directly: 146 calls tested 32,907 cells in total, about 225 per call, not 262,144.

## How it is wired

- `source/find_slot2.goose` — the walk and `head_start`.
- `integration/bridge_find.c` — wraps the generated module and exposes it with a predicate
  pointer. Compiled as C because the generated code uses C11 atomics.
- `integration/shim.cpp` — the predicate, which is a transcription of native's `can_use` body
  (lines 1064-1082) reading the real `llama_kv_cells` through its public methods
  (`is_empty`, `seq_count`, `pos_get`, `seq_get`, `seq_pos_max`) plus `llama_hparams::is_masked_swa`.

Splitting C and C++ was necessary, not cosmetic: the generated module uses `_Atomic` and
`_Thread_local`, which C++ rejects.

## Switch

`GGML_GOOSE_KV_FIND_SLOT=0` restores the native walk.

## Carried forward

EXP204's cell copy is composed into this stage, so the chain is 175..181 + 199..205. The link must
start from the current base and add both bridges; dropping either silently reverts a module.
