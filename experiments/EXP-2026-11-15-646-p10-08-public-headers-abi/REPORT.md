# REPORT — EXP-2026-11-15-645 (P10-08, public headers / FFI / binary interop)

Verdict: **ACCEPTED for the three slices** (S1 `abi_`, S2 `hdr_`, S3 `nm_`);
P10-08 itself stays open, because its acceptance covers the whole public header
surface and symbol-level interoperability (see "What is still open").

## 1. What was frozen

### S1 `abi_` — the ggml type table (7 exports)

`type_traits[GGML_TYPE_COUNT]` from `ggml/src/ggml.c:631-945`, plus
`ggml_blck_size` (1326), `ggml_type_size` (1332), `ggml_row_size` (1338),
`ggml_type_name` (1351) and `ggml_is_quantized` (1357). One row per slot
(`name`, `blck_size`, `type_size`, `is_quantized`) and the row/2-D byte
arithmetic.

Facts worth naming, because a reader of the ABI needs them:

- The type space is **43 slots and every slot is explicit** in the native table;
  there are no zero-initialised holes.
- Five removed slots still carry text that a consumer sees:
  `[4] [5]` = `DEPRECATED` with `blck_size = 0`, `type_size = 0` (a division by
  zero waiting in `ggml_type_sizef`), and `[31] [32] [33] [36] [37] [38]` =
  `TYPE_* REMOVED, use … with runtime repacking` with zero geometry.
- Legacy 32-element blocks: `q4_0` 18 B, `q5_0` 22, `q8_0` 34, `q8_1` 36,
  `iq4_nl` 18. K-quants on 256: `q2_K` 84, `q3_K` 110, `q4_K` 144, `q5_K` 176,
  `q6_K` 210, `q8_K` 292, `iq4_xs` 136, `iq1_s` 50, `iq1_m` 56, `iq2_xxs` 66,
  `iq2_xs` 74, `iq2_s` 82, `iq3_xxs` 98, `iq3_s` 110, `tq1_0` 54, `tq2_0` 66.
- New small-block types in this vintage: `q1_0` 128/18, `q2_0` 64/18,
  `mxfp4` 32/17, `nvfp4` 64/36.

### S2 `hdr_` — 187 numeric entries (3 exports)

Format: `GGML_FILE_MAGIC = 0x67676d6c`, `GGML_FILE_VERSION = 2`,
`GGML_QNT_VERSION = 2` with `GGML_QNT_VERSION_FACTOR = 1000`,
`LLAMA_SESSION_MAGIC = 0x6767736e`, `LLAMA_SESSION_VERSION = 10`,
`LLAMA_STATE_SEQ_MAGIC = 0x67677371`, `LLAMA_STATE_SEQ_VERSION = 3`, the four
state-sequence flag bits.

Geometry and limits: `GGML_MAX_DIMS = 4`, `GGML_MAX_PARAMS = 2048`,
`GGML_MAX_SRC = 10`, `GGML_MAX_N_THREADS = 512`, `GGML_MAX_OP_PARAMS = 64`,
`GGML_MAX_NAME = 64`, `GGML_DEFAULT_N_THREADS = 4`, `GGML_DEFAULT_GRAPH_SIZE =
2048`, `GGML_N_TASKS_MAX = -1`, `GGML_BACKEND_META_MAX_DEVICES = 16`, exit codes
0/1, ROPE modes 0/2/8/24/40, `GGML_MROPE_SECTIONS = 4`, precision 0/10, op hints
0/1.

Call contract: `LLAMA_DEFAULT_SEED = 0xFFFFFFFF`, `LLAMA_TOKEN_NULL = -1`.

Enums: `ggml_status` (-2..1), `ggml_ftype` (28 members from -1), `llama_vocab_type`
(0..6), `llama_rope_type` (-1, 0, 2, 8, 24, 40), `llama_token_type` (0..6),
`llama_token_attr` (0 and bits 0..9, `LLAMA_TOKEN_ATTR_SINGLE_WORD = 512`),
`llama_ftype` (0..41 plus `GUESSED = 1024`), `llama_rope_scaling_type` (-1..3 with
`MAX_VALUE` aliasing `LONGROPE`), `llama_pooling_type` (-1..4),
`llama_attention_type` (-1..1), `llama_flash_attn_type` (-1..1),
`llama_split_mode` (0..3), `llama_load_mode` (-1..4), `llama_lazy_mode` (0..2),
`llama_context_type` (0..1), `llama_model_kv_override_type` (0..3),
`llama_model_meta_key` (0..11).

