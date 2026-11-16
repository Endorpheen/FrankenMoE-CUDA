# EXP-2026-11-16-756 — the MoE expert-cache SLRU policy, in Goose, proven equal to the native one

Base: `cca358b7`. The expert cache nearly doubles decode (EXP-752), and its admission/eviction policy was
the last part of that hot path still written in C++ while a Goose module for it did not exist.

## 1. What was ported

`runtime/reconstructed/src/llama-moe-cache.cpp`, lines 66-167 - four decisions:

| native | line | decision |
|---|---:|---|
| `move` | 67 | doubly-linked list move between probation and protected |
| `touch` | 95 | promote a hit, demote the coldest protected slot when the protected half overflows |
| `victim` | 102 | first slot that may be taken: empty, another layer, or not selected this request |
| `plan` | 122 | dedupe ids, promote hits, give each miss a victim |

The copies, the allocations and the GPU transfers stay native. Only the choice of slot moved.

## 2. The gate

`tests/gate.py` drives a Python transcription of the native code and the Goose module with the same 400
random configurations and requires all four of: identical fills, identical hit count, identical refusal
decision, and an **identical final cache state**.

The state check matters: a policy can produce the right fills and still corrupt the lists, which only
shows on a later request. So each case runs a sequence of up to 30 requests and compares
`key_of/prev/next/prot`, both segment heads and tails, and `n_protected`.

```
slru gate: 50/50 cases match    (seed 1234)
slru gate: 400/400 cases match  (seed 99)
```

## 3. Three bugs the gate caught, all in the port, none in the native code

1. **Negative sentinel reaching a slice.** `key < 0 || seen[key % n_expert] != gen` - the index is formed
   even when `key` is the `-1` sentinel. Rewritten as nested branches so no index is ever built from a
   negative value.
2. **Scratch overflow.** The native `uniq` is a growing vector; the Goose version writes into caller
   scratch of `n_slots`, and the clone wrote before checking. Now the bound is tested before the write.
3. **Harness parser off by one level** - a bug in the test, not the module, but it produced a misleading
   "0/20 match" that looked like a policy divergence.

Recording these because they are the exact class of defect a differential gate exists to find, and a
straight transcription review had missed all three.

## 4. What this does not yet prove

The module is **not wired into the runtime**. It is proven equivalent on synthetic state; it is not yet
shown to produce the reference answer on Apex, and it is not yet measured for speed. Both are required
before it counts as migrated - rule 15 is explicit that a decision is replaced only when the wiring is
proven and the native body is removed.

The SLRU plan runs once per layer per token, so a Goose call there must not cost measurable decode. That
measurement decides whether it is accepted, not the gate alone.

## 5. Cost

2 gate runs, 450 cases total, seconds each. No model run.

## 6. Build status

`build.sh` produces the three objects the link needs and is reproducible:

```
slru-policy.o   7352 B   exports goose_init, slru_plan
slru-runtime.o 63528 B   the Goose runtime, emitted once from the pinned compiler
slru-shim.o     2544 B   gs_panic / gs_idxfail / gs_divfail
```

Verified functionally through ctypes against the built object: `[0,1,2,3]` fills four slots 0..3 with
no hits; the next request `[0,1]` fills nothing and reports two hits. Same as the native path.

Unlike the older `fgNNN` bridges, this module is not post-processed to strip the runtime calls, so it
links the emitted runtime. That is 63 KB of runtime for one module - acceptable, and the alternative
(re-writing the emission into the fgNNN style) is a bigger change than the policy itself.

## 7. What remains before it can be called migrated

1. Call `slru_plan` from `moe_cache_slru::plan` in `llama-moe-cache.cpp`, behind a gate that defaults on
   (rule 10), with the native loop kept as the fallback.
2. Rebuild `libllama-exp371.so` and check the reference hash still matches at 128/256/512.
3. Measure decode: the plan runs once per layer per token (41 times per token), so a Goose call there
   must be free. If it costs decode, it is rejected regardless of the gate passing.
