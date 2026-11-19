# EXP-2026-11-19-1420: what serving the cache policy would cost, and why it is not a switch

## The equality is in hand

EXP-1419: 61 441 decisions on the accepted profile, zero disagreements, fills and hits and their order equal. The next
step is serving, and this card is the arithmetic that says what serving requires.

## The mismatch that blocks a one-line switch

The door's ABI takes the cache state in a specific form:

| state | the cache has | the door wants |
|---|---|---|
| `seen` (generation marks) | `std::vector<uint32_t>` | `i64[:]` |
| `is_protected` | `std::vector<bool>` | int32 array |
| `head`, `tail` | `int32[2]` members | pointers to int32 |
| `key_of`, `prev`, `next`, slots | `std::vector<int32_t>` | pointers to int32 |

so a served call has to convert in and convert back, every time. With ~2 958 slots estimated from the cache size and an
expert of about 1.77 MB, seven arrays of int32 are **81 KiB per call**, and at the measured 61 441 calls per run that is
**5.1 GB of copying per acceptance run** - about half a second at memory speed, against an 8-second decode.

That is not fatal, and it is not free: it would show in decode as roughly one token per second, and it would be paid by
the cache for a decision that is already known to be equal. A conversion that costs a measurable fraction of the thing it
optimises is the wrong shape even when it is correct.

## The two honest routes

1. **Represent the cache state in the door's form.** `seen` as int64, protected flags as int32, and keep the LRU arrays as
   they are. Then serving is a call, not a marshalling layer, and the equality already measured carries over. This is a
   change to the cache's data structures, which is why it is a step of its own and not a flag.
2. **Serve through conversion and measure it.** Simpler to write, and the arithmetic above says what to expect: about
   0.5 s of copying per run. If the measured cost is lower than estimated - the arrays are small enough to sit in cache -
   this may be acceptable as an intermediate.

The order matters: route 2 is only worth doing if route 1 is not imminent, and route 1 is the shape the migration wants -
the decision and its state in one place, not a translation layer between them.

## What is not claimed

No serving was implemented here. The comparison remains behind the flag and the native answer remains the one used. The
card's contribution is the number that makes the next decision a calculation rather than a guess.
