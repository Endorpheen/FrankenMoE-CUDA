# EXP-2026-11-19-1421: serving the cache policy crashed, and the reason is a design fact

## What was attempted

Route 2 of EXP-1420: run the door on the cache's real state, converting only the two arrays whose type differs (`seen` to
i64, the protected flags to int32) and copying them back, then take the door's fills and hit count as the answer.

```
FG_SLRU_SERVED from_port=1
server: Аварийный останов (core dumped) during load
ggml_cuda_error ... ggml_abort
```

## Why it broke, precisely

The door updates what it is given: `key_of`, `prev`, `next`, the LRU heads and tails, and **the slot map of the layer it
was called for**. But an eviction has a second half in the native code:

```cpp
const int32_t s = victim(il);
if (key_of[s] >= 0) { slot_map[key_of[s] / n_expert][key_of[s] % n_expert] = -1; }
key_of[s] = il*n_expert + e;
```

The victim is cleared in **its own layer's** map before the new owner is written. My serving code did that clearing
**after** the door returned, using the evicted list - so between the door's write and my late clearing, two layers pointed
at the same slot, and the CUDA backend read a bank entry the graph no longer owned.

That is not a marshalling detail; it is a decision the door cannot make, because it does not know the per-layer maps.

## What this establishes

- the equality measured in EXP-1419 is real and unaffected: it compares fills, hits and order on snapshots, which does not
  require the maps;
- serving needs the **slot-map update inside the door's fill loop**, which means the door must receive the per-layer maps
  or the cache must be represented in a way the door can index - exactly the representation change EXP-1420 called route 1;
- route 2, serving through conversion, is therefore **not viable**: the conversion was never the obstacle.

## State

The serving code is removed from the tree; the reasoning stays as a comment where it was. The comparison remains behind
`FG_SLRU_DIAG`, the native answer is the one used, and the library is healthy:

```
decode 62.83 / 63.22 / 63.25   prefill 2697.4 / 2954.7 / 2952.9   hash c2ff42cc1219
unresolved symbols: ok      accepted components: ok
```

## What the next step has to be

Extend the door to take the per-layer slot maps - a slice per layer, or one flat array indexed by
`il*n_expert + expert` - so the eviction's second half happens inside the decision. That is a port change, and it is the
one route 1 was pointing at from the start.
