# EXP443: GGUF structural parser in Goose (P7-01 first slice)

GGUF is the model container. This experiment ports the structural parser used to
walk the header, metadata and tensor descriptors, and gates it against an
independent Python parser on the real Apex GGUF.

## What was ported

`source/gguf.goose`:

- magic/version/tensor-count/kv-count header for GGUF v2/v3;
- every metadata key: length, FNV-1a hash, value type;
- value skipping for all GGUF scalar types (uint8/int8, uint16/int16,
  uint32/int32/float32, bool, uint64/int64/float64), strings and arrays
  (element type + count + elements);
- every tensor descriptor: name length/hash, n_dims, up to four dimensions, GGML
  type and data offset;
- bounds checks on every read; truncated/corrupt input returns a negative code.

The output is a fixed array of 64-bit records: header (3), five words per
metadata key, ten per tensor.

## Gate

Apex GGUF, 21 713 463 136 bytes, structural region ends at 10 990 919 bytes.

| check | result |
|---|---|
| Goose records | 7 040 words |
| independent Python parser | 7 040 words, **exact match** |
| truncation below the structural end | **14/14 rejected** (0/4/8/16/23/24/64/1000/1M/5M/10M/10.99M/10,990,918) |
| corrupt magic | rejected |
| corrupt tensor count | rejected |

The Python reference is `check.py`, a separate `struct` parser; it does not call
the Goose code.

## What this is not

- Metadata values are skipped by type and their keys/types are hashed; this is
  not yet a typed value accessor (`gguf_get_val_*`) or a writer/splitter.
- GGUF v1, non-little-endian files, multi-shard files and tensor data alignment
  beyond the descriptor are not covered.
- P7-01 therefore moves from TODO to IN_PROGRESS; the remaining accessor,
  version/endian and shard work is recorded, not claimed.

## Reproduce

    bash experiments/EXP-2026-11-13-443-p7-01-gguf-parser/build_fixture.sh

The fixture reads the Apex GGUF read-only and writes only `records.bin` under
`runtime/exp443/build`.
