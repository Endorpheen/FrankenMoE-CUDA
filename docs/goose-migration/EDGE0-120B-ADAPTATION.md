# Edge0-120B Q3_K adaptation (out-of-ledger work item)

Status: **NOT IMPLEMENTED**. This document records the verified pack layout, the
gap against the current fork and the staged plan. Nothing here is a claim that
the model loads or runs. No server, model or benchmark was started.

The Edge0/strata engine is **not** used as a backend. It is a format reference
only: no process, no Python server, no linked library. All loading and compute
stay inside the FrankenMoE/Goose runtime.

## Inventory

Local folder: `/home/end0/AI/LocalAI/Models/Edge0-120B-q3k` (download in
progress at the time of writing). HF revision
`dab5ce727d494e2018626fe8896bfdc774215b17`.

Complete file list with sizes from the HF API (bytes):

| file | bytes | note |
|---|---|---|
| `model-skeleton.gguf` | 5 991 569 248 | dense backbone GGUF, not downloaded yet |
| `experts.bin` | 28 626 124 800 | 48 x 512 expert records |
| `dense.bin` | 1 538 625 024 | pool for the non-expert pack tensors |
| `latent_dense_q4.bin` | 1 415 577 600 | D/U projections, 4.5-bit format |
| `latent_dense_q8.bin` | 2 673 868 800 | D/U projections, 8.5-bit format |
| `ple-fp8.gguf` | 51 200 246 112 | PLE n-gram table, fp8 |
| `labels.bin` | 98 304 | expert group label per layer |
| `profile.bin` | 196 632 | static expert profile |
| `index.txt` | 117 464 | strata pack index v3, 1367 tensors |
| `native_experts.txt` | 1 458 | strata native experts v6, 48 lines |
| `scales/l0..l47.f32` | 5 120 each | 1280 f32 per layer |
| `mtp/dense.bin` | 116 099 072 | MTP dense weights |
| `mtp/experts.bin` | 707 788 800 | MTP expert weights |
| `mtp/draft_vocab.bin` | 425 196 | draft vocabulary |
| `tokenizer/*` | | vocab, merges, chat template |
| `model.json`, `start.sh` | | launch contract |

SHA256 of the files already present and complete (verified against the HF LFS
object ids where applicable):

| file | SHA256 |
|---|---|
| `index.txt` | `980366930f95ae1b8138652256c3b9be8925b02395d4f093b72410797675a349` |
| `native_experts.txt` | `28566dc85c852619f81afbe368dd0e361df5db06b23e98b41b5ac838e0c29df8` |
| `labels.bin` | `ce9b4a20b92c5c866e65d84a3884c104986522471cf11cc5f797a153affbcc06` |
| `profile.bin` | `543f8a364c8dbfef909ecd1a1d7adbc177d3db18ceecdca1fc568f3eaeb8c655` |
| `model.json` | `7dd6161693d6af7c7ae3b664e546b44120c6e7b3725b4234dd0bbb9172a0b5ba` |
| `mtp/dense.txt` | `991456cf4eb266eb8a888c349e548d11bc640f5dbd9149439aa9786af0f732fd` |
| `mtp/dense.bin` | `c724dc0b0822ada5d2977bf5bde821605feabaa64ea2e0045b67ca656329070a` |
| `mtp/experts.bin` | `09398406be61f1f54c93861f449e48b8df0bfccbc9ec9b2b7636775a6ea9244f` |
| `mtp/draft_vocab.bin` | `b1e1d3a7a9e4bf862dcd5923ce661fb59bbd07907e594df5cf86a62ac235cb91` |

`labels.bin`, `profile.bin` and the three `mtp/` binaries match the HF LFS
ids, so those downloads are complete. `dense.bin`, `experts.bin`,
`latent_dense_q4/q8.bin`, `model-skeleton.gguf` and `ple-fp8.gguf` are still
partial or missing; they are not hashed until complete.

## Derived pack layout (arithmetic proof)

