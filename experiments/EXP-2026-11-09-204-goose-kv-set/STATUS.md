# EXP204: KV cell state copy in Goose — module verified, live wiring open

`llama_kv_cells::set(i, other)` is the rollback path: when an ubatch cannot be emplaced, the
saved cell states are copied back. Getting it wrong corrupts a restored context silently.

## Result

    FG204S_KV_SET cells_copies=547
    FG204S_FIXTURE checks=11062 failures=0
    EXP204_LOCAL_GATES_PASS

The gate mirrors 300 randomized copy operations into both arms and compares pos, seq words and
the used flag for every cell in the destination arena after each step, plus two fork-isolated
traps: a source cell with a non-zero shift (native asserts it is zero, here it is safety code 304)
and a range overflow (302).

## What is verified

The decision logic: for each copied cell, the used flag follows whether the position is `-1`, the
position is replaced, and the eight sequence words are copied. That is the whole behavioural
content of `llama-kv-cells.h:166-192` apart from the `ext` record, which is not modelled because
it carries no decision this profile makes.

## What is NOT verified, and why live wiring is open

The module works over a *modelled* arena — one flat int64 block laid out as
`pos[n] | seq[n*W] | used[n] | shift[n]`. The real class does not use that layout:

- `pos`, `shift` and `ext` are `std::vector`, `seq` is `std::vector<std::bitset<LLAMA_MAX_SEQ>>`,
  and `used` is a `std::set<uint32_t>` — separate containers, not one interleaved block;
- all of them are **private**, so a bridge cannot obtain a base pointer without an accessor;
- the call site uses the *indexed* form `set(const std::vector<uint32_t> & idxs, other)`, whose
  destination indices are an arbitrary list, not a contiguous range.

So wiring this into `llama-kv-cache.cpp:825` needs, in order: an accessor added in an
experiment-local copy of the header, the three-vector layout reflected in the module signature
instead of a single base, and the indexed form ported as well. That is real work and it is not
faked here — reporting "integrated" from a modelled arena would be exactly the kind of claim this
project forbids.

EXP199 avoided the same problem by taking scalars (`pos_in(pos,p0,p1)`, `plan_cell(...)`) rather
than pointers, which is recorded in its bridge.

## Next step

Port the indexed `set` and give the module the real three-container signature, then wire it with
an accessor in the integration copy. Until then this is a verified algorithm, not an executing
component, and `active.json` is untouched.
