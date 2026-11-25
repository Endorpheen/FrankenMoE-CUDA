# EXP-2026-11-25-2003 — Bonsai through the common HTTP completion route

**Functional completion gate passed.** A diagnostic executable links the
EXP-2002 Bonsai session into the frozen EXP-1972 runtime. The archive's
original model host matches the EXP-1953 object byte for byte. The common
HTTP transport, Goose sampler and EXP-2000 CUDA images are unchanged.

The common GGUF reader gets the first attempt. On failure the Bonsai
reader can recognize PQ2_0. The branch requires dense Qwen35 metadata,
the exact Bonsai tensor layout, Q4_0 K/V and supported context and ubatch.
It reuses the common tokenizer and sampler. It evaluates and resets a
resident Bonsai session through the existing generation callbacks. The
Bonsai Goose unit has isolated exported names to avoid link collisions.

## Evidence

One RTX 4070 loopback server, context 36864, batch 4096, ubatch 128,
greedy, seed 123, EOS masked, MTP off. `/health` passed. Three requests
(273/8, 4/8, 273/8) through `/completion` match all 24 IDs from EXP-2002.
Five text prompts and independently prepared token arrays match on input
length and output ID. This is indirect tokenizer evidence. EXP-1973
provides the exact tokenizer ID comparison.

The repeated 273/8 requests report 464.91/15.64 and 463.87/15.63 tok/s.
The server divides decode by seven since the first token closes prefill.
These are diagnostic figures without a paired speed claim. The driver
reports 7,481,217,024 B of GPU weight payload and 1,112,077,568 B of
workspace. Peak VRAM and RAM were not sampled.

`/tokenize` returned 404 on the frozen HTTP checkpoint. The chat route
still has the inherited Apex template and is outside the validated
surface. Keep this artifact on loopback for raw completion diagnostics.
Model-specific chat rendering, explicit handling of unsupported routes,
Bonsai HTTP metadata/streaming and accepted 32K/512 testing remain open.
Production Apex and Inkling artifacts are unchanged. Their same-binary
32K/512 regression gates on the candidate remain open.

## Reproduction

From the repository root, retain the frozen EXP-1972 archive and headers,
the EXP-1953 model-host source and the EXP-2000 and EXP-2002 builds:

```bash
ex=experiments/EXP-2026-11-25-2003-bonsai-http
out=build/bonsai-http-repro
base=$(realpath build/bonsai-mmq-k64-2000-default)
bash "$ex/source/build.sh" "$out"
export CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1
export BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1
export BONSAI_CUDA_IMAGE="$base/mmq-k64.cubin" BONSAI_FROZEN_PTX="$base/frozen-goose.ptx"
taskset 0f0f "$out/frankenmoe-infer" --ctx-size 36864 --batch-size 4096 --ubatch-size 128 \
    --cache-type-k q4_0 --cache-type-v q4_0 \
    /home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf --serve 19103
```

In a separate shell from the repository root:

```bash
python3 experiments/EXP-2026-11-25-2003-bonsai-http/source/check-http.py 19103 build/bonsai-http-repro
```

See the [binary hashes](data/SHA256SUMS), [server log](data/server.txt)
and [first completion response](data/a1.json).
