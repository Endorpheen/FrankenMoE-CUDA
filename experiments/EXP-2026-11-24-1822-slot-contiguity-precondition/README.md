# EXP-2026-11-24-1822 — Destination contiguity is the blocker, and the index forbids fixing it cheaply

Status: **MEASUREMENT.** Baseline `37ac0f1d` unchanged (prefill 3122.4,
decode 78.225).

## The precondition, measured

The coalescing counter from EXP-1821 was split into the two halves, on the
acceptance profile with a 32768-token prompt:

```
Goose copy totals: jobs 99618 -> runs 99618 over 349 calls;
  adjacent pairs dst-contiguous 0, src-contiguous 62450, both 0
```

- **source-contiguity exists**: 62 450 of the 99 617 adjacent pairs (63 %) have
  `src[i] + bytes[i] == src[i+1]`, because consecutive missed experts of a plane
  are adjacent in the model file;
- **destination-contiguity is exactly zero**: the SLRU never gives two
  consecutive slots to two consecutive jobs.

So the lever the reviewer proposed is real in principle: with sequential slot
assignment 63 % of the pairs could merge, cutting the 99 618 transfers to
roughly 37 000, and the driver cost is proportional to the transfer count
(EXP-1818: 88.3 % of request processor samples inside libcuda).

## Why it cannot be done as a slot-order tweak

The expert index is an **open-addressed hash table**, and the slot is the hash
path:

```
    let entry: i64 = slots[slot];
    if entry == 0 { slots[slot] = index + 1; return true; }
    ...
    slot = (slot + 1) & (slots.len - 1); visited += 1;
```

A lookup probes from `hash(key)` forward and **stops at the first empty slot**.
Assigning a miss to "the next free slot" instead of its hash path would place the
key outside its own probe chain: a later lookup would terminate at an empty slot
before reaching it and report a false miss, which silently changes the hit
pattern and therefore the output.

Making slots sequential at fill time therefore requires replacing the index with
a key→slot map plus a separate free list (and eviction ties the two), that is a
re-architecture of the SLRU core in the Goose unit, not a change of an allocation
line. Note that this payoff is independent of the eviction policy: it does not
change how many experts are missed, only how many transfers carry them, so the
2.3 % Belady headroom of EXP-1816 does not cap it — the cap is the 63 % share of
adjacent pairs that are already source-contiguous, that is about 2.7x fewer
transfers.

## What the numbers say about the prize

If such a redesign landed, the transfers would fall about 2.7x, and the exposed
driver cost with them; that is the only remaining path to 3500 that the
measurements support, and it is a structural project in the Goose SLRU rather
than a driver tweak.

## Artefacts

`runs/contig-022059` (the split counter and its totals line).