Every size below reproduces the HF byte count exactly, which is the evidence
for the layout. Type ids 11 and 20 are the ggml enum values Q3_K and IQ4_NL.

1. Expert record = 1 164 800 bytes = gate + up + down:
   - gate Q3_K 1280x640 = 819 200 elements at 3.4375 bit = 352 000 B
   - up Q3_K 1280x640 = 819 200 elements at 3.4375 bit = 352 000 B
   - down IQ4_NL 640x1280 = 819 200 elements at 4.5 bit = 460 800 B
2. `experts.bin` = 48 layers x 512 experts x 1 164 800 = 28 626 124 800.
3. `native_experts.txt` v6 line = layer, 11, 20, offset, size; offsets are
   layer stride 596 377 600 = 512 x 1 164 800.
4. `labels.bin` = 48 x 512 x int32 = 98 304, so one group label per expert per
   layer.
5. `scales/lN.f32` = 1 280 x f32 = 5 120, one shared column scale per latent
   dimension per layer.
6. D/U projections = 48 layers x 8 groups x 2 matrices x 1280x2560:
   - 4.5 bit: 2 516 582 400 elements x 4.5 / 8 = 1 415 577 600 (`q4`)
   - 8.5 bit: same x 8.5 / 8 = 2 673 868 800 (`q8`)
   The factor 8 is the routing-group count asserted by the file sizes; the
   group labels in `labels.bin` are the second witness.
7. `index.txt` header: `align 64`, `pool 1538625024` equals the size of
   `dense.bin`, `tensors 1367` equals the number of tensor lines. Columns:
   name, file (0 = pool), dtype (0 = kept in the skeleton GGUF, 2 = f32,
   4 = bf16), offset, nbytes, pool offset, element count, dim1, dim2, then
   flags. A tensor with `nbytes 0` and pool offset equal to the pool size is
   not in `dense.bin` (it stays in the skeleton GGUF); `blk.47.attn_q.weight`
   is such a row.
8. MTP is a separate small model: `mtp/dense.txt` lists q8_0/bf16 tensors for a
   10 240-wide draft with 512-expert gate, and `mtp/experts.bin` is
   707 788 800 B.

## What the current fork already has

`src/models/qwen4exp.cpp` and `src/llama-arch.h` already carry:

- `LLM_ARCH_QWEN4EXP` with `moe_latent_size` and `expert_latent_length`
  hyperparameters, a latent expert path and `qwen4exp::build_layer_ffn`;
- PLE tensors (`PLE_KEY`, `PLE_VALUE`, `PLE_NORM_*`) and a model-level PLE
  table with n-gram hashing in the KV path;
- MTP loading (`load_mtp`, MTP layer filters, speculative decode);
- Q3_K and IQ4_NL CPU/CUDA kernels.

## What is missing (the actual work)

1. A pack reader: `index.txt` v3, `native_experts.txt` v6, `dense.bin`,
   `latent_dense_q4/q8.bin`, `scales/`, `labels.bin`, `profile.bin`.
2. A loader that synthesises the expert tensors from `experts.bin` into the
   qwen4exp model instead of expecting them in the skeleton GGUF, including the
   per-expert scale and input_scale tensors listed in the index.
3. The D/U application per routing group (8 groups) and the combination order:
   D down to 1280, route 10 of 512 plus the shared expert, U back to 2560,
   shared column scale applied. This exact path is not in the current
   `build_layer_ffn`.
4. External PLE: fp8 rows read from `ple-fp8.gguf` with bounded IO (direct or
   mmap), its n-gram/conv state preserved across continue, clear and restore.
5. MTP from this pack (`mtp/dense.bin`, `mtp/experts.bin`, draft vocab).
6. Memory budgets: VRAM expert cache, host-RAM cache, SSD fallback, KV for
   65 536 tokens at int8. None of these numbers are promised before
   measurement on the RTX 4070.
7. An isolated library bundle with verified SONAMEs and hashes (see below).

