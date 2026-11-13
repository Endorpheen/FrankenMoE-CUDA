# EXP464: mmap/pread read decisions in Goose (P9-02 sixth slice)

With mmap the tensor bytes come straight out of the mapping; without it they come
from `read`/buffered I/O, and the loader has to decide truncation, EOF and fallback
behaviour. This experiment moves those decisions into Goose.

## What was ported

`source/readpath.goose`, from `src/llama-mmap.cpp` (`llama_file::read_raw`,
`read_raw_unsafe`, `read_alignment`) and the loader's source selection:

| decision | rule |
|---|---|
| buffered plan | ask for `min(len, size - off)`; a request starting at or past EOF reads zero bytes |
| short-read rule | a short read is an error only when something was requested before the end |
| errno action | `EINTR` retry, `EFAULT`/`EINVAL` fall back to buffered I/O, anything else fatal |
| EOF policy | zero-fill the requested padding only when the position is exactly the file size |
| read alignment | block size for a direct descriptor, 1 for a buffered stream |
| source selection | the mapping when mmap is active and a mapping exists, otherwise the file |
| mapping offset | weight offset + inner offset, zero when the read is not from a mapping |

The syscalls, the `FILE*`, the descriptor and the mapping stay native.

## Gate

`tests/readpath_fixture.cpp` transcribes the native expressions and drives the
direct-read loop with a scripted errno sequence, so retry, fallback, fatal and
zero-fill paths are all exercised.

| corpus | cases |
|---|---|
| buffered plan over 4 sizes x 9 offsets x 4 lengths | 144 |
| errno action table | 6 |
| EOF policy over sizes and positions | 15 |
| alignment selection | 8 |
| 300 scripted direct-read sequences compared by outcome and bytes delivered | 600 |
| source selection and mapping offsets | 18 |
| total | **501 cases, 945 values, 0 failures** |
| ASan + UBSan (runtime, module, fixture) | 0 failures, rc 0 |

Mutations 9/9 caught: buffered read not truncated at EOF (14 failures), short read
never an error (36), EINTR fatal (47), EFAULT fatal (31), zero-fill past the file
end (14), alignment always the block size (5), source ignores the mmap flag (1),
mapping offset adds inner twice (3), mapping offset ignores the weight offset (4).

One candidate mutation was dropped rather than counted: `off >= size` versus
`off > size` at the buffered plan boundary is unobservable, because the
`size - off` clamp already yields zero at equality.

## Scope

P9-02 still needs the split-expert loader integration. The syscall layer, the
mapping object and the `pread`-style transfer itself remain native, which is the
primitive platform boundary.
