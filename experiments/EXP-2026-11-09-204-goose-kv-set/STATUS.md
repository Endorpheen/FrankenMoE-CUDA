# EXP204: KV cell state copy in Goose (P5-01, the "copy" clause)

`llama_kv_cells::set()` is the ubatch rollback path: when an ubatch cannot be emplaced the saved
cell states are copied back. A wrong copy corrupts a restored context silently.

## Result — live

    FG204X_KV_SET cells=32907
    2568.67 / 26.93, tokens and all 128 probabilities equal, text sha256 e68df05b

Gate: 11,255 checks / 0 failures over 400 randomized copies, plus one isolated trap for a
source cell with a non-zero shift (native asserts zero, here safety code 501).

## Two real failures on the way, both recorded

**First live run: `tokens=False`, decode 8.34.** Two separate defects, both mine.

1. **Wrong form.** The call site is `cells.set(sinfo.idxs[s], ...)` — the *indexed* form, where
   destination cells are an arbitrary index list. I first wired the contiguous form with base 0,
   which wrote the wrong cells.
2. **Wrong cost.** I marshalled the *entire* cache in and out on every call — O(cache) instead of
   O(m). That is what took decode to 8.34 tok/s. The 32,907 cells figure is why: this path is not
   rare, it runs on every ubatch that has to be rolled back.

Fix: the module now takes compact m-sized arrays. The host gathers the m cells named by
`sinfo.idxs[s]`, Goose copies them and decides the used flag, the host scatters them back. Cost is
O(m).

## Container shape, and why the first version could not be wired

The real class keeps `pos`/`shift`/`ext` as `std::vector`, `seq` as
`std::vector<std::bitset<LLAMA_MAX_SEQ>>` and `used` as `std::set` — separate containers, all
private. A modelled flat arena cannot express that. The integration copy of `llama-kv-cells.h` now
carries narrow accessors (pos/shift pointers, seq gather/scatter, used insert/erase) and the
`used` set stays authoritative in C++: Goose decides the flag, the container records it.

`LLAMA_MAX_SEQ` is 256, so a cell's sequence bitset is four 64-bit words; `W` is passed in rather
than fixed so a different build cannot be silently wrong.

## Not modelled

The `ext` record: it carries no decision this profile makes.

## Switch

`GGML_GOOSE_KV_SET=0` restores the native `cells.set(...)` loop.
