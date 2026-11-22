# Working Apex completion profile

The completion API defaults to zero probability rows. An omitted
`n_probs` and explicit `n_probs=0` preserve token sampling. Startup
does not enable `GOOSE_CPU_PROBABILITY_PIPELINE`. Neither the default
nor explicit zero creates a probability worker or prepares its queue.
An explicit positive `n_probs` still returns the original schema and
uses inline calculation. Operators may explicitly enable
`GOOSE_CPU_PROBABILITY_PIPELINE=1` for positive-probability requests.

The fast profile freezes the EXP-1695 executable and uses
6,290 MiB GPU SLRU, 210 MiB temporary prefill workspace, 384 MiB
activation arena, full 18,601 MiB configured host pin budget,
context 262,144, Q4_0 K/V, 4,096/4,096 batch, CPU 8/16, and
the tracked `fast-goose-options.env`.
The ordinary result has 32,768 input tokens, 512 output IDs, and
zero reused prompt tokens.

| Measured in one warmed process, worker flag unset | Prefill tok/s | Decode tok/s | Client s | Probability rows |
|---|---:|---:|---:|---:|
| Field omitted | 3,002.41 | 39.79 | 23.850 | 0 |
| Explicit zero | 3,006.72 | 39.88 | 23.805 | 0 |
| Explicit five | 3,007.20 | 34.00 | 26.040 | 512 |

All responses have identical text and 512 IDs. The five-alternative
response's probability rows are bitwise equal to the separately
verified opt-in worker path. Peak VRAM is 8,766 MiB and peak RSS
21,492,492 KiB. Process swap is zero throughout. Both fast measured
requests have zero system swap-out. The explicit-five request has
319 pages of system swap-out.

Preserved working checkpoint: `build/apex-fast-checkpoint` in the
FrankenMoE worktree. It includes the no-field `request.json`, exact
raw API outputs and SHA-256 manifest. Binary SHA-256:
`e1fbe0d6e49eae229e49a63d9d691c0f858009d885d431f3fb0a6e695691523d`.
The request SHA-256 is
`d81f501a24aa29e1933662fbb8b05d3a1eec5bf7709c622fe7ec63371d8d2d9f`.

Run the ordinary harness with an unset worker flag and that request:

```bash
checkpoint="$PWD/build/apex-fast-checkpoint"
mapfile -t profile < "$checkpoint/goose-options.env"
env -u GOOSE_CPU_PROBABILITY_PIPELINE -u APEX_EXPECT_REFERENCE "${profile[@]}" \
  APEX_GOOSE_BIN="$checkpoint/frankenmoe-infer" APEX_REQUEST="$checkpoint/request.json" \
  APEX_REQUEST_COUNT=2 APEX_CPU_PROFILE=0 APEX_SAMPLE_MEMORY=1 APEX_CTX=262144 \
  APEX_GOOSE_CPU_AFFINITY=1 APEX_GOOSE_THREADS=8 APEX_GOOSE_THREADS_BATCH=16 \
  CUDA_CACHE_DISABLE=0 CUDA_CACHE_PATH="$HOME/.nv/ComputeCache" \
  bash tools/apex-test.sh apex-fast 512 4096 4096 6290
```

This working profile does not replace the explicit-five compatibility
comparison and does not alter the frozen hybrid configuration.
