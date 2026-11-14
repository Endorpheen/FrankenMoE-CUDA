# EXP P7-02 S4 - GGUF split and merge

Goose port of `tools/gguf-split/gguf-split.cpp`: `split_str_to_n_bytes`,
`split_params_parse`, `split_strategy::should_split`, the per-shard metadata,
`gguf_split`, `gguf_merge` and the shard naming helpers from `src/llama.cpp`.

The file `source/gguf-split.goose` also carries the accepted S1 metadata
context and the S3 tensor writer unchanged, because split and merge emit
complete GGUF files and a canonical Goose file is self-contained. The S1-S3
body is byte-identical to `goose/model/gguf-tensors.goose` (lines 44-779); the
new code uses the `gsp_` prefix so it can be added to the canon without a
rename.

Base commit: `05048b02`. Native tree: `upstream/llama.cpp`. The experiment id
is provisional; the lead assigns it at integration.

## What this slice covers

```
gsp_parse(n, args, off, len, params)                  split_params_parse_ex
gsp_str_to_n_bytes(str, out)                          split_str_to_n_bytes
gsp_should_split(mode, i, n, next, limit_t, limit_b)  split_strategy::should_split
gsp_split_layout(n, mode, limit_t, limit_b, notf,
                 nbytes, t0, t1)                      the constructor's decisions
gsp_split_write(inp, kv..., t..., t0, t1, n_split,
                out, sh_off, sh_len)                  split_strategy::write
gsp_split_name(prefix, i, n, out)                     llama_split_path
gsp_merge_prefix(path, i, n, out)                     llama_split_prefix
gsp_read_kv(inp, kv..., info)                         gguf_init_from_file, KV half
gsp_read_tensors(inp, pos, n, align, append, ...)     gguf_init_from_file, tensor half
gsp_kv_u16/u32/i32(inp, key)                          scalar lookup without a context
gsp_merge(buf, off, len, n, have_output, out, ...)    gguf_merge
```

### Shard metadata

Native builds one `gguf_context` per shard: shard 0 receives `gguf_set_kv` of
the whole input, then every shard gets `split.no`, a `split.count` placeholder
and `split.tensors.count`, and finally `split.count` is replaced with the shard
count. The replacement removes the key and appends it, so the wire order is

```
[input KV except split.*] split.no split.tensors.count split.count
```

The port writes exactly that order. When the input itself already carries
`split.*` keys, native copies them and then replaces all three, which moves them
to the end; the port skips them in the replay and appends the same three, which
is the same byte sequence (`split_splitkv` covers it).

### Merge metadata

Native reads shard 0, sets `split.count` to 0 (remove + append), replays the
list into a fresh context and appends the tensors of every shard in shard order.
The port does the same through a private setter with the S1 drop/append
semantics, so the merged file keeps `split.no` and `split.tensors.count`,
carries `split.count = 0` and cannot be merged a second time.

### Output alignment

Native writes shards and the merged file from `gguf_init_empty`, whose alignment
is fixed at 32; the tool never copies the input alignment into it. The port
mirrors that: offsets and padding are always 32 bytes, while
`general.alignment` is replayed as an ordinary KV. See the limits below for what
that does to an input that declares 64.

## Layout

```
source/gguf-split.goose  port (S1 + S2 + S3 + S4)
tests/fixture.cpp        native oracle vs port, byte and semantic comparison
tests/build.sh           gate build and run (-O1)
tests/build-native.sh    builds the native gguf-split oracle
tests/version-shim.cpp   the four build-info symbols used by --version
tests/mutate.py          12 mutations, two halves of 6, port built at -O0
tests/gate.txt           recorded gate output
tests/mutations.txt      recorded mutation results
```

Generated C, objects, the oracle binary, the gate work directory and
`tests/mut/*/` are build artefacts; `tests/mut/*/` is covered by `.gitignore`.

## Gate

```
bash tests/build.sh
```

