# P9-02 closure: platform/file I/O and GGUF offset indexing

Criterion: "Bounded reads, mmap/pinned allocation, shard offsets, endian/error/overflow
cases Goose; only primitive platform imports retained."

## Coverage

| clause | state | evidence |
|---|---|---|
| GGUF offset indexing | Goose | EXP457: alignment rounding, power-of-two check, tensor byte size, descriptor-offset cumulativity, absolute offsets and padded data-section total; 753/753 Apex tensors reproduce `GGUFReader.data_offset`; 2 109 cases, 8/8 mutations |
| bounded reads | Goose | EXP458: scalar/array/string fit and limits, seek bounds, chunked transfer with the file-end clamp and EOF zeroing, `file_remain`; 8 275 cases, 7/7 mutations |
| shard naming and validation | Goose | EXP459: `llama_split_path` formatting with the `%05d` rule, the capacity rule including the terminator, first-split/count/index/tensor-total checks, little-endian magic and v2/v3 guard; 234 cases, 8/8 mutations |
| mmap load decisions | Goose | EXP460: load-mode flags with the unsupported-platform fallback, data-offset alignment gate, direct-I/O staging plan and 64 MiB cap, staging buffer size, page-aligned mlock growth, unmap range alignment; 1 783 cases, 10/10 |
| async-upload staging ring | Goose | EXP461: alignment maximum, four-buffer rotation, chunk plan with leading/trailing padding rules, eligibility chain with a reason per failure, biggest-first staged ordering; 3 147 cases, 11/11 |
| read-path decisions | Goose | EXP464: buffered plan with EOF truncation and the short-read rule, errno action table (EINTR retry, EFAULT/EINVAL fallback), EOF zero-fill, read alignment, mmap-vs-file selection, mapping offset; 501 cases, 9/9 |
| endian | Goose guard | the container is little-endian only: the magic is compared as a LE u32 and a big-endian file is rejected; native `gguf.cpp` has no byte-swap path either (EXP459) |
| overflow cases | Goose | descriptor counts, array element counts, byte products, alignment rounding and EOF arithmetic all have explicit guards with negative return codes |
| primitive platform imports | native by design | `open/read/pread/mmap/munmap/mlock/madvise`, the `FILE*` and the CUDA host-buffer allocation calls |

## Boundaries

- The syscall layer and the mapping object stay native; the decisions that feed
  them are Goose with differential gates.
- Split-expert placement across devices and the MoE cache are **not** part of this
  criterion: they belong to P3-11 (expert routing/placement) and the P6 model
  lifecycle work. The phrase "split-expert loader integration" in earlier notes was
  a shorthand, not an acceptance clause of P9-02.
- Byte-identical reconstruction of the deployed `libggml-base` is P10-06/P10-11.

## Verdict

P9-02 closes: every decision the criterion names is Goose and gated, the platform
imports that remain are primitives, and the boundaries are recorded rather than
silently dropped.
