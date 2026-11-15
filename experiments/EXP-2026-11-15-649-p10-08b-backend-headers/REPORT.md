# REPORT — EXP-2026-11-15-651 (P10-08, backend/GGUF headers, SONAME and exports)

Verdict: **ACCEPTED for the three slices** (S4 `bkh_`, S5 `ggh_`, S6 `son_`).
P10-08 stays open for the reasons in section 7.

## 1. What was frozen

### S4 `bkh_` — the backend header surface (8 exports)

39 header values resolved by the compiler from `ggml/include/ggml-backend.h`:
the three buffer usages (0/1/2), the five device types (0..4), the meta device
ceiling (16), the eight split axes (`0..3`, `10 MIRRORED`, `11 PARTIAL`,
`98 NONE`, `99 UNKNOWN`), and size plus every field offset of the five public
structs.

| struct | size | offsets |
|---|---:|---|
| `ggml_backend_dev_caps` | 5 | five `bool` at 0..4 |
| `ggml_backend_dev_props` | 56 | name 0, description 8, memory_free 16, memory_total 24, type 32, device_id 40, caps 48 |
| `ggml_backend_feature` | 16 | name 0, value 8 |
| `ggml_backend_meta_split_state` | 2128 | axis 0, ne 8 (2048 B), nr 2056 (64 B), n_segments 2120 |
| `struct ggml_backend_graph_copy` | 32 | four pointers 0/8/16/24 |

The eleven public callback typedefs are frozen by shape, not by name alone:
arity, return class and each parameter class (void, bool, int, size_t,
float/double, pointer, struct by value). That catches the two changes a
consumer cannot survive: a callback that gains or loses a parameter, and one
whose pointer parameter became a value.

The eight texts of `ggml_backend_meta_split_axis_name` are frozen as text. The
native switch calls `GGML_ABORT` on anything else; the port refuses with `-1`.

### S5 `ggh_` — the GGUF header surface (13 exports)

25 header values: the four magic bytes `47 47 55 46` and the little-endian word
`0x46554747`, version 3, default alignment 32, the alignment key length (17,
not 18 — the gate caught this in the port draft), the thirteen `gguf_type`
members and `GGUF_TYPE_COUNT = 13`, plus `sizeof`/offsets of `gguf_init_params`
(16 bytes, `no_alloc` at 0, `ctx` at 8).

The thirteen type names (`u8 i8 u16 i16 u32 i32 f32 bool str arr u64 i64 f64`)
and element sizes (`1 1 2 2 4 4 4 1 0 0 8 8 8`) come from the source maps. The
two zero sizes are the contract: `str` and `arr` are length-prefixed, not
fixed-size elements, and a lookup must return `nullptr`/0 outside the enum.

Rules that are code rather than data, reproduced instead of tabulated:

- an alignment is accepted iff non-zero and a power of two;
- the data section is padded up to the alignment **only** when the file has at
  least one tensor;
- a parser request of `size` bytes is split into `ceil(size / max_chunk)` chunks
  of `min(max_chunk, remaining)`, `max_chunk == 0` meaning no limit;
- a chunk shorter than requested is end-of-file: the request stops, it is not
  retried, and the parse fails;
- a null reader callback is refused.

The parse itself issues one request per field, and the magic is read element by
element through a vector overload, so a 24-byte header at chunk 7 produces the
sequence `[1, 1, 1, 1, 4, 7, 1, 7, 1]`. A model that only knew the file length
would predict four calls. The runtime arm checks the real sequence.

### S6 `son_` — SONAME, versioning and the export surface (26 exports)

Versions come from the two `CMakeLists.txt`: `ggml` `0.22.0` (SOVERSION from the
major, 0) and `llama` `0.3.0` (SOVERSION 0); `llama-common` and `mtmd` inherit
the llama version. Seven libraries are frozen as:

- stem, version, soversion, `SONAME = lib<stem>.so.<soversion>`;
- the chain `lib<stem>.so -> lib<stem>.so.<sov> -> lib<stem>.so.<version>`,
  verified against the actual `readlink` targets;
- required `DT_NEEDED` entries (e.g. `libllama.so.0` requires
  `libggml.so.0` and `libggml-base.so.0`) and one forbidden entry per library
  (`libllama` must never link a device backend directly: backends are loaded at
  run time).

The export surface is frozen as a reconciliation over 462 header declarations:
every declaration must be exported by one of the libraries its header belongs
to. Facts this produced:

- **235 `LLAMA_API` declarations, all exported** by `libllama.so.0.3.0`.
  Six `llama_*` exports have no declaration:
  `llama_copy_state_data`, `llama_get_expert_cache_stats`,
  `llama_get_state_size`, `llama_load_session_file`, `llama_save_session_file`,
  `llama_set_state_data`. They are enumerated, because a consumer can bind to
  them.
- **116 backend declarations split across two libraries:** 99 in
  `libggml-base.so` (implementation) and 16 registry entry points in
  `libggml.so`. Neither alone covers the header.
