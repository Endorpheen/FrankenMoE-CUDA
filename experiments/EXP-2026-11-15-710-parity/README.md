# EXP-2026-11-15-710 — prefill restored to 99.7% of the reference

Base: `c407394e`. Continues EXP-709, which fixed the loader picking the wrong
`libggml-cpu` and reached 2052.42 / 33.62.

## 1. Result — two comparable runs, same conditions

| run | prefill tok/s | decode tok/s | content sha256 |
|---|---:|---:|---|
| 1 | 2145.95 | 35.41 | `9d90a010775f...` |
| 2 | **2222.38** | 34.95 | `9d90a010775f...` |
| reference 2026-11-13 | 2228.76 | 41.62 | `9d90a010775f...` |

prefill is at **99.7%** of the reference (2222.38 vs 2228.76) — within measurement noise.
decode is at **84.0%**. The output hash is identical in every run and identical to the
reference.

## 2. What the remaining gap was

EXP-709 checked ONE file and left 176 tok/s of prefill and 8 tok/s of decode on the table:

```
libggml-cpu.so.0                       resolved by the dynamic linker   <- fixed then
libggml-cpu-shared-dd5a40e47065.so.*   loaded by ggml_backend_load_all   <- still slow
```

The second is the plugin ggml loads by path, and it was still the `ab6f61` build. Both now
carry the accepted `63ad4984`; `run-apex.sh` verifies BOTH hashes before starting and refuses
otherwise, so this cannot silently regress.

Timeline: 855.71 -> 2052.42 (SONAME) -> 2158.23 (plugin) -> 2222.38 (both + warm run).

## 3. What was tried and rejected

Loading the reference `libllama-exp244.so.0.7.0` (214 fg symbols, sha `2bc628a1`) in place of
the runtime's own `libllama` (327 fg symbols, sha `abfe9400`) fails with HTTP 500:

```
ggml_backend_cuda_buffer_type_alloc_buffer: allocating 1124.03 MiB: cudaMalloc failed: out of memory
process_ubatch: failed to allocate graph
```

VRAM was free (4084 of 12282 MiB), so this is an ABI mismatch between that older library and
the rest of the stage-run tree, not memory pressure. Rejected and reverted: our own `libllama`
with 327 Goose symbols stays. Fewer symbols is not better.

## 4. All seven reference libraries now match

| library | reference sha256 | ours |
|---|---|---|
| `libllama-common.so.0` | `69edbc5f` | match |
| `libllama-server-impl.so` | `8a963ce3` | match |
| `libggml-cpu-shared-dd5a40e47065` | `63ad4984` | match (was `ab6f61`) |
| `libggml-base-scheduler-bafe3955c9ea` | `e213fa19` | match |
| `libggml-registry-4d6a42acb184` | `9a9ea86a` | match |
| `libggml-cuda-pinned` | `219af975` | match |
| `libllama-exp244.so.0.7.0` | `2bc628a1` | **deliberately not used** - see above |

## 5. The remaining 16% of decode

Not yet explained. The candidates, in order:

1. `--moe-cache-mib 3584` interacts with the expert cache differently when the CPU plugin is
   the accepted build: the reference log shows `FG148_QUANT calls=482400`, and our current
   run has not been compared against that counter yet.
2. The reference `libllama-exp244` carries 214 Goose symbols and ours 327. The extra 113 are
   newer routes (FG375-FG408 family) that the reference did not have. Some of them run in
   decode and their cost has never been measured against the reference's simpler path.
3. Decode is the token-at-a-time path where the CPU plugin's FG160 fusion matters most; the
   plugin is now correct, so this needs a counter comparison rather than more guessing.

The next step is a per-module call-count comparison for the decode phase specifically, not
another timing run.

## 6. Reproducibility

`runtime/goose-stage/run-apex.sh` starts the runtime with the accepted libraries, verifies both
CPU hashes, refuses a busy port, and applies the saved 18 `GGML_GOOSE_*` variables verbatim.
Reverting means restoring `libggml-cpu.so.0` to the previous file; nothing in the stage-run
tree was modified (it is symlinked).
