# `slru_plan`'s contract, read from its source, and the shadow comparison it enables

## The ABI, from the generated header

```
int64_t slru_plan(uint64_t key_of, uint64_t prev, uint64_t next, uint64_t prot,
                  uint64_t head, uint64_t tail, sl_i64 seen, uint64_t slots,
                  uint64_t ids, int64_t n_ids, int64_t il, int64_t n_expert, int64_t n_slots,
                  sl_i64 scal, uint64_t uniq, uint64_t fills_e, uint64_t fills_s, uint64_t evicted);
```

Seventeen arguments, of which six are bare pointers (`uint64_t`) into arrays whose lengths the door already knows from
`n_slots` / `n_expert`. That is a note for the caller: **the door cannot check those lengths**, so the caller must pass
buffers exactly the size the cache already allocates.

## Inputs and outputs, from the body

| argument | direction | meaning |
|---|---|---|
| `key_of`, `prev`, `next`, `prot`, `head`, `tail`, `slots` | in/out | the cache's state, mutated exactly as the native `plan()` mutates it |
| `seen` | in/out | the generation marks, with `scal[0]` carrying the new generation |
| `ids`, `n_ids` | in | the experts this request selected in layer `il` |
| `scal` | out | `[0]` new generation, `[2]` fills, `[3]` hits, `[4]` 1 on success and 0 on failure |
| `uniq` | out (scratch) | the deduplicated, then sorted, expert list |
| `evicted[i]`, `fills_e[i]`, `fills_s[i]` | out | per fill: what was evicted from the victim slot, and which expert went where |

Return value: the number of fills, or `-1` when more distinct experts were requested than there are slots.

## The three behaviours that have to match, not just the counts

1. **promotion before eviction** - hits are moved before any victim is chosen, which is what keeps the hot set alive;
2. **sorted misses** - the fill order follows the sorted expert ids, so consecutive uploads can be merged; getting the
   order wrong produces the same fills and a slower run, which is the kind of difference a count comparison would miss;
3. **victim preference** - probation before protected, with the protected count capped at half the slots.

## The shadow comparison this enables

`plan()` is called with the cache's own state. The comparison has to see the **pre-state**, so:

1. snapshot `key_of`, `prev`, `next`, `prot` (converted from `vector<bool>` to int32), `head`, `tail`, `seen`, `slots`
   before the native call;
2. let the native `plan()` decide, as it does today;
3. run the door **on the snapshot**, so it cannot disturb the real state;
4. compare: return value, `scal[3]` (hits), and the `fills_e`/`fills_s`/`evicted` triples in **order**;
5. on a difference, print the first one with the layer, the ids and both fill lists, and refuse to serve the door's answer
   in that call.

Serving is not part of this step: the native answer stays authoritative until the comparison is clean over a real run, per
the rule the whole migration follows.

## Why the order matters and not just the set

A cache that fills the same experts into the same slots in a different order produces identical **contents** and different
**device traffic**. The acceptance signal for this door therefore includes prefill, not only decode - the same reasoning
that made step 2 of P9-04 about overlap rather than about results.
