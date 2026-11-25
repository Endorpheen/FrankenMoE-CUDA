# EXP-2026-11-26-2085 — Panel-major Bonsai FFN down

**Rejected at the component performance gate.** EXP-2084 remains the
cumulative checkpoint. No host routing, runtime or checkpoint change.

## Mechanism

The EXP-2084 full trace measured 5.321 s in full half2 FFN down and
0.881 s in the fused SwiGLU/FWHT/FP16 pack. Transfer EXP-2084's
panel-major idea to the FP16 down path. The fused producer stores
64-value K steps by 128-token panel, step and token. The matching
MMQ reads each K step in contiguous 128-byte chunks across tokens.
The producer uses the existing SwiGLU, Hadamard signs and FP16
conversion. The consumer preserves PQ2 unpack, each MMA and every
FP32 accumulation boundary. The packed buffer size remains the same.
At SM89, the MMQ uses 80 registers rather than 79, the same 27,648 B
shared memory, and zero spills. The probe covers only full tiles.

## RTX 4070 component probe

Same CUBIN and seeded gate/up activations with real GGUF Hadamard
signs. Four real 17408-to-5120 PQ2 down matrices at 1024 tokens.
Balanced medians of eight observations per arm include the fused
activation producer. The outputs were bitwise identical and finite
with exact zero in the zero-input row.

| Tensor | Token-major, ms | Panel-major, ms | Time saved |
| --- | ---: | ---: | ---: |
| Layer 0 down | 3.11048 | 3.05355 | 1.83% |
| Layer 21 down | 3.08370 | 3.09043 | −0.22% |
| Layer 42 down | 3.08275 | 3.04859 | 1.11% |
| Layer 63 down | 3.08318 | 3.03941 | 1.42% |

The candidate misses the 5% gate. A full HTTP request and process
memory measurements for this probe are unmeasured. The retained
EXP-2084 mean remains 986.57 prefill / 32.63 decode tok/s and 8,932 MiB
post-request VRAM. Saved PrismML reports 1076.60 / 47.63 tok/s.
This result covers the producer/consumer pair. It does not isolate
their individual efficiency or establish a limit for FFN down.

## Follow-up hypothesis

Prioritize the 7.991 s INT8 gate/up family. Examine the PQ2 code
expansion used by the paired weight loader. Its PRMT selector can
be formed in two OR/shift/mask stages by spreading a byte first
into two 4-bit groups and then into four 2-bit groups in distinct
nibbles. Exhaustively compare all 256 selector values before timing.
Compare generated SM89 instructions and full pack-plus-projection
cost. Keep code values, paired loads, the panel layout and the
arithmetic contract fixed.

## Reproduce

Use the EXP-2084 combined PTX and the EXP-2071 OpenCL source.
Compile using Clang 18 and CUDA ptxas. The component harness
takes the EXP-1973 bootstrap directory and GGUF inventory.

```bash
exp=experiments/EXP-2026-11-26-2085-bonsai-ffn-down-panel
out=build/bonsai-ffn-down-panel-2085
bash "$exp/source/build.sh" "$out" build/bonsai-q8-panel-layout-2084 build/bonsai-cumulative-half-unpack-2071/pipeline.cl
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" CORE_DIRECTORY INVENTORY "$out/pipeline.cubin"
```

[Component results](data/component.json) and [build receipt](data/build.json).
