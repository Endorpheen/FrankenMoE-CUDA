# EXP-2026-11-18-1202: P6-11 ngram-map hash and slot

`hash = hash * LCG_FACTOR + token` over 32-bit wraparound with `LCG_FACTOR = 2654435761`, and the
slot is `hash % key_map_size`.

```
ngram hash: cases=503 fails=0
```

Compared against the native `uint32_t` arithmetic verbatim, 500 random inputs of vocabulary-sized
token ids, plus three boundary values (`0xFFFFFFFF`, `0xFFFFFFFE`, `0x80000000`) where the product
`hash * LCG_FACTOR` exceeds a signed 64-bit integer.

## Why the multiply is not one line

`2654435761 * 4294967295` is about 1.14e19 and a signed 64-bit integer stops at 9.22e18, so writing
`h = h * lcg` and then reducing modulo 2^32 gives a **negative** value on the boundary inputs and a
different hash. The port multiplies in two halves (`h = hi * 65536 + lo`), reduces each half, and
adds - which is the same modular product without leaving the representable range.

The boundary cases are in the gate for exactly that reason: 500 random small-id cases passed with
the naive form too, and only ids near 2^32 expose it.

## Scope

This is the arithmetic the hash shortcut uses. The shortcut's *behaviour* (which matching position it
returns) is still out of scope, as recorded in the previous card; here only the hash and its slot are
claimed equal.
