# Which of the ported reuse decisions the pinned profile actually reaches

After porting all 21 `can_reuse` functions, the question that decides whether a matching hash means anything is:
which of them run in the profile that produced it? The run log answers it directly.

```
FG375S_HYBRID_MEMORY first call calls=1
FG376S_HYBRID_CONTEXT first call calls=1
FG397S_HYBRID_DELEGATION first call calls=1
FG376S_HYBRID_CONTEXT calls=100
FG375S_HYBRID_MEMORY calls=100
```

Apex runs on **hybrid** memory - attention plus recurrent (GDN) layers, exactly its 10/30 split. No `iswa` marker
appears anywhere in the log, so the sliding-window variants are not on this path.

## What that settles

| function | rows | reached by the pinned profile | so the hash is |
|---|---|---|---|
| `llm_graph_input_mem_hybrid::can_reuse` | 6 | **yes** - hybrid memory, 100 calls | evidence the port is correct |
| `llm_graph_input_embd::can_reuse` | 5 | yes - every graph build | evidence |
| `llm_graph_input_embd_h::can_reuse` | 3 | yes | evidence |
| `llm_graph_input_rs::can_reuse` | 4 | yes - hybrid carries the recurrent state | evidence |
| `attn_kv`, `attn_k`, `attn_kv_msa` | 2-3 | attention layers of the hybrid stack | evidence |
| `attn_kv_iswa`, `attn_k_iswa`, `attn_k_dsa_iswa`, `mem_hybrid_iswa`, `dsv4` | 2-8 | **no** - ISWA/DSA/DeepSeek paths are never entered | **not** evidence |
| `pos`, `out_ids`, `attn_temp`, `sampling` | 0-1 | depends on the request; pos and out_ids run | evidence where they run |

So the reference hash covers the hybrid and attention reuse decisions, and covers nothing at all for the five
ISWA/DSA variants. That is exactly why their native bodies are still in place: a green hash for them would be a
coincidence of them never being called.

## Consequence for the count

Of the 21 ported functions, roughly 12 are exercised by the pinned profile and 5 are not. The five cannot be
promoted from "ported" to "replaced" - to remove their C++ - without a check that reaches them: a smaller model
with sliding-window attention, or the standalone harnesses that construct graph inputs directly. That is the next
piece of verification work, and it is cheaper than it sounds because it needs no correctness run, only a reached
call.

## Not claimed

The presence of `mem_hybrid` in the log does not prove its rows were all evaluated - the guards mean a row can be
absent. It proves the function runs, which is the property that makes the hash meaningful for it.