Runtime limits: `llama_max_devices() = 16`, `llama_max_tensor_buft_overrides() =
4096` (`src/llama.cpp:84` and `:89`).

Structure layout (compile-time freeze, x86-64 LP64): `llama_batch` 56,
`llama_token_data` 12, `llama_token_data_array` 32, `llama_model_kv_override`
264, `llama_logit_bias` 8, `llama_context_params` 160, `llama_model_params` 80,
`ggml_tensor` 336.

### S3 `nm_` — the text contract (6 exports)

- `llama_ftype_name`: 36 explicit cases and a `default`. The mask is
  `ftype & ~1024`; the `(guessed) ` prefix (10 bytes) is kept only when bit 1024
  is set, and it is applied by pointer offset, so `1024|v` and `v` are two
  different strings for the same file type. Note the strings are not plain type
  names: `Q4_K - Medium`, `IQ3_S mix - 3.66 bpw`, `TQ1_0 - 1.69 bpw ternary`.
- `llama_flash_attn_type_name`: `auto`, `disabled`, `enabled`; anything else
  `GGML_ABORT`.
- `llama_load_mode_name`: `auto`, `none`, `mmap`, `mlock`, `mmap+mlock`, `dio`.
- `llama_load_mode_from_str`: exactly those six strings, `strcmp`, case
  sensitive; anything else throws `std::invalid_argument`.
- `llama_model_meta_key_str`: 12 `general.sampling.*` keys, `nullptr` otherwise.
- `ggml_status_to_string`: four texts plus `GGML status: unknown`; the texts are
  prose (`GGML status: error (failed to allocate memory)`), so they are ABI.

## 2. Gate

Oracle: `tests/extract_native.py` re-derives the native tables from
`upstream/llama.cpp` on every build (implemented tables parsed from the `.c`/`.cpp`
files, block sizes and `sizeof` resolved by the compiler from the real headers).
S2 additionally compiles the three public headers directly. Nothing is compared
against a hand transcription.

| slice | cases | values | `-O1` | `-O0` |
|---|---:|---:|---|---|
| `abi` | 106 | 274 | 0 failures | 0 failures |
| `hdr` | 197 | 384 | 0 failures | 0 failures |
| `nm` | 106 | 153 | 0 failures | 0 failures |
| total | 409 | 811 | 0 | 0 |

Every slice is well above the ten-case floor. The two levels produce byte
identical summaries (`results/gate-o1.log`, `results/gate-o0.log`,
`results/gate-final.log`).

Boundary checks in the gates: out-of-range type, `blck_size == 0` rows, a row
that is not a whole number of blocks, a negative row, an unknown key in either
direction, `hdr_key_at(-1)`/`hdr_key_at(count)`, unknown load-mode strings
(case, trailing space, prefix), unknown flash/meta/status values, and a caller
buffer shorter than the text (the port reports the bytes it wrote).

## 3. Mutations — 12/12, halves 6/6

Module generated and compiled at `-O0`, fixture at `-O2`, one mutation at a time.

| # | slice | mutation | caught by |
|---|---|---|---|
| 1-1 | abi | `q2_K` block size 256 → 128 | 4 value failures |
| 1-2 | abi | name `q4_K` → `q4_k` | 1 value failure |
| 1-3 | hdr | `LLAMA_SESSION_VERSION` 10 → 11 | 1 value failure |
| 1-4 | hdr | `LLAMA_TOKEN_ATTR_BYTE` 32 → 16 | 1 value failure |
| 1-5 | nm | guessed prefix loses its trailing space | 49 value failures |
| 1-6 | nm | unknown load-mode string returns −1 instead of −99 | 10 value failures |
| 2-1 | abi | second `DEPRECATED` slot reworded | 1 value failure |
| 2-2 | abi | whole-block guard `ne % blck` dropped | 1 value failure |
| 2-3 | hdr | buft-override limit 4096 → 1024 | 1 value failure |
| 2-4 | hdr | `GGML_MAX_DIMS` 4 → 5 | 1 value failure |
| 2-5 | nm | `penalty_repeat` key pluralised | 1 value failure |
| 2-6 | nm | guessed prefix applied when the bit is clear | 94 value failures |

Halves: 6/6 and 6/6, no survivors, no build breaks.

## 4. Compatibility

