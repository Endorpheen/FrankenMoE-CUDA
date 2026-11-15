# Staged S1.4 — Goose metrics/trace text encoders behind a build flag

Date: 2026-11-15
Base commit: `b18249e0`
Work directory: `/tmp/opencode/sub-633/tree` (the repository was not modified)
Stage: `docs/goose-migration/RUNTIME-INTEGRATION.md`, S1 item 1 family, following the
staged pattern of `experiments/EXP-2026-11-15-626-s1-1-apex-staging`
Module: `goose/metrics/metrics-traces.goose` (canon, `mt_` prefix, 21 exports, 430 lines)
Gate fixed in: `experiments/EXP-2026-11-15-618-p9-07-metrics-traces`

## 1. Goal and result

Goal: prove the switch path for the metrics/trace text encoders without touching
production — one build flag (`FG_GOOSE_METRICS`) that selects the Goose encoders, the
native formatting staying compiled in the same file behind `#else`, and a differential
gate that needs no model, no server and no GPU.

Result: done for the two trace sink translation units.

| check | result |
|---|---|
| composition (`tools/compose-goose.py`) | 1 module -> 15,924 B, no duplicate symbol |
| `goose --standalone` | 191,753 B C + 3,411 B header, 171 declarations, 90 specializations |
| staged build `-DFG_GOOSE_METRICS=OFF` | built, native bodies run |
| staged build `-DFG_GOOSE_METRICS=ON` | built, Goose encoders run |
| gate (53 cases, 91 values) | OFF 0 failures, ON 0 failures, reference 0 failures |
| byte parity, 19 output files | base == off == on, identical |
| dispatch actually entered Goose | `goose_calls=53` on ON, `0` on OFF |
| `mt_` export definitions | 21/21 exactly once in ON, 0 in OFF |
| dynamic symbol table | OFF == ON, identical |
| mutation check | 4/4 caught |
| model, server, GPU | not used at all |

## 2. Environment

```
gcc / g++   15.2.0 (Ubuntu 15.2.0-16ubuntu1)
cmake       4.2.3
goose       runtime 1e714e6146bc, toolchain goose-2026.3-string (git e0501dc7)
kernel      7.0.0-43-generic
```

No GPU run, no `llama-server`, no model file, no commit. Full reproduction (composition,
generation, two builds, three gate runs, parity and symbol audits) takes 11.6 s.

## 3. Base and what was copied

The staged tree is the CPU-only copy of `llama.cpp` made by EXP626, reused as it is. The
files this stage adds are copies of the carrier; every one of them is byte-identical to
`b18249e0` (`git diff b18249e0 HEAD -- <path>` is empty for the whole list):

| file | SHA-256 | role |
|---|---|---|
| `goose/metrics/metrics-traces.goose` | `13850b53…` | the canon, unmodified |
| `runtime/core/include/bmoe/decode_trace.h` | `87649da4…` | unmoved ABI |
| `runtime/core/include/bmoe/route_trace.h` | `0b0c92ad…` | unmoved ABI |
| `runtime/core/src/metrics/decode_trace_sink.cpp` | `996d9865…` | staged copy, hooked |
| `runtime/core/src/metrics/route_trace_sink.cpp` | `d73b0646…` | staged copy, hooked |
| `reference/native/metrics/decode_trace_sink.cpp` | `996d9865…` | pristine copy, the oracle |
| `reference/native/metrics/route_trace_sink.cpp` | `d73b0646…` | pristine copy, the oracle |

`goose/metrics/metrics-traces.goose` hashes to `13850b53…`, the value EXP-618 recorded for
its accepted source, so this stage builds the accepted file and not a mutation of it.

Added by this stage (all inside the staged copy):

