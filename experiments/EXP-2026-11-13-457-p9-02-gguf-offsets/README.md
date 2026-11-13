# EXP457: GGUF offset indexing in Goose (P9-02 first slice)

Before a single tensor byte is read, the loader has to know where every tensor
lives. This experiment moves that arithmetic into Goose.

## What was ported

`source/offsets.goose`, from `ggml/src/gguf.cpp` (the aligned seek to the data
section, the per-tensor offset check and the size accumulation) and
`ggml/include/ggml.h` (`GGML_PAD`):

| rule | native | Goose |
|---|---|---|
| round up | `(x + n - 1) & ~(n - 1)` | `align_up` |
| alignment validity | non-zero power of two | `align_ok` |
| tensor byte size | `type_size * (nelements / block_size)` | `nbytes`, returns `-3` when the division is not exact or the block is unusable |
| data section start | pad(end of structure, alignment) | compared through `align_up` |
| descriptor offsets | cumulative padded sizes, checked per tensor | `layout`, returns `-2` on a gap |
| absolute offset | data start + descriptor offset | `layout` |
| data section size | sum of padded sizes | `layout`, written after the last offset |

The type size/block-size table, the file reads and the container parse stay
native (the parse itself is EXP443).

## Gate

`gen_expected.py` builds the oracle from the real file with two independent
implementations: it parses the header/metadata/descriptors with `struct`, takes
the type table from the `gguf` library, computes the padded layout, and then
**cross-checks every absolute offset against `GGUFReader.data_offset`** before the
fixture runs.

| check | result |
|---|---|
| Apex GGUF, 753 tensors | alignment 32, structural end 10 990 919, data start 10 990 944, data size 21 702 472 192 |
| layout reproduces every absolute offset | 753/753 |
| total data size | exact |
| `align_ok` over -2..129, `align_up` grid (6 alignments x 201 values), byte-size cases | 0 failures |
| synthetic layout with unaligned sizes (10/10/10) | offsets 1000/1032/1064, total 96 |
| total | **2 109 cases, 2 109 values, 0 failures** |
| ASan + UBSan (module and fixture) | 0 failures, rc 0 |

Mutations 8/8 caught: round-up adds a full alignment (1 119 failures), alignment
check drops the power-of-two test (122), zero alignment accepted (1), byte size
rounds the block count up (5), divisibility check dropped (1), absolute offset
forgets the data start (756), gaps between descriptors accepted (1), total
accumulates unpadded sizes (4).

The unpadded-total mutation was missed at first: every real Apex tensor has a
byte size that is already a multiple of 32, so the file cannot distinguish padded
from unpadded accumulation. The synthetic 10/10/10 layout was added for that
reason instead of accepting the gap.

## Scope

P9-02 also covers platform file I/O (open/read/seek/mmap behaviour, path
handling, error reporting). That half is not in this slice, so P9-02 moves from
TODO to IN_PROGRESS.

## Reproduce

    bash experiments/EXP-2026-11-13-457-p9-02-gguf-offsets/build_fixture.sh
    python3 experiments/EXP-2026-11-13-457-p9-02-gguf-offsets/mutate.py
