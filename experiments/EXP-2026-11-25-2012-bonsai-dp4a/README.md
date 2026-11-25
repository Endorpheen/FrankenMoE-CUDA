# EXP-2026-11-25-2012 — Bonsai single-token Q8 DP4A

**Accepted in the experimental Bonsai HTTP line.** The source port
addresses the largest measured decode gap in EXP-2010. The model uses
the same weights and FP16 prefill. Only selected one-token decode
projections with more than 1024 output rows use DP4A. The host passes
the generation phase through the Bonsai session call and resets the
flag after each call. The prompt-ending evaluation keeps FP32.

## Arithmetic and component gate

Each 32-input group has independently rounded signed Q8 codes and an
FP16 scale, matching the relevant PrismML single-column operand
contract. The unused sum halfword in the local packet is zero.
PQ2 code groups use the original one scale per 128 weights. The
kernel computes the eight DP4A terms for each 32-input group, scales
in FP32 and reduces across lanes. Two output rows share the packed
activation on normal projections. The vocabulary uses one output
row per warp. The existing pack arena stores the Q8 packet. Pack
reuse is limited to the same input and invalidated by rotation or
the next evaluation. The F32 fallback remains available.

Seven actual model tensors include a QKV row tail, FFN and vocabulary.
Q8 quantized bytes and F16 scale bytes agree with an independent
NumPy oracle, including zero and half-tie fixtures. The two CUDA
groupings produce identical values. Sampled outputs match the
independent dot oracle within its tolerance. The candidate
changes arithmetic relative to FP32, with 0.81–0.95% projection L2
difference on this synthetic activation set.

| Projection | FP32 rows2, us | Q8 one row, us | Q8 two rows, us |
| --- | ---: | ---: | ---: |
| GDN QKV | 56.22 | 22.58 | 16.40 |
| GDN output | 37.68 | 15.45 | 12.44 |
| FFN gate | 93.19 | 35.23 | 24.47 |
| FFN up | 92.67 | 35.06 | 24.31 |
| FFN down | 101.26 | 35.92 | 26.42 |
| Attention key | 9.73 | 9.41 | 9.30 |
| Vocabulary | 1572.55 | 732.52 | 733.21 |

Times include a separate activation pack per measured projection.

## Quality and throughput

On the four EXP-1994 task fixtures, the independent free-generation
sequences of length 23, 63, 258 and 47 all match exactly. The 37
sampled same-history top-1 choices agree. Worst observed absolute
logit difference is 0.39847 and top-10 overlap is at least 9/10.
Task scoring agrees, including 18/63 code-trace cells and correct
incident extraction. The ledger and scheduling tasks fail in both
arms. This is scoped evidence, not a claim of bitwise equal logits
or a representative evaluation of all prompts.

RTX 4070, same binary and CUDA image within the scored pairs,
context 36864, batch 4096, ubatch 1024, Q4_0 K/V, greedy, EOG masked,
MTP off, affinity 0f0f. A explicitly uses FP32 and B enables Q8.

| Workload | Prefill A → B, tok/s | Decode A → B, tok/s | Decode change |
| --- | ---: | ---: | ---: |
| 4097/64, two balanced pairs | 819.07 → 818.49 | 17.946 → 30.615 | +70.60% |
| 32768/512, one pair B–A | 619.76 → 620.51 | 16.657 → 27.063 | +62.47% |
| 32768/512, rebuilt default, one check | 620.84 | 27.028 | — |

The long pair saves 11.796 s decode and 11.861 s client wall. The
default rebuild repeats the long result without an enabling flag.
All 512 IDs and text hashes in the three full runs match PrismML.
All 64 IDs and text match across both short pairs. In all seven
HTTP arms peak process VRAM is 8932 MiB, peak RSS remains about
7.11 GiB, and process swap and system swap-out stay zero.
Previously released Apex and Inkling binaries and CUDA images are
unchanged. Shared candidate 32K/512 release gates remain open.
The EXP-1977 decode reference is 47.97 tok/s on 32K/512, so the new
native result still trails it by 43.6%.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2012-bonsai-dp4a
dev=build/bonsai-dp4a-repro
test=build/bonsai-dp4a-quality-repro
out=build/bonsai-dp4a-http-repro
base=build/bonsai-flash-q-reuse-2011-http
bash "$ex/source/build.sh" "$dev"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$base/direct.cubin" "$dev/mvq.cubin"
bash "$ex/source/build-quality.sh" "$test" "$dev"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/quality.py" full "$test/quality" "$test" \
    /home/end0/AI/LocalAI/Models/Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf \
    build/bonsai-integration/inventory.json build/bonsai-integration/vocabulary.gguf \
    /home/end0/AI/LlamaServer/llama.cpp/build/bin/llama-tokenize
bash "$ex/source/build-runtime.sh" "$out" "$dev"
```

Use the EXP-2011 server arguments, `"$out/frankenmoe-infer"` and
`BONSAI_CUDA_IMAGE="$out/mvq.cubin"`. Set `BONSAI_DECODE_Q8=off` for A
and leave it unset for B. `ffn` and `vocab` are diagnostic scopes.
Post the exact EXP-1977 request to `/completion`. For the short
request keep the first 4097 prompt IDs and request 64 output IDs.
See [quality](data/quality.json), [paired data](data/results.json)
and [build hashes](data/SHA256SUMS).