- **One declaration is newer than the reference build:**
  `ggml_backend_op_alloc_size_may_expand` (`ggml-backend.h:429`, defined at
  `ggml-backend.cpp:2114`) is absent from the reference `.so`. It is an
  enumerated allowance: any additional unresolved declaration fails the gate,
  and the allowance list itself is compared with the oracle's unresolved set.
- `gguf.h` (61), `ggml-opt.h` (37) and `ggml-alloc.h` (13) are fully exported
  by `libggml-base.so`.
- **The ledger describes a differently named library.**
  `abi-ledger.json` names `libllama-exp371.so.0.7.0` with 307 exports, 165
  declarations, 948 primitive imports and zero structs crossing its ABI. The
  port's SONAME parser splits that name into stem `llama-exp371`, version
  `0.7.0`, soversion `0`; the identity check reports it as not the upstream
  `libllama.so.0`, and the counts are frozen as ledger facts.
- **Export macros.** Running the preprocessor over the real headers gives six
  (macro, shared) cells: `GGML_API`/`GGML_BACKEND_API` expand to
  `__attribute__((visibility("default"))) extern` when shared and to `extern`
  otherwise, `LLAMA_API` to `__attribute__((visibility("default")))` without
  `extern` and to nothing when static. Both the visibility and the `extern`
  difference are in the model.

## 2. Gate

| slice | cases | values | `-O1` | `-O0` |
|---|---:|---:|---|---|
| `bkh` | 65 | 184 | 0 failures | 0 failures |
| `ggh` | 65 | 239 | 0 failures | 0 failures |
| `son` | 498 | 570 | 0 failures | 0 failures |
| total | 628 | 993 | 0 | 0 |

Every slice is above the ten-case floor. The summaries are byte identical at
both levels (`results/gate-o1.log`, `results/gate-o0.log`,
`results/gate-final.log`).

Boundary and refusal checks in the gates: unknown constant index in both
directions, unknown callback id, unknown split axis (`-1, 4, 9, 12, 97, 100`),
unknown GGUF type (`-1, 13, 42, 1000000`), a zero alignment, a non-power-of-two
alignment (2, 3, 7 exercised natively and refused), a short reader chunk and a
null callback (both refused by the reference library), a malformed SONAME
(`llama.so.0`, `libllama.so`, `lib.so.0`, empty), a wrong-length SONAME for the
identity check, and out-of-range library/header indices.

The native runtime arm is not a reimplementation: it `dlopen`s the reference
`libggml-base.so` and compares its answers with the port. The refusal cases of
the library log to stderr (`alignment 3 is not a power of 2`, `failed to read
magic`); those lines are expected and appear in the gate log.

## 3. Mutations — 36/36, halves 6/6 per slice

Module generated and compiled at `-O0`, fixture at `-O2`, one mutation at a
time, 18 mutations per half.

| # | slice | mutation | caught by |
|---|---|---|---|
| 1-01 | bkh | meta device type 4 → 5 | 1 value |
| 1-02 | bkh | meta device ceiling 16 → 8 | 1 value |
| 1-03 | bkh | `MIRRORED` → `MIRROR` | 2 values |
| 1-04 | bkh | split-state size 2128 → 2120 | 1 value |
| 1-05 | bkh | first callback arity 2 → 3 | 2 values |
| 1-06 | bkh | unknown axis returns 0 instead of -1 | 6 values |
| 1-07 | ggh | last magic byte 70 → 71 | 1 value |
| 1-08 | ggh | version 3 → 4 | 1 value |
| 1-09 | ggh | type count 13 → 12 | 1 value |
| 1-10 | ggh | `f32` → `fp32` | 1 value |
| 1-11 | ggh | float element size 4 → 8 | 1 value |
| 1-12 | ggh | default alignment 32 → 16 | 1 value |
| 1-13 | son | first ggml version 0.22.0 → 0.21.0 | 2 values |
| 1-14 | son | `libggml-base` soversion 0 → 1 | 3 values |
| 1-15 | son | SONAME prefix `lib` → `libs` | 7 values |
| 1-16 | son | `libllama` stops requiring libggml | 1 value |
| 1-17 | son | forbidden dependency becomes one it links | 1 value |
| 1-18 | son | backend header mapped to one library only | 100 values |
| 2-01 | bkh | first callback returns void | 1 value |
| 2-02 | bkh | eval callback first parameter becomes pointer | 1 value |
| 2-03 | bkh | device caps offset 48 → 40 | 1 value |
| 2-04 | bkh | compute usage 2 → 3 | 1 value |
| 2-05 | bkh | `NONE` axis 98 → 97 | 1 value |
| 2-06 | bkh | callback parameter stride 4 → 3 | 16 values |
| 2-07 | ggh | zero alignment accepted | 2 values |
| 2-08 | ggh | data offset rounded down | 1 value |
| 2-09 | ggh | last partial chunk dropped | 13 values |
| 2-10 | ggh | last chunk ignores the remaining bytes | 7 values |
| 2-11 | ggh | any non-negative read continues | 1 value |
| 2-12 | ggh | alignment key loses its tail | 1 value |
| 2-13 | son | ledger exports 307 → 306 | 1 value |
| 2-14 | son | allowance name misspelled | 2 values |
| 2-15 | son | legacy export set loses a member | 3 values |
| 2-16 | son | soversion swallows the whole version | 3 values |
| 2-17 | son | identity length 12 → 13 | 2 values |
| 2-18 | son | `LLAMA_API` classified as `GGML_API` | 2 values |

