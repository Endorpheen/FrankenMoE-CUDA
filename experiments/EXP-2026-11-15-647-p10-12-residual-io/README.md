# EXP-2026-11-15-648 — P10-12: residual native logic in the carrier's I/O

Verdict: **ACCEPTED candidate** (model-free, differential) plus the P10-12 audit.
The lead's acceptance is the commit gate. Base commit: `aa9109b5`.

Work was done outside the repository, in
`/tmp/opencode/sub-648/EXP-2026-11-15-648-p10-12-residual-io/`. The shared tree,
the production runtime and other agents' files are untouched. No model, GPU,
server or benchmark was started.

## Hypothesis

P10-12 asks whether every owned implementation path and public entrypoint maps
to a completed Goose task or an audited primitive. The hypothesis was that after
P9-01…P9-08 the carrier (`runtime/`) has no unported decision left that can be
pinned without a model — only syscalls and llama.cpp API glue.

The hypothesis was **partly wrong**, which is the point of the audit: three
carrier translation units carried pure logic that no slice named —
`io/file_reader.cpp` (alignment and window arithmetic), `io/platform_io.cpp`
(the `/proc` text grammar, the residency window, the platform predicates) and
`moe/gguf_offsets.cpp` (which shard files to open). All three are model-free and
all three are now sliced. The remaining residuals are `engine/session.cpp` and
`cli/main.cpp`, both already listed as remainder by the P9-08 package.

`REPORT.md` is the audit: inventory by category, the seven entrypoints, the
residual list with a verdict per region, and the 15 gaps with their owners.

## Why these three slices

The P9-03 inventory (`experiments/EXP-2026-11-15-621-p9-03-stream-policy`)
classified `FileReader`/O_DIRECT/pread, `mincore`, `/proc/self/maps` and the
dma-buf as "native — kernel and hardware". The classification is right for the
syscalls and wrong for the decisions wrapped around them: the alignment window,
the pseudo-file grammar, the residency clipping and the shard naming are
integer and text logic, and a wrong one silently corrupts weights rather than
crashing. They are also the only such logic left in the carrier.

| native file | lines | model-free decision surface | slice |
|---|---:|---|---|
| `runtime/core/src/io/file_reader.cpp` | 195 | default alignment, lane clamp, O_DIRECT verify offset, plain EOF clip, aligned window and its three boundaries, the returned window, bounce growth | **S1** `fr_` |
| `runtime/core/src/io/platform_io.cpp` | 461 | `Key: <n> kB` grammar, the two key tables, the maps line grammar, pathname trim and suffix match, residency clipping and 512-page chunking, platform predicates | **S2** `pr_` |
| `runtime/core/src/moe/gguf_offsets.cpp` | 167 | `-DDDDD-of-DDDDD.gguf` recognition, first-shard rule, count ≤ 1, prefix trim, sibling naming | **S3** `sn_` |

Prefixes `fr_`, `pr_` and `sn_` are free in the active tree. The rest of each
file stays native on purpose: the descriptor and the preads, the `fopen`/
`mincore`/`mmap` calls, the gguf container walk.

## Slices and interfaces

### S1 — `source/file_reader_windows.goose` (`fr_`)

| export | native (file_reader.cpp) |
|---|---|
| `fr_align(align)` | `open()` 33: `align_ = align ? align : 4096` |
| `fr_lanes(lanes)` | `open()` 34: `lanes < 1 ? 1 : lanes` |
| `fr_verify_off(fsize, align, aligned)` | `open()` 60-63: the verify block offset, `fsize/2` rounded down, 0 when the window would pass EOF, -1 when no verify |
| `fr_plain_len(off, nbytes, fsize)` | `read()` 98-113: the EOF clip and `(long long)(end - off)` |
| `fr_aligned_win(off, nbytes, align, fsize, out)` | `read()` 115-119, 134-135, 152: `a0`, `a1`, the bounce size, `read_end`, `bulk_end`, the window, the tail |
| `fr_read_len(off, nbytes, align, fsize, aligned)` | `read()`'s return value in either mode, `0` for a zero-length read |
| `fr_bounce_grow(len, cap)` | `read()` 120-132: `bounce_sz_ < len` |

`end - off` is computed in `i64` exactly as the native computes it in `u64` and
casts to `long long`; two's complement subtraction gives the same bit pattern,
so even a read past EOF returns the same negative value. The alignment mask is
`0 - align`, i.e. `~(align-1)` for the power-of-two alignments the reader takes.

### S2 — `source/proc-readers.goose` (`pr_`)

| export | native (platform_io.cpp) |
|---|---|
| `pr_kb_scan(text, keys, koff, klen, out)` | `scan_kb_file` 274-296, over a text buffer: the faithful multi-key form, returns `found` |
| `pr_proc_memory(text, out)` | `process_memory` 298-305: `VmRSS`, `RssAnon`, `RssFile`, `VmSwap` |
| `pr_proc_device(text, out)` | `device_memory` 307-315: `MemAvailable`, `MemFree`, `SwapFree` |
| `pr_maps_line(line, basename, out)` | `file_mapped_regions` 317-357: the `sscanf` grammar, the trim, the suffix match |
| `pr_resident_pages(off_in_page, sz, page)` | `vm_resident_sample` 231-256: pages fully inside the range |
| `pr_resident_calls(pages)` | the same loop: 512 pages per `mincore` |
| `pr_fault_bytes(page)` | 267-272 |
| `pr_direct_needs_alignment(apple)` | 360-365 |
| `pr_pinned_max(android)` | 379-383 |

