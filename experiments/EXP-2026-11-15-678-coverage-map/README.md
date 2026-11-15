# EXP-2026-11-15-678 — coverage map: every own C++ file against its Goose owner

Base: `e8c34027`. Audit only; no code change, no model, no GPU.

The target is not "N modules integrated" but **zero mandatory functions still
implemented only in the old C++ runtime**. This card is the measurement that
criterion needs: the own C++ engine, file by file, against the Goose module that
now owns its decisions.

## 1. What "own C++" means here

`runtime/core` is the project's own engine, as opposed to the vendored llama.cpp
substrate. 18 files, ~7 600 lines:

| file | lines |
|---|---:|
| `moe/router_hook.cpp` | 1 704 |
| `moe/expert_stream_source.cpp` | 1 463 |
| `moe/dense_weights.cpp` | 479 |
| `io/platform_io.cpp` | 461 |
| `moe/row_stream.cpp` | 277 |
| `config.cpp` | 265 |
| `engine/session.cpp` | 2 070 |
| `moe/deterministic_workload.cpp` | 195 |
| `io/file_reader.cpp` | 195 |
| `moe/gguf_offsets.cpp` | 167 |
| `metrics/csv_metrics_sink.cpp` | 145 |
| `engine/thinking_control.cpp` | 121 |
| `metrics/decode_trace_sink.cpp` | 101 |
| `moe/arch_registry.cpp` | 97 |
| `engine/runtime.cpp` | 70 |
| `engine/ngram_draft.cpp` | 68 |
| `engine/chat_parse.cpp` | 23 |
| `metrics/route_trace_sink.cpp` | 61 |

## 2. Decision ownership: who owns the logic now

| C++ file | Goose owner | exports in the unit | verdict |
|---|---|---:|---|
| `moe/router_hook.cpp` | `goose/model/hook-rules.goose`, `overlap-rules.goose` | `rh_` 18, `ov_` 20 | ported |
| `moe/expert_stream_source.cpp` | `goose/model/expert-lru.goose`, `dense-place.goose` | `xl_` 12, `dw_` 22 | ported |
| `moe/row_stream.cpp` | `goose/model/row-select.goose`, `stream-plan.goose` | `rs_` 12, `sp_` 9 | ported |
| `moe/gguf_offsets.cpp` | `goose/model/gguf-offsets.goose` | 4 | ported (EXP676) |
| `moe/arch_registry.cpp` | `goose/model/arch-registry.goose` | `ar_` 6 | ported |
| `config.cpp` | `goose/cli/config-validate.goose` | `cfg_` 1 (+48 rules) | ported (EXP630) |
| `engine/session.cpp` | `goose/chat/session-loop.goose` | 13 | ported (EXP643) |
| `engine/thinking_control.cpp` | `goose/chat/thinking-control.goose` | 5 | ported |
| `engine/chat_parse.cpp` | `goose/chat/parse-params.goose` | 3 | ported |
| `engine/ngram_draft.cpp` | `goose/misc/draft-source.goose` | `sdp_` 11 | ported (EXP640) |
| `metrics/decode_trace_sink.cpp` | `goose/misc/metrics-traces.goose` | `mt_` 21 | ported (EXP628) |
| `metrics/route_trace_sink.cpp` | `goose/misc/metrics-traces.goose` | `mt_` 21 | ported (EXP628) |
| `metrics/csv_metrics_sink.cpp` | `goose/misc/metrics-traces.goose` | `mt_` 21 | ported |
| `moe/deterministic_workload.cpp` | `goose/misc/tallies.goose` | `mt_` | ported (EXP650) |
| `moe/dense_weights.cpp` | `goose/model/dense-place.goose` | `dw_` 22 | partly: layout ported, the weight copy is native |
| `io/file_reader.cpp` | `goose/model/file-io.goose` | 6 | ported |
| `io/platform_io.cpp` | none | 0 | **substrate**: POSIX/Win32 `open`/`pread`/`mmap`/`VirtualAlloc` |

## 3. The honest count

- **15 of 17 logic files** have a Goose owner with exports in the unit.
- `io/platform_io.cpp` is not logic: it is the OS seam (`open_read`, `pread_at`,
  `file_size`, `alloc_aligned`, `vm_reserve`/`vm_commit`/`vm_evict`), the same
  category as libc. Deleting it would mean re-implementing `pread` and `mmap`.
- `moe/dense_weights.cpp` is **partly** ported: its layout decisions are `dw_`,
  its byte-copy loop is native. That loop is memory movement, not policy, but the
  distinction must be stated rather than assumed.

So the remaining work is not "which modules are missing" but three specific
things:

1. **The wiring.** Ownership is ported; the *call sites* are native. That is what
   P6-11, P7-12, P8-06, P9-01, P9-03, P9-04, P9-05 and P9-08 record as open, and
   it is why this file's table says "ported" while the binary still runs C++.
2. **The engine itself**: graph builder, allocator, backend, KV buffers, HTTP.
   EXP674 lists them; none is owned by an own-C++ file because they are llama.cpp
   code, which is substrate-adjacent but not yet replaceable.
3. **`dense_weights`' copy loop**, if it is judged policy rather than substrate.

## 4. What this map changes about the plan

It reframes the criterion usefully: the count of Goose modules is irrelevant, and
even "every file has an owner" is not enough, because ownership without wiring
leaves the C++ doing the work. The measurable criterion for rule 15's removal
step is:

> for every own C++ file, the binary's call path reaches the Goose export, the
> Goose export answers, and the native body is dead code.

That is what the per-file differential gates already prove for the ten accepted
stages, and what the remaining eight call-site requirements still owe.

## 5. Evidence reused

Every row cites an experiment already accepted (EXP628, 630, 640, 643, 650, 676)
or an export count read from `build/goose-main/main.h`. Nothing was rebuilt or
re-run to write this card.