The router top-k (10 routed + 1 shared) is fixed; it is not a tuning knob.

## No delegation

Reading the Edge0 format documentation is allowed and necessary. Running the
model on `strata`, `edge0-infer`, their Python server or any of their
libraries is not part of this work. Any stage that would need that is reported
as blocked instead.

## Readiness check (2026-11-13)

`tools/check-edge0-pack.py <pack>` validates the download without loading the
weights: expected sizes per file, no `*.part` left, the index v3 header, tensor
count, 64-byte alignment and every offset inside `dense.bin`, the native
experts v6 table (48 layers, types 11/20, record 1 164 800, stride 596 377 600),
`labels.bin`, `scales/` and the latent D/U sizes.

Current result: the complete files pass (index: 1367 tensors, 1065 in
`dense.bin`; native experts: 48 layers), and the pack reports `NOT READY` only
because three downloads are still partial:

| file | downloaded | total |
|---|---:|---:|
| `model-skeleton.gguf` | 3.16 GB | 5.99 GB |
| `experts.bin` | 2.93 GB | 28.63 GB |
| `ple-fp8.gguf` | 1.38 GB | 51.20 GB |

What this means for the plan: E1 (the pack reader and its validator) can be
written and gated **now** against the complete index, native-expert table,
`dense.bin` and latent D/U files. Only E2 onward — binding the skeleton,
loading expert records and running a forward pass — needs the three remaining
downloads. PLE streaming (E5) needs `ple-fp8.gguf` last.

## E1 status: pack reader (2026-11-13)

`tools/edge0/pack.{hpp,cpp}` parses the two text tables into structures the
loader can consume and validates them against the pool:

- index v3: 1367 rows, header `align 64 pool 1538625024 tensors 1367`, 1065
  tensors in `dense.bin`, every in-pool offset 64-byte aligned and inside the
  pool (`max_end` equals the pool size exactly);
- native experts v6: 48 layers, type ids 11/20, record 1 164 800, stride
  596 377 600, every layer offset follows the stride.

`tools/edge0/run.sh [pack]` compiles and runs the gate; on the current download
it reports `FGEDGE0_PACK failures=0`. The reader is the foundation for E2: the
same structures will drive tensor creation in the loader.

## Staged plan (each build and check limited to five minutes)

| stage | deliverable | gate |
|---|---|---|
| E0 | this document: inventory and derived layout | sizes reproduce HF byte counts |
| E1 | pack reader in Goose: index v3 and native v6 parser, offset/size validation, group map | gate over the real `index.txt`/`native_experts.txt`: 1367 tensors, every offset inside its file, expert stride, labels/scales/D-U arithmetic |
| E2 | loader synthesis: skeleton GGUF + pack tensors bound into qwen4exp | model loads without loading all of `experts.bin`; tensor table dump matches the index |
| E3 | one-layer forward parity against an independent CPU reference written from this document | exact values on a fixed input |
| E4 | full latent pipeline: 8 groups, top-10 + shared, scales, D/U | logits parity within the agreed tolerance |
| E5 | PLE direct-IO streaming and state restore | bounded RSS while the 51.2 GB table is streamed; state round-trip |
| E6 | MTP pack + speculative decode | draft acceptance and output equality against non-speculative decode |
| E7 | budgets, 65 536 context, server command | accepted measurements, not estimates |

E1 can start now because its inputs are complete. E2 needs
`model-skeleton.gguf` (5.58 GiB) and `experts.bin` (26.66 GiB) local.

## Isolated library bundle

`/home/end0/AI/FrankenMoE-CUDA/builds/goose-2026-11-13-stage-run` is a copy of
the reconstructed `stage-run` (1.6 GiB, 102 files), not a replacement for it.
The original `stage-run` and the fallback `runtime/exp244/run` are untouched.
The copy was made self-contained: the three `libllama.so*` entries that were
symlinks into the parent directory are real files now, so nothing resolves
outside the bundle.