Halves: 6/6 per slice in both halves, no survivors, no build breaks.

One mutation was replaced after its first run: dropping the
`limit > total` clamp in the chunk model survived, because that clamp is
behaviourally dead under `ceil` division (the first chunk is already the whole
request). Mutating "the last chunk ignores the remaining bytes" instead
produces seven failures, so the gate does protect the real branch.

## 4. Compatibility

- `goose check` passes for each module: 69, 74 and 91 specialisations.
- Zero name collisions: the 55 functions of the three modules against all 8233
  function names in the canonical `goose/` tree, exported or not.
- Symbol guard: the new exports are `bkh_*`, `ggh_*`, `son_*`; no name in
  `tools/goose-symbols.txt` changes and no existing module is shadowed.

## 5. Assumptions

1. The header canon `upstream/llama.cpp` `0b5be7e4a` and the reference build
   `build/exp054-dflash/bin` are git-ignored local artifacts. Their SHA-256s in
   `results/hashes.txt` are the durable anchor; a missing artifact fails the
   oracle with an explicit message instead of silently skipping a check.
2. The reference build predates the header canon by at least
   `ggml_backend_op_alloc_size_may_expand`. That single gap is enumerated; a
   second gap fails.
3. Callback shapes are classified by kind, so `int` vs `int64_t` is not a break
   here, while arity, pointer-ness and a struct returned by value are. Pointer
   type identity (`ggml_tensor *` vs `void *`) is not checked; that is a
   source-compatibility concern, not an ABI one.
4. The export-macro arm covers the Linux non-Windows expansion only. The two
   `LLAMA_API` codes are a macro difference, not a platform difference.
5. Native abort/nullptr/throw branches are never executed. The port answers
   `-1` and the gate compares refusal with refusal.
6. Struct sizes are x86-64 LP64 facts, asserted unconditionally by this gate;
   the experiment is recorded as such.
7. The ledger counts are properties of the reconstructed runtime, not of the
   upstream libraries, and are frozen separately.

## 6. Conflicts and risks

- **Ledger identity.** `abi-ledger.json` describes a library whose SONAME is
  `libllama-exp371.so.0.7.0`, while the upstream build produces
  `libllama.so.0`. Consumers that linked the reconstructed runtime bind a
  different SONAME than consumers of the upstream build; nothing in this
  experiment changes that, it only records it. Whether the reconstructed runtime
  should be published under the upstream SONAME is an owner decision.
- **Overlap with EXP-646.** S4 adds the backend header that EXP-646's S2 native
  arm already included for constant resolution; the two slices share no value:
  S2 froze `GGML_BACKEND_META_MAX_DEVICES` in its map, S4 freezes the same
  number as part of a complete backend surface. The duplication is deliberate
  and both gates re-derive it from the header.
- **The reference build is a local artifact.** The son_ slice depends on it.
  A machine without the build cannot run that slice; the oracle fails loudly.
- **Scheduler protocol.** Only the typedef shape of
  `ggml_backend_sched_eval_callback` is frozen. Its ask/observe sequencing needs
  a real backend and stays with host wiring.

## 7. What is still open for P10-08

1. The per-backend headers (`ggml-cuda.h`, `ggml-metal.h`, `ggml-vulkan.h`, ...)
   and `ggml-cpp.h`.
2. `carrier/runtime/core/include`.
3. The scheduler eval-callback ask/observe/cancel sequence at run time and the
   `llama_progress_callback` / log-callback ordering; the batch ownership
   contract.
4. Wiring the six accepted slices into the runtime and CLI hosts, and the
   decision about the ledger library's SONAME.

## 8. Evidence

```
results/gate-o1.log      all three gates, -O1, 0 failures
results/gate-o0.log      all three gates, module at -O0, identical summaries
results/gate-final.log   final clean run
results/mutate-1.log     mutations 1-18, 18/18 caught
results/mutate-2.log     mutations 19-36, 18/18 caught
results/hashes.txt       sha256 of every artifact, native input and .so
```

Base: assigned `1b3f4a1a`. Header canon `upstream/llama.cpp`
`0b5be7e4a25862bc2777d0c47eae18788a8c963a`. Reference build
`build/exp054-dflash/bin` (ggml 0.22.0 / llama 0.3.0). Toolchain
`goose-2026.3-string`, `gcc-13` 13.4.0.