The flint build tree used by the other gates has no `gguf-split` binary, so
`build-native.sh` rebuilds it from the pinned `upstream/llama.cpp` source. Only
the `--version` banner needs generated build-info; `version-shim.cpp` supplies
those four symbols and the rest of `gguf-split.cpp` is the unmodified native
file. If a prebuilt `$FG_BIN/gguf-split` exists, it is used instead.

For every case the fixture writes a tiny GGUF with the port writer, runs the
native tool on it as the oracle, splits the same bytes in memory with `gsp_*`,
and compares:

- the shard names (`gsp_split_name` vs the files native wrote) and the count;
- every shard byte (`gsp_split_write` vs the native file);
- the KV dump and the tensor dump (name, dims, ne, type, offset, nbytes) from
  the port reader against the native reader;
- every port shard reloaded natively with `no_alloc=false`: each tensor's data
  must equal the pattern that went into the input;
- merge: `gsp_merge` bytes against the native merged file, the KV and tensor
  dumps of both, every tensor's data against the input patterns, and
  `split.count == 0` in the result.

Recorded result:

```
FG_G4 cases=39 checks=587 failures=0
```

That is 57 shard byte comparisons, 42 shard names, 13 merged files and 100
tensor data read-backs.

### Cases

| Case | Dataset | Split | Shards |
|---|---|---|---:|
| `split_t128_four` | 4 tensors (f32/f16/q8_0/i32), 4 KVs | tensor 128 | 1 |
| `split_t2_four` | the four | tensor 2 | 2 |
| `split_t1_four` | the four | tensor 1 | 4 |
| `split_t3_four` | the four | tensor 3 | 2 |
| `split_t2_six` | 6 tensors, str + array + string-array KVs | tensor 2 | 3 |
| `split_t1_six` | the six | tensor 1 | 6 |
| `split_notf_four` | the four | tensor 1, no-tensor-first | 5 |
| `split_notf_six` | the six | tensor 2, no-tensor-first | 4 |
| `split_empty` | 0 tensors | tensor 1 | 1 |
| `split_size_big2` | 3 x 1 MiB f32 | size 2M | 2 |
| `split_size_big4` | 3 x 1 MiB f32 | size 4M | 1 |
| `split_size_edge` | 2 x 2,000,000 B f32 | size 2M | 2 (exact boundary) |
| `split_align64` | the four, `general.alignment` 64 | tensor 1 | 2 (bytes only) |
| `split_splitkv` | 2 tensors, input already has `split.*` | tensor 1 | 2 |
| `split_dry_four` | the four | tensor 2, `--dry-run` | 0 written, plan 2 |
| `split_t2_six_dry` | the six | tensor 2, split and merge dry-run | 0 written |
| `split_merge_dry` | the four | tensor 2, merge dry-run | 0 written |

The `edge` group checks the refusal paths: existing output, one shard missing,
an input without `split.count`, a merge that starts on shard 2, a size limit
below the first tensor, and `--split-max-tensors 0`.

The 21 `p_*` cases drive `gsp_parse` with the same argv vector as the native
binary and compare acceptance plus the parsed operation, mode, limits, flags and
positional indexes: defaults, explicit flags, underscore spelling, merge,
size/tensor limits, conflicting operations, conflicting modes, unknown flag, one
and three positionals, a bad unit, a zero size, a missing value, `-h` (native
treats it as a positional) and `--help` (native exits 0).

`unit` cases cover `gsp_str_to_n_bytes` and `gsp_should_split` values that no
argv reaches.

## Refusals

Native `exit(EXIT_FAILURE)`s; the port returns `-1` and leaves the decision to
the caller:

- two operations or two split modes at once, an unknown flag, a missing flag
  value, a bad unit, a non-positive size, a wrong positional count;
- a shard with no tensors (`error: one of splits have 0 tensors`);
- a size limit below the first tensor;
- `--split-max-tensors 0` (native divides by zero);
- merge: output already exists, fewer shards than `split.count`, a missing
  `split.count`, a first shard that is not shard 0, a shard whose `split.no`,
  `split.count` or `split.tensors.count` disagrees;
