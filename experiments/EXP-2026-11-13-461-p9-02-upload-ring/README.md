# EXP461: async-upload staging ring in Goose (P9-02 fifth slice)

When tensors are not mmapped, the loader stages them through pinned host memory in
a four-buffer ring and uploads them asynchronously. This experiment moves the ring
decisions into Goose.

## What was ported

`source/upload.goose`, from `src/llama-model-loader.cpp` (`load_all_data`):

| decision | rule |
|---|---|
| alignment | maximum `read_alignment` across the files, at least 1 |
| ring | four staging buffers, `(idx + 1) % 4` |
| chunk | `min(buffer_size, remaining)` bytes, skip the leading alignment padding on the first chunk, trim the trailing padding on the last one |
| eligibility | not mmap/checking, buffer present, device present, buffer type is the device default, async + host-buffer + event caps, host buffer type, allocations and backend init succeeded — each failure has its own reason code |
| ordering | without mmap, staged (non-host) tensors load first, largest first |

The buffers, events, transfers and the backend calls stay native.

## Gate

`tests/upload_fixture.cpp` transcribes the native expressions and compares the
module, driving the chunk loop over random reads and comparing a stable sort with
the native comparator.

| corpus | cases |
|---|---|
| alignment aggregation over empty/single/multi file lists | 40 |
| ring rotation including a zero-size ring | 24 |
| 200 random reads (alignment, buffer size, offset, size) with the chunk loop and byte accounting | ~2 800 |
| eligibility matrix covering every reason code | 12 |
| 200 staged-tensor ordering sorts compared element by element | 200 |
| total | **3 147 cases, 4 155 values, 0 failures** |
| ASan + UBSan (runtime, module, fixture) | 0 failures, rc 0 |

Mutations 11/11 caught: alignment floor becomes two (7 failures), aggregation
keeps the minimum (19), ring does not wrap (180), empty ring returns an index
(12), chunk ignores remaining (178), first chunk does not skip padding (155),
trailing padding copied (153), mmap no longer blocks uploads (1), missing host
capability ignored (1), order ignores staged (188), order sorts ascending (196).

## Note on the native assumption

The chunk loop assumes the staging buffer is at least as large as the alignment
(native sizes it at 64 MiB or 1 MiB). The fixture keeps its buffer sizes inside
that assumption; outside it the native expression itself can subtract more padding
than the chunk holds.

## Scope

P9-02 still needs the mmap/`pread` path and the split-expert loader integration. It
stays IN_PROGRESS.
