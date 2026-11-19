# EXP-2026-11-19-1340: the first native path now comes from Goose - integral metadata keys

## The change

`GKV::get_kv` - the funnel behind 17 `get_key` sites - no longer calls the native getter for integral keys. It asks
the port, and only when the port cannot answer (a split-file shard, a key outside the window) does it fall back:

```cpp
int64_t port_value = 0;
if (::fg_port_int(k, port_value)) { ... return (T) port_value; }
return GKV::getter(ctx, k);          // fallback only
```

This is the first place where the shipped runtime takes a metadata decision **from the ported reader** rather than
from the native context.

## Measured on a real load

```
FG_GGUF_PARSE  nk=52 nt=753 align=32 off=10990944 disagreements=0
FG_GGUF_SERVED from_port=8
FG_GGUF_SERVED from_port=16
FG_GGUF_DIFF   0 lines
```

Sixteen integral reads served from the port during one model load, zero disagreements, all five HTTP endpoints
answering, and the acceptance hash unchanged:

```
decode  62.23 / 62.79 / 63.11
prefill 2696.4 / 2951.9 / 2953.5
hash    c2ff42cc1219
```

## Why this is safe to do now and was not before

The shadow comparison of EXP-2026-11-19-1339 made the two readers known equal at the moment of use, for every integral
key, during a real load. Serving from the port is therefore not a bet: if the port answered differently the load would
already have been failing.

## What is still native

Strings (13 keys) and arrays (the tokenizer tables) still come from the native context, and so do the 6 tensor
descriptor sites and the split-file path. The order and the risks are in
`integration/p7-12-gguf-context/PLAN.md`; strings are next, and they are the one the loader reads as
`general.architecture`.

## What this means for the ledger

P7-12's column "the loader takes its values from the port" moves from *no* to *partly*: integral keys yes, strings and
arrays no. That distinction is the reason the column exists.
