# EXP460: mmap, direct-I/O and mlock decisions in Goose (P9-02 fourth slice)

Model loading picks a file-access mode and then has to honour that choice: mmap
needs an aligned data section, direct I/O needs an aligned staging window, and
mlock grows in page steps. This experiment moves those decisions into Goose.

## What was ported

`source/mmap.goose`, from `src/llama-model-loader.cpp` (load-mode mapping, the
`gguf_get_data_offset` gate, the staging buffer size) and `src/llama-mmap.cpp`
(`read_aligned_chunk`, `llama_mlock::grow_to`, `align_range`):

| decision | rule |
|---|---|
| `use_mmap` | AUTO, MMAP and MMAP_MLOCK, dropped when the platform has no mmap |
| `use_direct_io` | DIRECT_IO only |
| `use_mlock` | MLOCK and MMAP_MLOCK |
| mmap gate | `data_offset % backend_alignment == 0`; a bad alignment or offset is rejected |
| staging plan | aligned start, offset inside the window, byte count rounded up, buffer capped at 64 MiB, skip = offset inside the window |
| staging buffer | 64 MiB + 2 x alignment for direct I/O, 1 MiB for buffered files |
| mlock step | target page-aligned, only the new tail is locked, and the object stops for good after a failure |
| unmap range | first moved to the next page, last moved to the page start, empty range clamped to zero length |

The system calls (mmap/munmap/mlock/read) stay native, which is the "primitive
platform imports" boundary the requirement allows.

## Gate

`tests/mmap_fixture.cpp` transcribes the native expressions and compares the
module, including a staging-loop simulation over a synthetic 128 MiB image.

| corpus | cases |
|---|---|
| load modes -2..5 x mmap supported/unsupported | 16 |
| data-offset alignment gate over 5 alignments x 9 offsets | 45 |
| chunk plan over 4 alignments x 9 sizes x offset sweep | 916 |
| staging-loop simulation: 200 random (offset, size, alignment) reads, bytes compared with the file window | 400 |
| staging buffer size | 5 |
| mlock growth over sizes x targets x failed flag | 72 |
| unmap range alignment over 2 page sizes x 13 x 13 positions | 338 |
| total | **1 783 cases, 1 983 values, 0 failures** |
| ASan + UBSan (runtime, module, fixture) | 0 failures, rc 0 |

Mutations 10/10 caught: AUTO stops using mmap (1 failure), mmap also sets direct
I/O (8), MLOCK alone stops locking (2), unsupported mmap not dropped (3),
alignment gate accepts unaligned (21), staged read not capped (246), byte count
rounds down (1 349), staging buffer ignores the buffered case (1), mlock keeps
retrying after a failure (40), empty unmap range not clamped (252).

## Scope

P9-02 remains open for the async-upload path and pinned host buffers
(`llama-model-loader.cpp` staging ring), which are a larger slice than the mode
and alignment decisions covered here.

## Reproduce

    bash experiments/EXP-2026-11-13-460-p9-02-mmap/build_fixture.sh
    python3 experiments/EXP-2026-11-13-460-p9-02-mmap/mutate.py
