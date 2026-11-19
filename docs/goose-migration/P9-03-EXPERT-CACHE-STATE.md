# P9-03: expert cache, dense placement and row streaming

## Correction: the expert-cache doors are reached, and my first count was wrong

The first version of this record said `expert-lru.goose`'s 16 doors have no call site. Re-checking
with headers and `integration/` included (after the same mistake was found in P8-06) gives **five**:

| door | called from |
|---|---|
| `xl_touch` | `integration/m1/common/fg-stream-policy.cpp`, gate `s16/fg_goose_stream_gate.cpp` |
| `xl_evict_tail` | `integration/m1/common/fg-stream-policy.cpp`, `integration/edge0/pack.cpp` |
| `xl_set_budget` | `integration/m1/common/fg-stream-policy.cpp` |
| two more | the same stream-policy bridge |

`slru_plan` is called from `vendor/flint/src/llama-moe-cache.cpp` and the staged carrier, which the
first record had right. What is unchanged: the shipped *server* path does not reach these - the
callers are the m1 stream-policy bridge and the Edge0 packer, not `libllama`'s inference path. But
"zero call sites" was false and is replaced by the table above.
