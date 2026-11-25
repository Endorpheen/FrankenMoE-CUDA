# EXP-2026-11-25-1974 — Bonsai recurrent layer on CUDA

**PROVEN — component correctness.** No end-to-end performance result.

Hypothesis: the accepted Goose scalar GDN kernel can execute the Bonsai
48-value-head, 16-key-head recurrent update without changes to its device
image. The surrounding graph must retain Prism's L2 normalization for Q/K and
group the GDN output features immediately before the signed Hadamard transform.

The candidate chains real PQ2 embedding lookup, inverse Hadamard, RMS norm,
QKV/Z projection, F32 decay and beta projections, four-tap causal convolution,
Q/K normalization, the frozen Goose GDN decode kernel, gated output norm,
grouped Hadamard, output projection and dense SwiGLU FFN with both residuals.
Weights, intermediate activations and recurrent state stay on-device between
operations. The diagnostic copies output and state out after each token to
compare against independent NumPy arithmetic on the same GGUF weights.

## Results

RTX 4070, local `Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf`, layer 0, token inputs
`1511, 17, 248046, 1511`. Predefined tolerance was `atol=2e-5, rtol=5e-4`.
Maximum absolute errors were 1.79e-6 for the layer output, 3.90e-6 for the
convolution history and 2.40e-6 for the GDN state. Reset and replay reproduced
the exact GPU bytes for all three outputs. The recurring input produced a
different exit activation after history had been populated. The full values
and both device image hashes are in [results.json](results.json).

The initial diagnostic was stopped by a device-image loading error because
the frozen header encodes PTX bytes in decimal. The final harness handles
decimal and hexadecimal byte lists. The shared Apex and Inkling runtime
sources and the accepted build are unchanged. This fixture has no FFN timing,
model throughput, or whole-model peak memory measurement.

## Reproduction

From the repository root, with the EXP-1973 build and the EXP-1880 frozen image
header available. The build uses Clang 18, CUDA libdevice and sm_89 ptxas.
NumPy is a diagnostic-only dependency.

```sh
exp=experiments/EXP-2026-11-25-1974-bonsai-recurrent
bash "$exp/source/build.sh" build/bonsai-recurrent-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-layer.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-recurrent-repro/recurrent.cubin \
  build/checkpoint-apex-exp1880-before-inkling/build-inputs/goose-cuda-image.h
```

The [Prism Qwen3.5 graph](https://github.com/PrismML-Eng/llama.cpp/blob/prism/src/models/qwen35.cpp)
defines the Q/K L2 convention. The [Prism Hadamard graph](https://github.com/PrismML-Eng/llama.cpp/blob/prism/src/llama-graph.cpp)
defines the feature permutation for the folded output projection. The CPU
fixture checks the layer equations using unpacked F32 weights. Generation
parity against a separate runtime remains a later integration check.