SONAME and SHA256 of the DSOs actually selected by `ldd` for `llama-server`:

| library | SONAME | SHA256 |
|---|---|---|
| `libllama-server-impl.so` | `libllama-server-impl.so` | `8a963ce35bc919880354b226f5737c6c5d3f2dc47184c291badfe8470a209168` |
| `libllama.so.0` | `libllama.so.0` | `abfe9400488ed9a9e1b5db7d7350967cf45bf06c485f9253cf68dd1dce68afe1` |
| `libllama-common.so.0` | `libllama-common.so.0` | `69edbc5fc5e2a646b427ca0d025beb4d862c6fd4a452d9c6be92da13378e82ed` |
| `libmtmd.so.0` | `libmtmd.so.0` | `099bdb69066ce52d71b3c1c7fa37ad2bbe70ef8271bc0a097e22a79e0c4eb0cd` |
| `libggml.so.0` | `libggml.so.0` | `9a9ea86ac80e725bbfcd89b8a7843a99591eeeaa054639bc76c871fecceccdf8` |
| `libggml-base.so.0` | `libggml-base.so.0` | `e213fa19e1697a37daaf22dd294a0659d95ebd4d57df65b5b4a72bebc3c3d118` |
| `libggml-cpu.so.0` | `libggml-cpu.so.0` | `ab6f61ea793d607830c0f13d2f08e2c7de1d41bbf8766ebd6dd56543e29cc274` |
| `libggml-cuda.so.0` | `libggml-cuda.so.0` | `219af97578109e62823a92acc5f9d50a008637e471ace19ab92d21a763e4b644` |

The CUDA driver (`libcuda.so.1`), CUDA runtime (`libcudart`, `libcublas`,
`libcublasLt`), glibc, OpenSSL and zlib are system libraries; they are listed
in the bundle manifest but are not copied. `llama-server` itself has no RPATH,
so `LD_LIBRARY_PATH` must point at the bundle; otherwise the flint C++ build in
`/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin` is selected.

Verification already done: `ldd` resolves every entry inside the bundle,
`--version` prints the FG witnesses, `--list-devices` sees
`CUDA0: NVIDIA GeForce RTX 4070`, `strace` shows no open of the original
`stage-run` or the flint tree during both runs.

## Target launch command (not runnable yet)

The pack flags do not exist yet. Once E2 lands, the shape is:

```
BIN_DIR=/home/end0/AI/FrankenMoE-CUDA/builds/goose-2026-11-13-stage-run
LD_LIBRARY_PATH="$BIN_DIR" "$BIN_DIR/llama-server" \
  --edge0-pack /home/end0/AI/LocalAI/Models/Edge0-120B-q3k \
  --ctx-size 65536 --host 127.0.0.1 --port 8081
```

## Acceptance checks to agree separately

1. Load: every pack file accounted for, no silent fallback to the skeleton as a
   complete model.
2. Numerics: per-layer and full-logit comparison against an independent CPU
   reference built from this document, fixed inputs, stated tolerance.
3. PLE: bounded RSS during a long prompt, n-gram/conv state exact across
   continue and restore.
4. MTP: draft acceptance rate and output equality against non-speculative
   decode.
5. Memory: measured VRAM/RAM at 65 536 context, with the expert cache and the
   CPU/PCIe fallback, no expert dropped, router top-k unchanged.

## Known risks

1. The 8-group D/U mapping is inferred from sizes and `labels.bin`; it must be
   confirmed when `model-skeleton.gguf` is local.
2. Type ids 11/20 are read from the ggml enum; the data itself has not been
   decoded yet.
3. 51.2 GB of PLE cannot fit in 64 GB RAM alongside the expert cache; streaming
   policy is mandatory, not optional.
4. 65 536 tokens of KV at int8 on a 12 GB card is unproven; it needs a measured
   budget, not an estimate.
5. The current binary knows nothing about this pack; running it on the folder
   today would fail at load.
