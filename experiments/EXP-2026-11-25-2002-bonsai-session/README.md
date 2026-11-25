# EXP-2026-11-25-2002 — Bonsai request state and logits adapter

**Functional gate passed.** A narrow C ABI exposes the accepted EXP-2000
Goose/CUDA path to a future model host. It owns an opaque session, accepts
token batches, reports the final token's full logit row when requested,
identifies EOG tokens and resets generation state. A test process opens
one resident model and generates A–B–A with an explicit reset between
requests. This is a dependency for normal HTTP integration.

The adapter calls the same Goose forward graph and CUDA launches as the
EXP-2000 CLI. It validates the full token batch and capacity before a
state-changing operation. A device failure during evaluation requires a
successful reset before more input can be accepted. The caller serializes
operations on a session. Reset clears the current convolution and
recurrent buffers (including the rotating GDN scratch) and restarts
positions from zero. Attention overwrites the active Q4 KV prefix before
reading it. Reset leaves the allocated weight and scratch arenas in place.
The adapter scopes the CUDA context to each operation.

## Evidence

RTX 4070, identical EXP-2000 CUDA images, Q4_0 K/V, context 36864, ubatch
128, greedy with EOG masked and MTP off. A uses the first 273 IDs of
the matched EXP-1977 prompt, B uses four distinct IDs. Each returns
eight tokens.

- All 1,986,560 F32 logits from both A requests agree byte for byte.
- B produces different logits. Its first four IDs match EXP-1976.
- An out-of-range token submitted after the second A prefill returns an
  error. All eight A decode rows still match.
- A separate process running the unmodified EXP-2000 CLI produces the
  same eight IDs and selected logits for A.
- Both processes allocate 7,481,217,024 bytes for weights and
  1,112,077,568 bytes for device workspace. This diagnostic was not a
  performance or peak RSS/VRAM comparison.

See the [raw output](data/session.out), [CLI control](data/control.out)
and [build hashes](data/SHA256SUMS). Existing server paths and Apex/Inkling
artifacts were outside this change. HTTP linking, tokenization through
the common host, generation policy and the complete shared regression
gate remain open.

## Reproduction

Rebuild the EXP-2000 artifact with its own source recipe if necessary.
From the repository root, using its unchanged CUDA images:

```bash
ex=experiments/EXP-2026-11-25-2002-bonsai-session
base=build/bonsai-mmq-k64-2000-default
out=build/bonsai-session-repro
model=/home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf
bash "$ex/source/build.sh" "$base" "$out"
python3 -c 'import json,sys; print(*json.load(open(sys.argv[1]))["prompt"][:273])' \
    experiments/EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json > "$out/ids.txt"
export CUDA_VISIBLE_DEVICES=0 BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1
export BONSAI_FLASH_ORDERED_MERGE=1 BONSAI_FLASH_COMPACT=1 BONSAI_FLASH_ASYNC=1
taskset 0f0f "$out/check-session" "$model" "$base/mmq-k64.cubin" "$base/frozen-goose.ptx" "$out/ids.txt"
read -r -a ids < "$out/ids.txt"
BONSAI_UBATCH=128 taskset 0f0f "$base/bonsai-mmq-k64" "$model" "$base/mmq-k64.cubin" \
    "$base/frozen-goose.ptx" 36864 8 --ignore-eos "${ids[@]}"
```
