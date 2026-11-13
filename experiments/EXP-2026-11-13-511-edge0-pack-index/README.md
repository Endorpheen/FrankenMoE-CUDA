# EXP511: Edge0 pack index reader in Goose (universal runtime, model family 2)

First Edge0 slice, written in Goose and kept inside the one universal runtime:
the same `llama-server` gains a second model family instead of a separate
build. Apex keeps the existing GGUF path and profile unchanged.

## What was ported

`source/pack.goose`, from the strata pack format documented in
`docs/goose-migration/EDGE0-120B-ADAPTATION.md`:

| piece | rule |
|---|---|
| line scanner | splits on space, tab and CR, handles the last line without a newline |
| field parser | decimal fields with a leading `-`; missing fields return -1 |
| index v3 | header `align/pool/tensors`, 1367 rows, in-pool rows counted, total bytes, maximum end offset |
| index validation | 64-byte alignment, every in-pool row inside the pool, malformed rows counted |
| native experts v6 | 48 layers, record size from the first layer, stride = 512 records, type ids 11/20, offsets follow the stride |

The C++ side is only the differential fixture: it reads the files and computes
the same statistics independently.

## Universal runtime contract

This is a capability of the universal runtime, not a per-model binary:

1. One `llama-server` serves every model family. The loader selects a provider
   from the model directory: a GGUF file keeps the Apex path, a directory with
   `index.txt`, `profile.bin` and the pack files selects the Edge0 provider.
2. The Goose modules are linked once and registered as capabilities. Edge0 adds
   a reader and later a loader/graph provider; it does not fork the runtime.
3. Backward compatibility is a gate, not a promise: the Apex profile, CLI flags
   and output hashes must stay unchanged. Edge0 options are additive and off by
   default.
4. No model-specific build directories: the provider is chosen at load time.

## Gate

`tests/pack_fixture.cpp` reads the real tables, calls the Goose module and
compares every statistic with an independent C++ parse, plus a synthetic
malformed corpus for the negative paths.

| corpus | cases |
|---|---|
| real `index.txt` (1367 rows, 1065 in pool, pool 1538625024, align 64, max end = pool) | 1 |
| real `native_experts.txt` (48 layers, record 1164800, stride 596377600) | 1 |
| synthetic misaligned and out-of-pool rows | 1 |
| synthetic wrong type id and wrong layer offset | 1 |
| total | **4 cases, 38 values, 0 failures** |
| mutations | **6/6** in two halves of three |

The first campaign missed the alignment and pool-bound mutations because the
real pack is correct; the synthetic corpus was added and both are now caught.

## Build

```
experiments/EXP-2026-11-13-511-edge0-pack-index/build_fixture.sh [pack]
```

The script compiles the Goose module with the pinned `goose-2026.3-string`
compiler and links it into the fixture. Generated `pack-gen.c/h` are not
committed.

## Scope

E1 only. The loader binding the skeleton and expert records (E2), the forward
pass (E3/E4), PLE streaming (E5) and MTP (E6) are not started. The reader works
on the complete files of the current download; loading needs
`model-skeleton.gguf` and `experts.bin` complete.
