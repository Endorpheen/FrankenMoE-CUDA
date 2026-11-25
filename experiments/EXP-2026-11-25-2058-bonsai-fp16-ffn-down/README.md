# EXP-2026-11-25-2058 — FP16 MMA partial accumulation only for Bonsai FFN down

**Not promoted.** EXP-2057 measured an isolated improvement on FFN
down, but broad K32 partial accumulation reduced the code-trace
score. Here only the 17,408-to-5,120 FFN down projection uses the
candidate. The host checks both widths and the FFN down destination.
All other matrix products keep EXP-2049 arithmetic. The CUDA image
and component evidence are reused unchanged from EXP-2057.

## Targeted model gate

Use the EXP-1994 code-trace prompt, 440 raw input IDs, context 36,864,
ubatch 1024, Q4_0 K/V, greedy and normal EOG. A and B use the same
executable and CUDA image. B enables the scoped experiment with
`BONSAI_MMQ_HALF_ACC=1`.

- Independent code-trace accuracy is **18/63 → 15/63** correct cells.
- A and B return 258 and 261 IDs, with a matching 91-token prefix.
- All 11 sampled full-vocabulary rows on identical forced history
  agree on top-1. Top-10 overlap is at least 9 and the maximum
  sampled absolute logit difference is 0.4029.
- At output step 92, forced replay with the same prior 91 IDs gives
  control ID 15 and candidate ID 16. Maximum absolute full-vocabulary
  delta on that history is 0.3006.

The inherited harness labels the experimental arm `int8` in the
JSON, but the changed execution path uses FP16 accumulation.
The decline in independently scored cells keeps this scope out of the
working runtime. The test only measures the known diagnostic task.
No 32K/512 rate or process VRAM/RSS measurements were taken.
Apex and Inkling artifacts and runtime dispatch were unchanged.

## Reproduce

Use the accepted EXP-2049 snapshot and the EXP-2057 candidate build.
Set `FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-25-2058-bonsai-fp16-ffn-down
prior=experiments/EXP-2026-11-25-2057-bonsai-mmq-fp16-accumulator
out=build/bonsai-fp16-ffn-down-2058
fixtures="$FRANKENMOE_CHECKPOINTS/bonsai-integration"
bash "$exp/source/build-quality.sh" "$out" build/bonsai-fp16-acc-2057
bash "$exp/source/build-http.sh" "$out"
OPENBLAS_NUM_THREADS=1 python3 "$prior/source/quality.py" \
  "$out/code-trace" "$out" /path/to/bonsai.gguf \
  "$fixtures/inventory.json" "$fixtures/vocabulary.gguf" /path/to/llama-tokenize code-trace
OPENBLAS_NUM_THREADS=1 python3 "$prior/source/replay.py" \
  "$out" /path/to/bonsai.gguf "$out/code-trace/code-trace"
```

[Task evidence](data/quality.json) and [forced divergence](data/history-91.json).
