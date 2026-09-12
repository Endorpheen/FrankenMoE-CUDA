# EXP-2026-09-10-054: PixelML DFlash prototype on the accepted runtime

Status: IN PROGRESS. Implementation and model-free gates only; no baseline change.
Kind: isolated implementation + model-free gate.
Owner: Igor. Implementation: OpenCode agent. Model correctness run: separate step.

## Hypothesis recorded before implementation

The DFlash skeleton already present in the pinned llama.cpp base can host the
PixelML Qwen3.8-Flash-Next DFlash drafter if two concrete mismatches are
corrected:

1. the target tap must expose the contracted attention hyper-connection mix
   output at the boundary layers, not the raw HC-wide residual that
   `qwen4exp.cpp` currently stores in `t_layer_inp`;
2. the DFlash driver must honour the anchor-first layout
   (`dflash.sample_from_anchor=true`, `query_zero_predicts_next`) in the
   DFlash1 path, which today samples the mask rows only.

If both hold, a verifiable prototype can be produced with a small patch,
without rewriting the server. No performance is claimed at this stage.

## Scope

- Converter: PixelML `Qwen3DSparkModel` safetensors to a DFlash GGUF sidecar,
  BF16, no target reload. Explicit checks on rope theta, rotary dimension,
  rotation type, normalization and tensor mapping. The draft uses its own
  default NeoX RoPE; the M-RoPE of the target is not inherited automatically.
- Contracted taps: gated by the existing `embeddings_layer_inp` opt-in, so the
  general `t_layer_inp` contract for other consumers is not changed.
- Anchor-first: explicit support in the existing DFlash driver via the
  existing `sample_from_anchor` GGUF key; no duplicate key.
- CPU round-trips of target features are kept for the prototype; their cost is
  recorded but not optimised here.

## Non-goals

- No performance claim, no 64k run, no aggregate throughput.
- No removal of the CPU feature round-trips.
- No EXP-053 changes.

## Provenance

- Draft: `PixelML/Qwen3.8-Flash-Next-NVFP4-DFlash`, revision
  `9cd660f9050c92fedc88cbe547bd53af0392abe1`, 58 BF16 tensors,
  498,106,880 parameters, no embedding and no LM head.
- Serving reference: `PixelML/deepspec-qwen38-flash-next`,
  `54b35f57af721ffa8d55f1c0ae79f24952a82fd6` (anchor layout patch, HC aux
  overlay, epoch7 plugin).
- Target tap semantics: `deepspec/modeling/dspark/common.py`
  `extract_context_feature` uses `hidden_states[layer_id + 1]`; the serving
  overlay captures the next layer attention HC contraction.
- Base source: pinned `4aaad5d318a790a42c2197975ec8fadbad42602b` plus the
  accepted EXP-038/041/045/046 chain.
- Isolated tree: `work/llama.cpp-exp054-dflash`; isolated build:
  `build/exp054-dflash`.

## Correctness coverage required (separate model step)

- first reject at block position 0 and at position 1;
- partial acceptance with correction token;
- full acceptance with bonus token;
- KV/recurrent rollback and the following step;
- ON and OFF runs compared separately (ON/OFF parity is not derived from one
  ON run).

A single request is a smoke, not the gate. A smaller accepted prefix does not
close the direction; performance is measured later over total emitted tokens
and total time.

## Memory impact

Draft weights 0.928 GiB BF16 (gguf 1,007,227,008 B). At the accepted daily
context (196608) the draft cannot be offloaded to the GPU alongside the
target: `cudaMalloc failed: out of memory` for 998.21 MiB during load. With the
draft on CPU (`-ngld 0`) the same context loads and runs. So 192k currently
requires a CPU draft.

## Results

Model smoke and A/B, one binary (`build/exp054-dflash`), one fixed prompt,
temperature 0, warmup request then a 128-token request, single response.
`decode_tok_s = predicted_n / predicted_ms`.

| Arm | ctx | draft placement | decode tok/s | acceptance | mean len |
| --- | --- | --- | --- | --- | --- |
| MTP n_max=2 | 8192 | GPU | 24.40 | 0.813 | 2.62 |
| DFlash n_max=2 | 8192 | GPU | 17.24 | 0.372 | 1.74 |
| DFlash n_max=2 | 8192 | CPU | 12.76 | 0.372 | 1.74 |
| DFlash n_max=4 | 8192 | GPU | 10.40 | 0.371 | 2.47 |
| DFlash n_max=5 | 8192 | GPU | 9.11 | 0.176 | 1.87 |
| MTP n_max=2 | 196608 | GPU | 19.41 | 0.813 | 2.62 |
| DFlash n_max=2 | 196608 | CPU | 10.24 | 0.372 | 1.74 |
| DFlash n_max=4 | 196608 | CPU | 12.98 | 0.371 | 2.47 |

Longer warm runs (512-token request, EOG allowed) and a GPU q8 draft:

| Arm | ctx | draft | decode tok/s | acceptance | mean len |
| --- | --- | --- | --- | --- | --- |
| DFlash n_max=4 | 8192 | BF16 GPU | 12.00 | 0.232 | 1.93 |
| MTP n_max=2 | 8192 | GPU | 22.89 | 0.724 | 2.45 |
| DFlash n_max=4 | 196608 | Q8_0 GPU | 12.28 | 0.238 | 1.95 |

The Q8_0 draft fits on the GPU at 196608 (540 MB) but is not faster than the
CPU BF16 draft; the bottleneck is not draft placement. Over a longer run the
DFlash accepted length falls to ~1.93 against MTP's ~2.45, and DFlash runs at
~52% of MTP speed. Step cost is the binding constraint: at 8k DFlash is about
161 ms/step against MTP's 107 ms/step, and the accepted length is also lower.

DFlash works: proposals are produced, accepted and verified without abort,
rollback or state errors. But it is a decisive speed loss. The step cost, not
acceptance, is the binding constraint: at 8k, DFlash n_max=4 reaches a mean
accepted length of 2.47 against MTP's 2.62, yet runs at 42% of MTP's speed
(10.40 vs 24.40 tok/s). Larger blocks raise the accepted length but widen the
CPU expert verify (5-6 rows leave the EXP-046 fused guard) and add the CPU
feature round-trips, so throughput falls.

## Verdict

Correctness prototype: WORKS (technical success, useful card).
Speed goal: REJECTED. DFlash does not reach the >=2% paired decode target; it
loses to native MTP by 29% (8k) to 47% (192k) in the current integration. The
CPU feature round-trips and the wider verify batch dominate; closing the gap
would need a large transfer-path rewrite starting from a ~50% deficit, with an
unfavourable ceiling (mean length parity at best).

## Artifacts

- Card; isolated tree `work/llama.cpp-exp054-dflash`;
- build `build/exp054-dflash` (`llama-server` sha256
  `ef40f567e0bc0510945fbcd6a85e10bab25fb57987e46270a5817dfa32f8245a`);
- draft GGUF `models/qwen38/DFlash-PixelML/dflash-pixelml-bf16.gguf` (sha256
  `0553ac6b4fa354b7e74c9e2ebdfa9038cf1ad94d7ba36620c85c26fc33c70006`);
- source delta: 3 files (`src/models/qwen4exp.cpp`, `common/speculative.cpp`,
  `conversion/qwen.py`);
- logs and per-arm JSON under `/tmp/opencode/exp054-*`.
