# EXP P7-02 S2 — GGUF writer core

Goose port of the GGUF serialization block from `ggml/src/gguf.cpp`:
`gguf_writer_base`, `gguf_writer_buf`, `gguf_write_out`, the header, KV encoding
order, string/array encoding, alignment padding, `only_meta`, and the
`gguf_get_meta_size`/`gguf_get_meta_data` byte producer.

Base commit: `a640bac9` (`EXP-2026-11-14-595`). Native tree:
`upstream/llama.cpp` (GGUF version 3, default alignment 32).

The experiment id is assigned by the lead at integration; the tree here is not
committed.

## What this slice covers

`source/gguf-writer.goose` is the accepted S1 context (KV list, all setters,
`remove`, `set_kv`, reserved-key rule) plus the writer `s2_write`:

```
s2_write(st, pool, key_off, key_len, types, arr, v0, v1, v2,
         out, version, alignment, only_meta) -> bytes_written
```

- header: magic `GGUF`, `u32` version, `u64` n_tensors (0 here), `u64` n_kv;
- KV in list order: key as `u64` length + bytes, `u32` type id, value;
- arrays: type 9, element type, `u64` count, then raw element bytes or one
  `u64` length + bytes per string element;
- zero padding until `bytes_written % alignment == 0`;
- `only_meta` is accepted but cannot change the bytes in this slice: with no
  tensors the metadata section is the whole file. Native behaves the same way.

Out of scope: tensor descriptors, `write_tensor_meta`, the data section, the
file writer (`gguf_writer_file`), split/merge, `llama_model_saver`. Those are
S3–S5 of `docs/goose-migration/P7-02-COVERAGE.md`.

## Layout

```
source/gguf-writer.goose   port (S1 + S2)
tests/build.sh             gate build and run
tests/fixture.cpp          native + port in one binary, byte compare
tests/mutate.py            12 mutations, two halves of 6
tests/cases.md             case table and wire layout
```

## Gate

```
bash tests/build.sh
```

`build.sh` generates `build/gguf_writer.c/.h` with `goose --standalone`, compiles
it with `gcc-13 -O1`, builds `tests/fixture.cpp` against the native
`libggml-base.so` and runs the comparison. Generated C and objects stay in
`build/`; the source tree stays clean.

Recorded result:

```
FG_G2 cases=31 checks=161 failures=0
```

`only_meta=false` uses the public `gguf_write_to_file`, `only_meta=true` uses
`gguf_get_meta_size` + `gguf_get_meta_data`; both call the same native
`gguf_write_out`. The private `gguf_write_to_buf` symbol is deliberately not
declared by hand.

## Mutations

```
python3 tests/mutate.py --half 1   # 6/6 caught
python3 tests/mutate.py --half 2   # 6/6 caught
```

Half 1: key length prefix, KV order, missing array element type, version,
swapped n_kv/n_tensors, hard-coded pad target 16.
Half 2: string array element keeps its NUL, 8-byte scalars written as 4 bytes,
magic letter, array count taken from the byte length, scalar string without
length prefix, padding filled with `0xff`.

Each mutation rewrites the port, regenerates C, rebuilds the fixture and must
make the gate fail. Mutant sources and binaries live in `tests/mut/Mxx/`.

## Recorded limits

- No tensors: `n_tensors` is 0, so `only_meta=true` and `only_meta=false`
  produce identical bytes. S3 must extend the writer before the two differ.
- Native `gguf_write_to_buf` appends to the caller's vector; the port writes
  from offset 0. The gate uses fresh buffers, which matches the
  `gguf_get_meta_data` contract (exact-size destination).
- `ctx->alignment` is private and `gguf_init_empty` fixes it at 32; non-default
  alignments (64, 1) are verified by reloading the port bytes through the native
  reader, the only public way to set it.
- `bool` scalars are gated with 1/0; native normalizes any nonzero to 1, the
  port copies the stored byte. The setter path only receives 0/1.
- `gguf_get_meta_size` / `gguf_get_meta_data` wrappers are exercised on the
  native side; the port exposes the byte producer (`s2_write`), not those two
  wrappers.
- The fixture buffer is 1 MiB; the largest case is under 400 bytes.
