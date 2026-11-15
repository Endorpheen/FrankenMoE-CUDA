# P11-04 — removal plan for replaced owned implementations

Status: **PARTIAL** — the plan and inventory are accepted; no file is removable
in the tracked tree today.

## Key finding

Nothing in the tracked tree can be deleted now. Every carrier patch lives only in
`/tmp` copies, the native tree has zero `FG_GOOSE` occurrences, and all ten flags
default to OFF. Removal is a function-level operation, not a file-level one.

## Candidates (18: 10 full, 8 partial)

Fully replaced (the native body becomes dead when the flag is ON):

| id | target |
|---|---|
| F1 | `json_schema_to_grammar` entry (the class is still needed by `build_grammar`) |
| F2–F4 | `gguf_write_to_buf`, `gguf_get_meta_size`, `gguf_get_meta_data` |
| F5 | the native `should_split` branch in `gguf-split.cpp` |
| F6–F7 | `quantize_row_q8_0`, `dequantize_row_q4_0` |
| F8 | `determine_vocab_type_native` |
| F9 | `string_strip_native` |
| F10 | `validate_native` |

Partially replaced (8): `ggml_blck_size` (7 of 43 types), `tokenizer_st_partition`,
metrics, stream policy, spec, model and session (the six side libraries are not
engine switches yet), and the quantisation remainder (170 of 172 native targets).

## Substrate (never delete)

Ten groups: libc/libm/POSIX; CPU SIMD kernels; CUDA kernels and template
instances; driver/cuBLAS/NCCL; IQ tables (data); the GGUF container
(reader/writer); model weights; vendored libraries; Apple/Windows branches; the
compiler runtime. The adapter and the 65 helper entry points are primitives
(P11-06), not logic.

## Order (D0 → D4)

1. **D0, no deletions:** put the carriers into the canonical tree, flip the flags
   ON by default, take a rollback copy with `--repro`, restore the Apex reference,
   and close the five failure-recovery items from P11-05.
2. **D1 (S1):** delete F1–F7; the quantisation bodies last or at D4.
3. **D2 (S2):** F10 plus the config/spec side libraries after their call sites
   are switched.
4. **D3 (S3):** F8, F9, the P2 remainder, metrics/stream/model/session; Jinja body
   conflicts are merges, not renames.
5. **D4 (S4):** remove every `*_native` and `#ifdef FG_GOOSE_*`, move oracle
   bodies into `experiments/<EXP>/oracle/`, run the symbol audit, then the Apex
   A/B only with Igor's explicit approval.

Rollback at each step is the previous commit plus a frozen tree, not an in-place
revert.

## Risks

- **R-1:** deleting the native quantisation bodies removes the only byte-parity
  fallback on the decode path while only 2 of 172 row types are gated and no Apex
  A/B exists yet (P11-02/P11-03 are TODO).
- **R-2:** the reference binary `runtime/reconstructed/build/stage-run/llama-server`
  (`badbfc5c…`) is not in the tree; `builds/` is empty and
  `build/expert-tier-franken-cuda/bin/llama-server` is a different binary
  (`3cd3586e…`). The rollback anchor must be restored before the first deletion.

P11-04 is not closed by this plan: even D1 removes 7 of 18 units.
