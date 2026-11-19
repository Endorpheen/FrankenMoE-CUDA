# EXP-2026-11-19-1422: the door now performs the eviction's second half, and still agrees

## What changed

The native eviction has two halves: the victim is cleared in **its own layer's** slot map, then the new owner is written.
The door did only the second, and EXP-1421 showed that doing the first afterwards crashes - two layers pointed at one slot.

So `slru_plan` now takes the per-layer slot maps and clears the victim inside its own fill loop:

```goose
let old: i64 = g32(key_of, s);
if old >= 0 && maps.len > 0 {
    let old_il: i64 = old / n_expert;
    if old_il < n_layer && old_il < maps.len { st_i32(maps[old_il] as u64, (old - old_il*n_expert) * 4, 0 - 1); }
}
p32(key_of, s, base + e);
```

The maps arrive as an array of base pointers and the store goes through `slru_st_i32`, the FFI the host adapter already
provides - the same pair the module's own host side uses, not a second one.

## The comparison, with the maps in the signature

```
FG_SLRU_COMPARE calls=61441 disagreements=0
decode 62.79 / 63.28 / 63.21   prefill 2732.1 / 2985.1 / 2983.2   hash c2ff42cc1219
```

The shadow passes **copied** maps, so the door's clearing cannot touch the real ones.

## What this proves and what it does not

Proves: the fills, the hit count and their order still match the native decision over 61 441 real decisions, **with** the
map-clearing code in the door's path.

Does **not** prove: that the maps end up equal. The comparison compares fills, not the resulting slot maps - and the map
clearing is precisely the part that crashed when it was wrong. A wrong clear would leave a layer pointing at a slot the
graph no longer owns, which the fill comparison cannot see.

## The next check this defines

Compare the maps themselves, but not per call - a full comparison of 42 layers x 256 experts at 61 441 calls would cost more
than the thing it checks. Instead: snapshot the real maps before a request, let the native path run with the door's
clearing applied to copies, and compare the two map states **at the end of the request**, once. That costs one diff per
request and catches exactly the failure that crashed.

## State

The door carries the whole eviction; the comparison covers fills and hits but not the maps; serving is still not
implemented. Each of those is a named next step rather than an assumption.