| file | purpose |
|---|---|
| `goose-gen/metrics-traces.composed.goose` | composer output, input of the generator |
| `goose-gen/metrics-traces.c/.h` | `goose --standalone` output (generated, not for commit) |
| `common/fg-goose-metrics-bridge.{h,cpp}` | the host seam: `std::string` in, `mt_*` out |
| `tests/fg_goose_metrics_smoke.cpp` | the gate: 53 cases, byte compare with the native format |
| `scripts/metrics-stage-build.sh` | the exact command sequence of section 4 |
| `scripts/metrics-mutate.py`, `scripts/metrics-mutation.sh` | the mutation check |
| `patches/s1-4/*.diff` | the complete source diff against the pristine sinks |
| `evidence/` | logs, gate output, parity, `nm` dumps, hashes, sizes |

The metrics sinks do not live in `llama.cpp`, so the flag is wired into their own targets
rather than into `llama-common` (see section 5).

## 4. Exact commands

```bash
cd /tmp/opencode/sub-633/tree        # or: scripts/metrics-stage-build.sh

# 0. composition: canonical module -> one translation unit
python3 /home/end0/AI/FrankenMoE-CUDA/tools/compose-goose.py \
    goose-gen/metrics-traces.composed.goose goose/metrics/metrics-traces.goose

# 1. goose --standalone: composed Goose -> one C file + exported ABI header
/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose --standalone \
    -o goose-gen/metrics-traces.c --header goose-gen/metrics-traces.h \
    goose-gen/metrics-traces.composed.goose
# -> parsed ok: 171 top-level declarations, 2 file(s)
#    typechecked ok: 90 specialization(s), 2 struct/0 enum instance(s); optimized -O1
#    bce: elided 4/7 index and 0/0 slice checks
#    warning: redundant `as u64` (composed line 110, module line 51)
#    wrote goose-gen/metrics-traces.c (191753 bytes), .h (3411 bytes)

# 2. staged build, flag OFF
cmake -S . -B build-metrics-off -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_METRICS=OFF
cmake --build build-metrics-off -j 32 --target fg-goose-metrics-smoke fg-goose-metrics-smoke-base

# 3. staged build, flag ON
cmake -S . -B build-metrics-on -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_METRICS=ON
cmake --build build-metrics-on -j 32 --target fg-goose-metrics-smoke fg-goose-metrics-smoke-base

# 4. gate: the staged library and the pristine-carrier library, same source
./build-metrics-off/fg-goose-metrics-smoke      evidence/out-off
./build-metrics-on/fg-goose-metrics-smoke       evidence/out-on
./build-metrics-on/fg-goose-metrics-smoke-base  evidence/out-base

# 5. byte parity of the three output trees
diff -r evidence/out-base evidence/out-off && diff -r evidence/out-base evidence/out-on \
  && diff -r evidence/out-off evidence/out-on

# 6. mutation check (module restored afterwards)
scripts/metrics-mutation.sh all
```