- `goose check` passes on each module and on the composition of the three.
- Composition with the 66-module main set (`tools/compose-goose.py` + `goose
  check`): `typechecked ok: 3059 specialization(s)`. The composed unit was also
  generated to C and compiled with `gcc-13 -O1` (3.9 MB `.text`), with **no
  duplicate global symbol** and all **16 exports** present.
- Duplicates: zero name collisions against all 7902 function names in the
  canonical `goose/` tree, exported or not, so `tools/compose-goose.py` needs no
  rename entry and no existing module is shadowed.
- Symbol guard: the 16 new exports are `abi_*`, `hdr_*`, `nm_*`; none of the
  names in `tools/goose-symbols.txt` changes.

## 5. Assumptions

1. `upstream/llama.cpp` at `0b5be7e4a` is the header canon; it is git-ignored, so
   the per-file SHA-256s in `results/hashes.txt` are the durable anchor.
2. The native accessors assert on out-of-range input; the port refuses with `-1`
   instead of trapping. The gate checks refusal as refusal and never calls a
   native branch that would abort or divide by zero.
3. `llama_load_mode_from_str` throws; a C ABI has no exception channel, so the
   port uses `-99` and the gate compares the accepted set exactly.
4. Tables in the port are literals derived once (`tests/gen_port_tables.py`,
   templates in `tests/templates/`). The gate's strength comes from the native
   side being re-derived and from the mutation run.
5. Structure sizes are LP64 facts and are asserted only on a 64-bit build.
6. `abi_row_size` keeps the native multiplication order; the corpus keeps the
   product below 2^63.
7. The frozen identifier set is a curated selection of the public surface. A
   table that grows upstream fails loudly (S1 asserts 43 slots and every parsed
   count in `extract_native.py` is exact), but a brand-new public constant that
   nobody adds to `HDR_*` in `tests/gen_port_tables.py` is invisible to the gate
   until it is added. That gap is the reason P10-08 stays open.

## 6. Conflicts and risks

- **Overlap with P7-04 (quant types).** `goose/model/quant-types.goose` already
  declares the `GGML_TYPE_*` enum values as its own `T_*` constants. This
  experiment freezes the same numbers in `abi_` (as table indices) and in `hdr_`
  (not at all — `enum ggml_type` was deliberately left out of S2). No name
  collides, but the lead should decide whether `quant-types.goose` should later
  drop its private `T_*` block in favour of `abi_`; that is a change to an accepted
  module and is **not** part of this experiment.
- **Overlap with P8-06/P9-01** on `llama_ftype_name`-adjacent output: the model
  router and the CLI print file-type names. They currently carry their own
  strings; `nm_ftype_name` is the single frozen copy, and switching them over is
  host wiring, not a new port.
- **The header surface is not closed.** `ggml_backend_*`, `ggml-opt.h`, `gguf.h`,
  `ggml-cpp.h`, the per-backend headers (CUDA/Metal/Vulkan/...), symbol
  visibility and shared-object versioning, and `carrier/runtime/core/include` are
  outside these three slices.
- **Behavioural ABI is outside the slice.** Callback semantics
  (`llama_progress_callback`, `llama_log_callback` ordering), pointer lifetime
  rules and the `llama_batch` ownership contract are documented contracts that no
  table can freeze; they need the host wiring of P7-10/P9-03.

## 7. What is still open for P10-08

1. The remaining header families listed above (backend/opt/gguf/cpp and the
   device backends).
2. Symbol-level interoperability: an export list and `SONAME`/versioning check of
   the built `libllama`/`libggml-*` against `abi-ledger.json` (165 declarations,
   307 exports at the last audit). This is a link-time gate, not a table.
3. Pointer-typed ABI: callback signatures and the ownership rules.
4. Wiring the three slices into the runtime and the CLI hosts.

## 8. Evidence

```
results/gate-o1.log      all three gates, -O1, 0 failures
results/gate-o0.log      all three gates, module at -O0, identical summaries
results/gate-final.log   final clean run
results/mutate-1.log     mutations 1-6, 6/6 caught
results/mutate-2.log     mutations 7-12, 6/6 caught
results/hashes.txt       sha256 of every artifact and native input
```

Base: assigned `792293ad`, observed working tree `c344b309` (moved by other
slices during the run; irrelevant to these gates). Header canon
`upstream/llama.cpp` `0b5be7e4a25862bc2777d0c47eae18788a8c963a`. Toolchain
`goose-2026.3-string` (`git e0501dc7`, runtime `1e714e61`), `gcc-13` 13.4.0.
