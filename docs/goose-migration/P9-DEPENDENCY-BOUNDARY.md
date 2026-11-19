# Where the rest of the P9 family can be verified, and where it cannot

Measured 2026-11-19, after P9-01 closed.

## What is verified against compiled native code

| item | native reference | harness |
|---|---|---|
| recipe registry | `bmoe_core` symbols | EXP-1248, 106 cases |
| config validation | `bmoe_core` symbols | EXP-1252, 4000 configs / 44 rules |
| ngram draft | `bmoe_core` symbols | EXP-1249, 3000 cases |
| recipe matcher | `router_hook.cpp` white-box include | EXP-1255, 4160 cases |

All four link `bmoe_core` built with `llama.cpp present = OFF`, so they need no model and no GPU.

## Why the rest cannot follow the same route

| item | native home | why it is out of reach |
|---|---|---|
| P9-03 dense weights, streaming, LRU | `ExpertStreamSource` | `init()` needs shard paths and real expert tensors; `entry_bytes`/`worst_cycle_bytes` read the bound geometry, so no instance can exist without a model file |
| P9-04 row streaming, router hooks, overlap | `RowStream`, `RouterHook` | `RowStream` is class-bound the same way; the decisions the port reproduces (`rs_table_verdict`, `rs_session_take`) are in `session.cpp`, which needs llama |
| P9-05 sessions, runtime | `session.cpp`, `runtime.cpp` | needs llama.cpp, and the engine is written against the **bmoe fork** - it does not compile against `vendor/flint` (EXP-1250: `common_chat_parser_params` API mismatch) |

## The options, stated without a preference

1. **Build the engine against its own fork.** Then session/streaming/LRU become reachable, and the whole family can
   be verified the way P9-01 was. This needs the pinned llama.cpp fork, which is a dependency decision.
2. **Wire the ports into the runtime that does build here.** The Q4_K kernel already follows this route (source
   integration with a counter). It needs an interface decision per family, and it would make the ported code part
   of the shipped binary rather than a library the engine never calls.
3. **Keep the synthetic gates.** They are honest but weaker: they compare against my reading, and this session
   produced seven verification errors of exactly that kind.

## What this means for DONE

The three P9 items cannot be marked DONE under the definition P9-01 met, because their decisions cannot be
compared with compiled native code in this environment. Saying so is more useful than a number that mixes the
states - which is why the coverage block separates ported, called and verified-against-native.