Native quirks that are part of the port, all verified against the real reader:
the 255-byte `fgets` chunking (a key at offset 255 starts a fresh chunk and
matches), the colon requirement, `sscanf(" %llu kB")` succeeding on the number
alone (`MemFree: 12abytes` sets 12, `MemFree: 0x10 kB` sets 0), the `-` sign
negating in `u64`, glibc's `ERANGE` clamp to `UINT64_MAX`, the found flag being
the value itself (so `0 kB` does not lock a key but a non-zero value does), the
lenient maps grammar (a missing field shifts the rest), and the suffix match
that accepts `/x/not-<name>`.

### S3 — `source/shard-naming.goose` (`sn_`)

| export | native (gguf_offsets.cpp) |
|---|---|
| `sn_prefix_len(path)` | `split_count_from_path` 111-136, the five assertions |
| `sn_split_count(path, prefix_out)` | the same: count, 0 for a single file, -1 for a non-first shard |
| `sn_shard_path(prefix, no, count, out)` | `make_shard_path` 138-142: `<prefix>-%05d-of-%05d.gguf` |

`stoi` over five digits is decimal accumulation, and `%05d` is zero padding with
the sign kept.

## Gate

Three gates, one per slice, at `-O1` and `-O0`; the summary is identical.

```
FG648_FR cases=105 values=486 failures=0 skipped=0
FG648_PR cases=132 values=417 failures=0
FG648_SN cases= 97 values=145 failures=0
```

`cases` counts distinct inputs, `values` counts compared fields. The native arm
is the **real translation unit**: `file_reader.cpp` and `platform_io.cpp` are
compiled and linked into the `fr` gate, and `platform_io.cpp` /
`gguf_offsets.cpp` are `#include`d by the `pr` / `sn` fixtures so their
anonymous-namespace functions are callable. Nothing is re-implemented to make a
test pass, with four documented exceptions where the native value has no
callable observer:

1. the open-time verify branch (a transcription of `open()` 60-63);
2. the bulk/tail split, checked through its invariants (`bulk_end % align == 0`,
   `tail < align`, `window == read_end - a0`);
3. the shard tail grammar for names the native refuses (where it leaves the
   prefix untouched), transcribed in the fixture;
4. the 512-page chunking, transcribed from the loop.

The `fr` gate creates its own temporary files and reads them through the native
class; the aligned half runs when the scratch storage accepts `O_DIRECT` and
reports `skipped` otherwise (it ran: `skipped=0`, both on tmpfs and on ext4).
The `pr` gate maps a real file of its own so `/proc/self/maps` contains the
basename it searches for, and calls `vm_resident_sample` on a real allocation.
The `sn` gate opens no file at all: the gguf API half of the TU is dropped by
`--gc-sections`.

## Mutations

12 per slice, two halves of six, module generated at `-O0`:

| half | file_reader | proc readers | shard naming | total |
|---|---|---|---|---|
| 1 | 6/6 | 6/6 | 6/6 | 18/18 |
| 2 | 6/6 | 6/6 | 6/6 | 18/18 |

36/36 caught, 0 survived, 0 build failures. The table is `tests/mutate.py`
(`python3 tests/mutate.py 1|2`); each mutation rewrites one decision, for
example: the default alignment becomes 512, the lane clamp accepts zero, the
verify boundary includes a one-block file, the bulk end is not rounded down,
`read()` reports the requested bytes instead of the window, the bounce grows on
an exact fit, the key no longer ends at a colon, the `fgets` buffer grows to
511, the `u64` clamp is dropped, the maps name is matched by prefix, the
residency window ignores the offset in the page, the shard number is parsed in
base 2, one shard becomes a split set, the pad loses a zero.

Three first-round survivors were real gaps and were fixed rather than
re-labelled: the plain EOF clip mutation was semantically equivalent and was
replaced by an off-by-one clip; the bulk end was not observed at all, so the
gate now checks its invariants; and the 255-byte chunking, the `plen < blen`
boundary, the residency offset and multi-digit shard numbers needed corpus
cases that are now in the gate.

## Exact integration step

1. Copy the three modules into the canon, next to the streaming slices:

   ```
   cp source/file_reader_windows.goose goose/model/file-reader-windows.goose
   cp source/proc-readers.goose        goose/model/proc-readers.goose
   cp source/shard-naming.goose        goose/model/shard-naming.goose
   ```

2. In `tools/build-main-runtime.sh`, append to `MODULES` after
   `goose/model/stream-plan.goose`:

   ```
   goose/model/file-reader-windows.goose
   goose/model/proc-readers.goose
   goose/model/shard-naming.goose
   ```

   No `RENAMES` entry is needed in `tools/compose-goose.py`: every helper and
   constant is prefixed (`fr_`, `pr_`, `sn_`; `FR_ALIGN`, `SN_TAIL`).

