# P11-04 — removal plan for replaced owned implementations

Status: **PARTIAL** — D0 is done (the carriers are tracked and the tree is
reproducible from the repository); D1–D4 are open.

## D0 — done (EXP664)

The key finding below was that no file was deletable because every carrier lived
only in a `/tmp` copy. That is fixed: `integration/m1/` holds the eleven carrier
patches, the eight stage bridges, the host adapter, the IQ descriptor header and
the twelve gates, and `tools/stage-m1-tree.sh` rebuilds the whole staged tree
from `upstream/llama.cpp` + `integration/m1` + `goose/`.

Reproduced at acceptance: 11/11 patches apply, OFF and ON-all configure and
build clean, `llama-server` is 17 794 640 / 22 136 040 B, `goose_init` is 0 in
OFF and 1 in ON, all ten accepted gates are green in both configurations with
the counters the cards record, and the IQ seam gate passes 15 360 comparisons
with 0 failures. The Apex reference binary was not touched.

The ON size differs from M1's 22 050 592 B because the unit is composed from the
current 110-module list rather than M1's frozen 98; the carrier behaviour, which
is what this step proves, is identical.

## Key finding (superseded by D0)

Every carrier patch now lives in `integration/m1/patches/` with a `TARGETS`
manifest. Removal is still a function-level operation, not a file-level one.

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
   **Carriers: done (EXP664).** Flipping the ten flags ON by default and the
   five failure-recovery items are still open.
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
- **R-2 (closed):** the reference binary was restored from the GitHub release
  `v0.1.0`: `llama-server` SHA-256 `badbfc5c…`, 15 960 bytes, bundle manifest 8/8,
  at `/home/end0/AI/LlamaServer/apex-reference-v0.1.0/`. Details in
  `APEX-REFERENCE.md`. The anchor is available for the first A/B; the binary has
  not been started.

P11-04 is not closed by this plan: even D1 removes 7 of 18 units.