The `OFF`/`ON` builds do not compile `llama.cpp`; `add_subdirectory(llama.cpp)` is only
configured (the model library is EXP626's subject, not this one).

## 5. How the flag is wired

`CMakeLists.txt` (staged project) gains one option and three targets:

```cmake
option(FG_GOOSE_METRICS "Select the Goose metrics/trace text encoders ..." OFF)

# the byte oracle: the carrier's sinks exactly as production compiles them
add_library(fg-metrics-base SHARED
    reference/native/metrics/decode_trace_sink.cpp
    reference/native/metrics/route_trace_sink.cpp)

# the staged sinks plus the bridge; the flag picks the encoders inside them
add_library(fg-bmoe-metrics SHARED
    runtime/core/src/metrics/decode_trace_sink.cpp
    runtime/core/src/metrics/route_trace_sink.cpp
    common/fg-goose-metrics-bridge.cpp)

if(FG_GOOSE_METRICS)
    add_library(fg-goose-metrics OBJECT goose-gen/metrics-traces.c)   # C11, PIC, -w
    target_compile_options(fg-goose-metrics PRIVATE -w -fvisibility=hidden)
    target_compile_definitions(fg-bmoe-metrics PRIVATE FG_GOOSE_METRICS=1)
    target_include_directories(fg-bmoe-metrics PRIVATE goose-gen)
    target_link_libraries(fg-bmoe-metrics PRIVATE fg-goose-metrics)
endif()
```

`runtime/core/src/metrics/decode_trace_sink.cpp`, `write_csv_field`:

```cpp
void write_csv_field(std::FILE * f, const std::string & v) {
#ifdef FG_GOOSE_METRICS
    const std::string enc = fgmt::field_encode(v);
    std::fwrite(enc.data(), 1, enc.size(), f);
#else
    ...the original body, unchanged...
#endif
}
```

`CsvComputeTraceSink::on_rows` and `CsvIoTraceSink::on_rows` replace their
`fprintf`/`write_csv_field`/`fputc` sequence with one `fgmt::compute_row` /
`fgmt::io_row` call and one `fwrite`; `CsvRouteTraceSink::on_rows` replaces its two-arm
`nan`/`%.6g` sequence with `fgmt::route_row`. In every case the arguments are the
struct fields in the native print order and the native text is kept verbatim in the
`#else` branch. The preamble, the column headers, the `FILE*` ownership, the destructor
and the `fflush` policy are untouched and stay native in both configurations.

`common/fg-goose-metrics-bridge.cpp` is the only new translation unit in the library. It
sizes a buffer for the encoder it is about to call, lets the module report the length it
would have written, grows once if that length is larger, and returns a `std::string` of
exactly that length. `goose_init(0, nullptr)` runs once through `std::call_once` on the
first encode. `mt_fmt_g6` — the module's one libc dependency — is defined here.

Properties that matter for the production rule ("a new feature's flag exists only to
disable it"):

- OFF is the default and runs the native bodies byte for byte;
- the native bodies are compiled in both configurations, so switching the flag off is a
  rebuild, not a revert, and no native call is deleted;
- the sinks' signatures, factories and struct layout are unchanged; the emitted bytes are
  unchanged; only the code that produces them moves.

## 6. The gate

One binary per configuration, from the same source, linked against a different library.
It drives the public factories (`make_csv_compute_trace_sink`, `make_csv_io_trace_sink`,
`make_csv_route_trace_sink`), writes a file per case, reads it back and compares it with
the native format transcribed in the fixture — preamble line, column header, then every
row byte for byte. The transcription is not trusted: the reference binary links the
pristine carrier copies and must pass every case, and the three output trees must be
identical.

| slice | cases | what it pins |
|---|---:|---|
| S1 fields | 16 | quote predicate (`,` `"` `\n`; `\r` raw), RFC4180 doubling, newline-to-space, empty field, 3,000-byte field, 600 commas, raw non-UTF-8 bytes |
| S2 compute rows | 12 | nine columns, signed layer/step, `%llu` counters up to `2^64-1`, escaped `op`/`name` |
| S3 io rows | 12 | twelve integer columns, signed `expert`/`proj`/`lane`, unsigned `spec`/sizes/latency, `uint8_t` 255 |
| S4 route rows | 13 | `nan`, negative `nan`, `inf`, `-inf`, `-0`, subnormal, 1/3, 1,234,567, `%.6g` column, residency, dropped |

```
FG633_S1 cases=16 values=48 failures=0
FG633_S2 cases=12 values=14 failures=0
FG633_S3 cases=12 values=14 failures=0
FG633_S4 cases=13 values=15 failures=0
FG633 mode=native cases=53 values=91 failures=0 goose_calls=0      # OFF library
FG633 mode=goose  cases=53 values=91 failures=0 goose_calls=53     # ON library
FG633 mode=native cases=53 values=91 failures=0 goose_calls=n/a    # pristine carrier
```

Every field case reaches the encoder through `mt_compute_row_encode`, which is the
production path in the ON build; the standalone `mt_field_encode` export wraps the same
`mt_field_into`.

### Mutations

`scripts/metrics-mutate.py` rewrites one decision of the staged module; the script
recomposes, regenerates, relinks the ON library and reruns the same gate. The module and
the generated C are restored on exit (verified: `13850b53…` and `b3705fa1…` afterwards).

```
MUT 1 caught   failures=3    comma dropped from the field quote predicate
MUT 2 caught   failures=10   newline written raw inside a quoted field
MUT 3 caught   failures=12   io row prints the requested size as the read size
MUT 4 caught   failures=1    NaN route weight takes the numeric arm
```

Mutation 4 is the interesting one: it survived the first corpus, because glibc's `%.6g`
of a *positive* NaN also prints `nan`. The difference is observable only for a
**negative** NaN, where `%.6g` prints `-nan` while the native arm prints `nan`. The corpus
now carries `-nanf("")`, so the NaN arm is pinned by bytes and not by intent.

## 7. Symbols

```
mt_ export definitions in the ON library     21 / 21 exactly once
mt_ definitions in the OFF library            0
```

`evidence/nm-export-counts.txt` lists all 21 with `on=1 off=0`. The ON library defines 25
`mt_`-prefixed symbols in total: the 21 exports, the host shim `mt_fmt_g6`, and three
compiler-local helpers (`mt_put_g.part.0`, `mt_put_u64_g`, `mt_field_into_g`).

The dynamic symbol table does not move:

```
$ diff evidence/abi-off.txt evidence/abi-on.txt        # empty
T _ZN4bmoe22make_csv_io_trace_sinkERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE
T _ZN4bmoe25make_csv_route_trace_sinkERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE
T _ZN4bmoe27make_csv_compute_trace_sinkERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE
T _ZN4fgmt5callsEv
V _ZTIN4bmoe12IIoTraceSinkE ... (three typeinfo/vtable symbols)
```

Against the pristine carrier library the only difference is `fgmt::calls` — the gate's
counter accessor. A production integration would drop it or make it hidden; it is
identical in OFF and ON, which is what keeps the switch a pure code swap.

## 8. Sizes

| artefact | bytes |
|---|---|
| `goose/metrics/metrics-traces.goose` (input) | 15,835 |
| `goose-gen/metrics-traces.composed.goose` | 15,924 |
| `goose-gen/metrics-traces.c` (generated) | 191,753 |
| `goose-gen/metrics-traces.h` (generated) | 3,411 |
| `metrics-traces.c.o` (Goose object, `-O3 -fPIC -fvisibility=hidden`) | 18,056 |
| `libfg-metrics-base.so` (pristine carrier sinks) | 19,160 |
| `libfg-bmoe-metrics.so` OFF | 19,248 |
| `libfg-bmoe-metrics.so` ON | 44,408 (+25,160, +131 %) |
| `fg-goose-metrics-smoke` | 56,920 |

The OFF library's text section is 88 bytes larger than the pristine one (7,584 vs 7,496)
and the only extra defined symbol is `fgmt::calls`: the counter stub. The ON growth
is the Goose object (18 KB) plus the bridge and its `std::string` machinery.

## 9. SHA-256

Sources and stage files (`evidence/sha-sources.txt`):

```
13850b53f856344b115ff333b937f5e1564c74f0f98b9268900c82cb06382eda  goose/metrics/metrics-traces.goose
2136dcf99b4b76c1f360cdbec4d66f6a6cb482655cf08cdc92bf4a8f587f85c2  goose-gen/metrics-traces.composed.goose
b3705fa12c144c6e3ad6d724e36d8d0c39a631a8872bfcf30ce1aa9d616cbd61  goose-gen/metrics-traces.c
0acd73e68f45ac1ce5edfe01107fef19a3b0da11252dafac59878895321edbbc  goose-gen/metrics-traces.h
10e11753cc4e164ade8f7be607ac0f3170c6874cbe2dbb36257b739477104093  runtime/core/src/metrics/decode_trace_sink.cpp
116caada894ba83e0df7cdf3d780aa278a5b76575a32bacaacc6bfaeb937e629  runtime/core/src/metrics/route_trace_sink.cpp
996d98657503ead0c8b58f508ced3ed3690d034b65a167186fa581d92b029b96  reference/native/metrics/decode_trace_sink.cpp
d73b06465305a9520df33fd7ac9fae6e7d9d20f2439189afde5767e9ae3390a6  reference/native/metrics/route_trace_sink.cpp
eef56737a5391eabd892fc90c8050c4596719bb93782f60bb9dbae23143ce6ae  common/fg-goose-metrics-bridge.cpp
90df443933e4f86b07af7bce901590e5d800e55bbfd7f49c87a6c01f36762f21  common/fg-goose-metrics-bridge.h
b24b5d935d2ac5857b5c39d8871aec7d7eea9c2ea89f778689d4587a13556976  tests/fg_goose_metrics_smoke.cpp
78b761f464f8dcb205dc7db58904a031a479441feb49500b9702a3afd8f3c36e  CMakeLists.txt
64121976de5b2917b77cd941627385b3d75a44fc2904d3b43e5c90f297157ea7  scripts/metrics-stage-build.sh
9d2531d21739dcd7e0bb528c09465bb1aa56b43ec8a13c3f26e426548c3a285f  scripts/metrics-mutate.py
0a89387f07e8f23444dcd424827ad1994aae655256a50d901140579fb085b955  scripts/metrics-mutation.sh
84cb8c3421b32e46d40befd462398108926a2b6e69c812c979beca9853937680  patches/s1-4/decode_trace_sink.cpp.diff
3923e665acdc5f9ef803727a003d7095b10bebb7473f3ebfc599d3dc1eeda8ab  patches/s1-4/route_trace_sink.cpp.diff
```

Artefacts (`evidence/sha-artifacts.txt`):

```
b4b4e0903d79d349390b3b2d7067611a2f5887b93c7fc361cf332fd6c4cd0dd3  build-metrics-on/libfg-bmoe-metrics.so
f6b0e5241259162001c796089602ffdd4e36801ed9261b165986868d2ca310fd  build-metrics-off/libfg-bmoe-metrics.so
e4525ef5066131ec9f8a9a505cb5bbc8569e145c61a50eb169c1703844543554  build-metrics-on/libfg-metrics-base.so
271c19b1f04a43e20aba7173ed20290c52f68997e83a10e054aed1a817d22492  build-metrics-on/fg-goose-metrics-smoke
2c29a44d990d0a9234ddb3a922fb2268adaba8f0ae924405ecebaacf806068b5  build-metrics-off/fg-goose-metrics-smoke
35fb84305b6e6241a3a4c9500ac0d9565ba6350b05c56ba99424c515f9e2162f  build-metrics-on/CMakeFiles/fg-goose-metrics.dir/goose-gen/metrics-traces.c.o
```

`goose-gen/metrics-traces.h` hashes to `0acd73e6…`, the same value EXP-618 recorded for
its generated header: the exported ABI is unchanged across the two experiments.

## 10. Sizes of the seam and its borders

**The one conversion left to libc.** The route weight column is `%.6g`, and the module
cannot format a double. `mt_fmt_g6` is one definition in the bridge — `snprintf("%.6g")`
at the offset the module chooses, returning the length libc would have written. The column
is byte-exact only against the same libc the sink is compiled against; a different libc
must be the one the process links, not a reimplementation. This is the only place where
the module does not own its own bytes.

**The main seam.** The module owns the text of one field and one row and nothing else.
Everything around it stays native and was deliberately not moved:

- `std::FILE*` ownership, `fopen` mode (`"wb"` for compute/io, `"w"` for route), the
  destructor's `fclose`, and the `fflush` cadence (once per decode, not per row);
- the `#` preamble and the column header of all three traces, including the route trace's
  per-layer `expert_bytes`/`dense_bytes` loop — the module exports no preamble encoder;
- the struct-to-argument mapping: the bridge widens `int8_t`/`int32_t`/`uint8_t`/`float`
  exactly as the native call did, and that widening is the caller's contract, not the
  module's.

The bridge is therefore "one row in, one `std::string` out". Its cost is one allocation
and one copy per emitted row, in exchange for one `fwrite` instead of four to six
`fprintf`/`fputc` calls. That is the honest price: the traces are diagnostics, but the io
trace is on the read path, so a pooled buffer (or an encoder that writes into a caller
arena) is the next step before any measurement of trace overhead. The module already
supports that shape — every encoder writes into a caller-owned slice and
`mt_field_into` already takes a running offset — the
`std::string` is the bridge's choice, not the module's.

Two consequences worth recording:

1. `write_csv_field` is unreachable in the ON build (the row encoders do the field
   themselves), so it is marked `[[maybe_unused]]` and kept compiled as the switch-off
   path. Deleting it would make turning the flag off a revert.
2. `goose_init` is called lazily on the first encode. The native sinks are single-writer
   per file; the module's behaviour under two concurrent encodes from different threads is
   not established and is not needed by the current call sites.

## 11. What is still open

1. **The metrics CSV itself is not switched.** `csv_metrics_sink.cpp` still prints its 29
   per-token columns and its 62-key summary trailer natively. The module exports only the
   two width constants (`mt_metrics_cols`, `mt_summary_keys`) and no row or key encoder, so
   switching that file needs either 62 key encoders or a key-table contract in the module.
   Until then `mt_metrics_cols`/`mt_summary_keys` are constants that can drift.
2. **The deterministic workload is not wired.** `mt_wl_*` (10 exports) is compiled and
   present, but `runtime/core/src/moe/deterministic_workload.cpp` is not part of this
   stage and no workload gate was run. The EXP-618 gate covers it; this stage does not.
3. **Reader-side exports are unused here.** `mt_count_cols`, `mt_unescape_field`,
   `mt_field_needs_quote` serve a reader, and no reader was staged.
4. **Per-row allocation.** As in section 10; a pooled arena is required before the io trace
   path is measured.
5. **No model, no server, no GPU.** Nothing here claims a step toward the S1 acceptance of
   identical model output; that needs the runtime and Igor's explicit approval.
6. **Optimization level.** The generated object is built by the Release configuration
   (`-O3 -DNDEBUG -fPIC -w -fvisibility=hidden`), not at the repository's `-O1` gate
   convention. Mutations rebuild the same ON library; they do not re-run at `-O0`.
7. **Error paths.** The sinks have none for formatting (they cannot fail), so there is
   nothing to compare on the failure side; buffer growth is the only failure-adjacent
   branch and it is exercised by construction, not by a case.
8. **Generated artefacts.** `goose-gen/` and `build-metrics-*/` are reproducible from the
   Goose file plus `scripts/metrics-stage-build.sh`; the generated C and the `.so` files
   are not for commit.

## 12. Raw evidence

```
evidence/goose-compose.log        python3 tools/compose-goose.py, 1 module
evidence/goose-generate.log       goose --standalone, 90 specializations, 191,753 B
evidence/off-configure.log        staged configure, flag OFF
evidence/on-configure.log         staged configure, flag ON
evidence/off-build.log            staged build, OFF
evidence/on-build.log             staged build, ON
evidence/smoke-off.txt            FG633 mode=native cases=53 values=91 failures=0 goose_calls=0
evidence/smoke-on.txt             FG633 mode=goose  cases=53 values=91 failures=0 goose_calls=53
evidence/smoke-base.txt           FG633 mode=native cases=53 values=91 failures=0 goose_calls=n/a
evidence/parity.txt               base == off == on, 19 files
evidence/nm-on.txt                full symbol table, ON library
evidence/nm-off.txt               full symbol table, OFF library
evidence/nm-export-counts.txt     21 exports, on=1 off=0 each
evidence/abi-off.txt, abi-on.txt, abi-base.txt, abi-parity.txt
evidence/mutations.txt            MUT 1..4, all caught
evidence/sha-sources.txt, sha-artifacts.txt, sizes.txt
evidence/s1-4-script-run.log      the whole sequence, 11.6 s
evidence/out-off/, out-on/, out-base/   19 files each, byte-identical across the three
evidence/s1-1/                    the EXP626 evidence that came with the staged tree
```
