# EXP-2026-11-26-2080 — Full-tile INT8 GDN input projections in Bonsai

**Experimental alternative.** Preserve EXP-2078 as the cumulative
profile. The candidate improves prefill and request time in one
full pair. Decode decreases in that pair, and CodeTrace shows a
task-specific quality risk. Keep the variant for further combinations.

## Mechanism

EXP-2079 identified 3.822 s in the 5120-to-10240 and 5120-to-6144
FP16 grids. These are the paired `attn_qkv` and `attn_gate`
projections in the 48 recurrent Bonsai layers. The [Goose graph](../EXP-2026-11-25-1976-bonsai-trunk/source/trunk.goose)
passes the same Hadamard-transformed activation to both in sequence.

Reuse the EXP-2073 PQ2 × Q8 N128 path with two output strides. The
existing gate/up entry has a fixed stride of 17408 and cannot be used
directly. Select the replacements in the existing Bonsai `MATRIX`
dispatch using widths, output buffers, and complete N128 tiles. The
two launches share the Q8 pack and scale buffer. Invalidate reuse
when another operation overwrites that storage. The existing GDN
math, FP16 tails and count-one dispatch remain in place. No extra
resident allocation. The two added SM89 entries each use 93 registers,
28,416 B of shared memory, and zero spills. The previous 91 CUDA
machine-code sections compare byte-for-byte.

## RTX 4070 component probe

Real PQ2_0 tensors, seeded F32 activations, 1024 tokens. Eight timings
per arm in balanced order after the correctness launches. Medians
include a fresh pack on each call. The component passed the 8% threshold
for both projection families. All outputs were finite and the zero row
was exact. The relative L2 change was 0.648–0.649%.

| Tensor | FP16, ms | INT8, ms | Time saved |
| --- | ---: | ---: | ---: |
| Layer 0 QKV | 1.6604 | 1.4607 | 12.02% |
| Layer 0 gate | 1.0414 | 0.9298 | 10.72% |
| Layer 62 QKV | 1.6607 | 1.4611 | 12.02% |
| Layer 62 gate | 1.0404 | 0.9341 | 10.22% |

## RTX 4070 HTTP 32K/512

Single serial A/B, same executable and CUBIN in both arms. Use staged
EXP-2078 flags, raw EXP-1977 IDs, context 36864, batch/ubatch 4096/1024,
Q4_0 K/V, greedy, seed 123, empty prompt cache, and MTP off.
Only `BONSAI_GDN_Q8_PREFILL` changes.

| Run | Prefill, tok/s | Decode, tok/s | Prefill, s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A · GDN input FP16 | 969.70 | 32.86 | 33.792 | 49.365 | 8932 | 7457192 |
| B · GDN input INT8 | 980.81 | 32.57 | 33.409 | 49.121 | 8932 | 7457636 |
| Saved PrismML | 1076.60 | 47.63 | 30.436 | 41.174 | 8288 | 7651032 |

Prefill improves 1.15%, saving 383 ms, with 244 ms of client time
saved. Decode decreases 0.88% in the pair. All 512 IDs and response
text match in both arms and PrismML. Swap is zero. The candidate
removes 383 ms of the 3.356 s prefill deficit observed in the paired
control. Stable Bonsai, Apex, and Inkling artifacts are untouched.
No cross-architecture inference runs were performed in this experiment.

## Numerical diagnostics

The targeted harness uses ubatch 128 to exercise the new path for
three full chunks in CodeTrace and one in independent fact extraction.
CodeTrace was 22/63 versus 13/63, with the first differing free token
at position 60 (one-based). All 11 sampled fixed-history top-1 IDs match,
the minimum top-10 overlap is nine, and the maximum absolute logit
difference is 0.413210. Both fact extraction runs are correct and
match all 47 free IDs. All nine sampled fixed-history top-1 IDs match
there, with maximum absolute logit difference 0.394138.
These task results leave overall quality and decode repeatability
open. The short prompts do not exercise inherited long-context FP8.
The nominal 9.015 s gate/up family in EXP-2079 remains the largest
single measured PQ2 cost and a higher-payoff target for the next probe.

## Reproduce

Start with the exact EXP-2078 profile and EXP-2076 build inputs.
Generate the EXP-2073 device source as described in its experiment.
Compile the component and the HTTP host in an isolated output directory:

```bash
exp=experiments/EXP-2026-11-26-2080-bonsai-gdn-projection-int8
out=build/bonsai-projection-int8-2080
base=build/bonsai-combined-fp8-pv-2076
bash "$exp/source/build.sh" "$out" "$base" build/bonsai-q8-full-tiles-2073/pipeline.cl
python3 "$exp/source/verify-code.py" "$base/pipeline.cubin" "$out/pipeline.cubin"
bash "$exp/source/build-http.sh" "$out" "$base"
```

Set `FRANKENMOE_CHECKPOINTS` to the checkpoint store for the host
build. Generate the GGUF inventory with EXP-1973's `inventory.py`.
Run `source/check.py CORE_DIRECTORY INVENTORY CUBIN`; the core directory
contains the EXP-1973 `core.so`. Use the same binary and image for
both HTTP arms with `BONSAI_GDN_Q8_PREFILL=0` and `=1`. Serve one
[raw request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
for each arm with all EXP-2078 overrides. The diagnostic fixture can
be built with `source/build-quality.sh FIXTURE EXP2080_BUILD
EXP2078_PACKAGE`. Use its `source/quality.py` adapter with the original
EXP-1994 harness arguments, a tokenizer verified on the tensor-free
vocabulary fixture, and the `code-trace` or `incident-extraction` case.

[Component data](data/component.json), [CUDA identity](data/device-code.json),
[full A/B and hashes](data/results.json), [CodeTrace](data/quality.json),
and [fact extraction](data/incident.json).
