# P5-07 inventory: checkpoint, prefix reuse, branch and restore

Status: inventory. The item stays TODO; this maps each clause to what exists, so the remaining work is a list of
specific gaps instead of a slogan.

## Clause by clause

| clause | where it lives | state | evidence |
|---|---|---|---|
| exact saved state | `llama_memory::state_write/state_read` | Goose decisions, live | APEX.KV: slot file 387989920 bytes, sha256 `3e216c49...`, byte-identical to the file the accepted runtime writes (EXP-400); restore round trip returns `e68df05b` with every saved probability equal |
| restore | same | Goose, live | same; plus the rejected-restore path fires the clear route (EXP-367/368) |
| rewind | `llama_memory::seq_rm` | Goose modules, gated | KV: EXP-291/341/375; recurrent: EXP-295/341. Not witnessed on a model run of its own |
| early branch | `seq_cp` (same stream EXP-291, cross-stream EXP-292, recurrent EXP-294, hybrid-idx EXP-341) | Goose modules, gated | the buffer copy itself stays native and is reported as a pair |
| prefix reuse | **`server_prompt_cache`** (`tools/server/server-task.h`, `server-context.cpp` `prompt_save`/`prompt_load`) | native, server layer | no Goose calls in `tools/server/*.cpp`; the policy decides which prompt to keep, truncate and evict |
| near-capacity | KV defrag/maintenance = P5-09; cache SLRU eviction = EXP-150 | split across two items | defrag has no Goose module; the device-cache eviction is Goose and gated |
| outputs, not KV alone | measurement rule | followed | every acceptance above compares tokens, all saved probabilities and the content hash; KV equality alone is never used as correctness |

## The one clause that does not belong to this subsystem

"Prefix reuse" is the server's prompt cache: `server_prompt_cache` keeps `server_prompt` snapshots, picks a prefix to
reuse per request and evicts by size and token limits. That is server-layer policy with its own lifecycle, and the item
that owns the server (`P8-03` queues/slots/scheduling, `P8-04` request APIs) is where it belongs. If P5-07 is meant to
include it, the clause needs its own module and gate; it cannot be satisfied from the memory subsystem, which never sees
a prompt.

## What would close P5-07 as a memory item

1. A model run that exercises rewind and branch directly (not through save/restore): remove a sequence range, copy a
   sequence, then complete, and require the same tokens and probabilities a from-scratch run produces.
2. A near-capacity run: fill the KV to its bound, then rewind and reuse, with the same output requirement. This shares
   the P5-09 defrag work.
3. A recorded decision on whether the server prompt-cache policy is in scope here or stays with P8.

## What is explicitly not claimed

The gated modules for rewind and branch prove the decisions, not their execution on a model. Save and restore are the
only P5-07 paths with a live model witness today.