- a shard path whose name does not carry the expected `-NNNNN-of-NNNNN.gguf`
  suffix, and a name buffer that is too small to hold the shard name.

## Mutations

```
python3 tests/mutate.py --half 1   # 6/6 caught
python3 tests/mutate.py --half 2   # 6/6 caught
```

Each mutation rewrites the port, regenerates the C, rebuilds the fixture (port
at `-O0`) and must make the gate fail. `--jobs N` builds several mutants at
once; the recorded run used one at a time.

Half 1: `split.no` off by one, `split.count` always 1, `split.tensors.count`
off by one, input metadata replayed in the wrong shards, descriptor offsets
starting at the shard's file position, the size comparison using `>=` so a
tensor that exactly fills the limit starts a new shard.

Half 2: the tensor limit one higher, no data padding in shards, descriptors
using the input file offset, merge leaving `split.count` from the shard, merged
descriptors at 64-byte offsets, merge prefix accepting any `.gguf` name.

## Recorded limits

- **Refusals are return codes.** The port does not print usage, does not
  reproduce `exit(1)` and cannot stop the process; `--help` and `--version` set
  a marker in `params[9]` and return 0, like native's `exit(0)`.
- **`-h` is a positional.** The native option loop only accepts arguments that
  start with `--`, so `-h` never reaches the help branch; the port mirrors that.
- **Silent name truncation is refused.** `llama_split_path` returns 0 and the
  caller uses the truncated buffer; `gsp_split_name` returns -1 instead.
- **Alignment is the native tool's 32, not the input's.** An input that
  declares `general.alignment` 64 is split into shards that carry the KV but pad
  at 32, exactly like native. Such a shard is internally inconsistent and the
  native reader misreads it; the port mirrors the bytes and its reader refuses
  the file. `split_align64` therefore compares bytes only, with no reader round
  trip and no merge.
- **Dry run is a host decision.** `gsp_split_write` and `gsp_merge` always
  write; `--dry-run` is only a parsed flag, and the plan is
  `gsp_split_layout` plus the shard count. The gate compares the plan against a
  real native split instead.
- **`--delete-splits` is parsed only.** Goose has no filesystem; deleting the
  inputs stays with the caller, after the merged file is written.
- **Merge is stricter than the tool.** Native fails only when a shard file
  cannot be opened and never checks the later shards' `split.no`; the port
  requires `split.no` on every shard (the tool always writes it), checks
  `split.no`, `split.count` and `split.tensors.count` against the first shard,
  the total tensor count, and that every shard declares the alignment of shard
  0. The native model loader has the same checks, so this closes a gap in the
  tool rather than adding a new rule; a hand-made file without `split.no` is
  refused by the port and merged by native.
- **The reader is the minimum split and merge need.** `gsp_read_kv` /
  `gsp_read_tensors` parse version 2 and 3 files written by this writer,
  validate every bound, refuse duplicate keys and names, unknown or removed
  types, non-power-of-two alignment and out-of-range data, and keep the source
  offsets. They are not a general GGUF inspector: no mmap, no lazy tensor
  allocation, no `llama-io` helpers.
- **Everything is in memory.** The caller owns every buffer and pool; the
  writers return -1 when the output buffer is too small. Size arithmetic is i64
  without overflow guards (same as S3); the largest gate input is 3 MiB.
- **Contiguous tensors, no backend.** Inherited from S3: `nbytes` is the
  contiguous size and the data is read from the file offsets, so a tensor that
  lives in a backend buffer is out of scope.
- **The type table is a copy of this tree.** A newer upstream type needs a table
  update; the gate exercises f32, f16, q8_0, i32 and i8 only.
- **Merge does not compare KVs across shards.** Like native, only the three
  split keys and the tensor counts are cross-checked; a shard with different
  `general.*` metadata is accepted as long as it is not shard 0.
