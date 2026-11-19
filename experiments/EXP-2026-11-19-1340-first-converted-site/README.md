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

## Strings followed, and only one form of them

`std::string` keys are now served by the port too - the loader reads `general.architecture` and the tokenizer names
that way:

```
FG_GGUF_SERVED_STR from_port=4
FG_GGUF_SERVED     from_port=16
decode  62.33 / 63.15 / 62.91
hash    c2ff42cc1219
```

The `const char *` form is deliberately **not** converted: it would need a NUL-terminated copy whose lifetime nobody
has stated, and inventing one to convert a second string accessor is exactly the kind of shortcut this report keeps
paying for. It stays native and is listed below.

## What is still native

The `const char *` string form, the arrays (tokenizer tables), the 6 tensor descriptor sites and the split-file path
still come from the native context. The order and the risks are in
`integration/p7-12-gguf-context/PLAN.md`; strings are next, and they are the one the loader reads as
`general.architecture`.

## What this means for the ledger

P7-12's column "the loader takes its values from the port" moves from *no* to *partly*: integral keys yes, strings and
arrays no. That distinction is the reason the column exists.