3. Replace the three native bodies with calls into the composed unit where the
   host already has the values: `FileReader::open/read` take `fr_align`,
   `fr_lanes`, `fr_verify_off`, `fr_aligned_win`; `scan_kb_file`,
   `process_memory`, `device_memory` become `pr_kb_scan` over the file text;
   `split_count_from_path` becomes `sn_split_count`. The syscalls do not move.
   `file_mapped_regions` keeps its `fopen` and calls `pr_maps_line` per chunk.

4. Copy this experiment directory to
   `experiments/EXP-2026-11-15-648-p10-12-residual-io/`, then run
   `python3 tools/sync-goose-tree.py` and `python3 tools/check-goose-tree.py`
   so `goose/MANIFEST.json` records the three files, this experiment and the
   gate.

5. Ledger (`docs/goose-migration/tasks.json`, P10-12 currently `TODO`):
   `status = "PARTIAL"`, `slices_total = 3`, `slices_done = 3`,
   `remaining = ["session.cpp: GenTally/PrefillTally", "session.cpp: prefill
   chunk plan", "session.cpp: trace framing order", "session.cpp: MTP capture
   warm-up", "session.cpp: sampler chain construction (API)", "cli/main.cpp:
   moe-predict report", "cli/main.cpp: driver loop beyond the line protocol"]`,
   `coverage_doc = "experiments/EXP-2026-11-15-648-p10-12-residual-io/REPORT.md"`.
   Refresh `status_totals`, `updated_at` and `work_front`. P10-12 does not close
   with this change: the audit is the closure argument, the seven regions are
   the remainder.

6. Commit subject:
   `EXP-2026-11-15-648: slice the carrier I/O decisions and audit residual native logic (P10-12)`.

## Compatibility and assumptions

- Toolchain: Goose `2026.3-string`, sha256
  `664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab`
  (matches `runtime/compiler/current.json`), runtime `1e714e6146bc`; `gcc-13`
  / `g++-13` 13.4.0.
- ABI: scalars and `u8`/`i64` slices only; `goose_init` is emitted by every
  module and deduplicated by the composer.
- The native arithmetic is `uint64_t`; the port is `i64` and the gates keep
  every value below `2^63`, the domain of real file offsets and sizes. The one
  place where the top of the `u64` range is modelled is `pr_scan_uint`'s
  `ERANGE` clamp, which sets `UINT64_MAX` (bit pattern `-1`); a 20-digit value
  at or below `2^64-1` is out of the gate's corpus and is documented
  there.
- The aligned path of S1 requires storage that honours `O_DIRECT`; the gate
  reports `skipped` instead of failing where it does not. Both filesystems
  available here (tmpfs and ext4) accepted it.
- S2 keeps the native `fgets` chunking, so a pseudo-file line above 255 bytes
  is parsed exactly as the native reader parses it; the fake files in the gate
  are written to exercise that boundary.
- S3 duplicates neither `gsp_split_name` nor `gsp_merge_prefix` from
  `goose/model/gguf-split.goose`: those cover the writer's naming and
  `llama_split_prefix`; this slice is the reader's
  `split_count_from_path` with the first-shard refusal, which the writer side
  does not have.

## Overlaps and conflicts

- `goose/model/gguf-split.goose` (`gsp_`) — no symbol or decision is duplicated;
  see the boundary above.
- `goose/cli/config-resolve.goose` (`cc_`) and `goose/cli/cli-protocol.goose`
  (`cli_`) — S1/S2 touch no flag, no line-protocol field, no emitter.
- `goose/model/expert-lru.goose`, `dense-place.goose`, `stream-plan.goose`
  (P9-03) — they own the LRU, the placement and the slab plan; S1/S2 own the
  read mechanics and the text those plans are fed from. `DenseWeights::init`
  already takes `pinned_max_bytes()` as an input, which `pr_pinned_max` now
  decides.
- `goose/model/hook-rules.goose`, `row-select.goose`, `overlap-rules.goose`
  (P9-04) — untouched.

## Artifacts

```
source/file_reader_windows.goose  S1 module
source/proc-readers.goose         S2 module
source/shard-naming.goose         S3 module
tests/fr_fixture.cpp              S1 gate (native FileReader linked)
tests/pr_fixture.cpp              S2 gate (platform_io.cpp included)
tests/sn_fixture.cpp              S3 gate (gguf_offsets.cpp included)
tests/build.sh                    build and gate runner
tests/mutate.py                   36 mutations, two halves
results/gate-o1.log               gate output at -O1
results/gate-o0.log               gate output at -O0
results/mutate-1.log              18/18 caught
results/mutate-2.log              18/18 caught
results/hashes.txt                SHA-256 of every committed artifact
REPORT.md                         the P10-12 audit
```

Reproduce (from this directory):

```
bash tests/build.sh all            # -O1
bash tests/build.sh all mutate     # -O0
python3 tests/mutate.py 1
python3 tests/mutate.py 2
```

`build/` holds generated C, objects and gate binaries and is not part of the
evidence set.
