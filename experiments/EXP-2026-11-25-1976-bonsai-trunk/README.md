# EXP-2026-11-25-1976 — Resident Bonsai trunk and first greedy parity

**Short functional check passed.** The experiment composes the checked GDN
and full-attention blocks with the Goose GGUF reader and a Goose forward
graph. The Linux driver shim owns CUDA resource allocation, launch packets
and the diagnostic CLI. Apex and Inkling code paths are untouched.

The tested forward step runs all 64 main layers, output RMS norm, signed
Hadamard and the vocabulary projection. It keeps every tensor from the
local GGUF on CUDA, including the inactive MTP suffix. For context 36864,
the allocation contains 7,481,217,024 B of weights and 844,834,816 B of
activation, Q4_0 KV, GDN and scratch buffers. The final tested process
reported 1,810,628,608 B of device memory free after loading. This is an
allocation observation. Phase timing and peak RAM were outside the run.

Greedy generation on input IDs `1511, 17, 248046, 1511` matches PrismML
at all four positions with EOS masked in both engines: `17, 369, 264, 3050`.
The initial native diagnostic omitted the EOG mask and selected EOS ID
248046 at step two. PrismML's teacher-forced raw top candidate was also
248046 at this position, with logprob -0.1950. With the benchmark settings,
PrismML excludes EOG IDs and picks 369. The native diagnostic now uses the
existing Goose tokenizer EOG classification for both stopping and explicit
masking. Full logit equivalence is unverified.

## Reproduction

The build uses the self-hosted KeelWing compiler, GCC 13 and CUDA Driver
API. It requires the EXP-1973 composed core, the EXP-1975 supplemental
cubin and the frozen EXP-1880 Goose image header.

```sh
exp=experiments/EXP-2026-11-25-1976-bonsai-trunk
bash "$exp/source/build.sh" build/bonsai-trunk-repro
build/bonsai-trunk-repro/bonsai-infer \
  "$HOME/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf" \
  build/bonsai-attention-1975/attention.cubin build/bonsai-trunk-repro/frozen-goose.ptx \
  36864 4 --ignore-eos 1511 17 248046 1511
```

The [PrismML reference](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/README.md)
provides the long workload and the EOS diagnostic. The test path evaluates
input positions one at a time. Batched prefill, integration with the normal
inference and HTTP routes, speculative decoding, longer generation parity
and matched performance measurements remain open.
