# EXP-2026-11-26-2104 — CUDA Graph component for Bonsai FFN decode

**Component verified.** No runtime or CUDA kernel changes. Use the
exact EXP-2101 image and 16 distinct Bonsai FFN layers. Preserve
each layer's ten current nodes: RMSNorm, FWHT, Q8 pack, gate and
up MVQ, SwiGLU, FWHT, pack, down MVQ and residual. Use the real
GGUF norm weights, PQ2 matrices and Hadamard signs. Feed each layer
the preceding output and retain the same device buffers and
operand bytes in all variants.

Compare direct CUDA launches, one graph of 160 dependent nodes,
and 16 separate ten-node graphs in one stream. Native C issues
the prepared CUDA launch and per-layer replay calls. There are
no model-server requests in this experiment. Attention, GDN,
token choice, changing positions and KV updates are outside
the fixture.

## Correctness and timing

An early run with an unsynchronized pageable input upload failed
the bitwise comparison. The upload used the default stream while
execution used a nonblocking compute stream. Add a context
synchronization after upload, outside timing, to close that ordering
hole. Exclude both early runs. The corrected probe checks 12 seeded
vectors with distinct outputs. Direct, full-graph and per-layer
replay match bit-for-bit on all vectors.

RTX 4070, same stream and buffers, 24 balanced observations per
mode after setup. GPU interval uses CUDA events around submission.
Wall includes event handling and synchronization. Enqueue time is
isolated from the completion wait. Values below are medians.

| Submission | Enqueue, µs | GPU, µs | Wall, µs |
| --- | ---: | ---: | ---: |
| 160 direct launches | 271.26 | 3254.72 | 3265.55 |
| One 160-node graph | 6.79 | 3097.95 | 3104.81 |
| Sixteen 10-node graphs | 42.48 | 2993.58 | 3063.23 |

Single-graph setup takes 197 µs capture, 471 µs instantiate,
and 69 µs upload and ready synchronization. Sixteen-graph setup
takes 436, 1022 and 261 µs, respectively. Per-layer capture
improves component GPU time by 8.0% and wall time by 6.2% in
this validated block. The experiment does not establish that
per-layer graphs intrinsically beat the single graph. Thermal
and dispatch differences are not separately isolated.

## Next probe

Place one graph around each real Bonsai FFN segment rather than
assuming the entire decode graph can be cached unchanged. Keep
the original fixed layer weights and output and scratch addresses
in each graph. Compile and preserve a non-graph path for unavailable
CUDA Graph support and for nonmatching geometry. Verify that
capture does not run or advance model state and that replay
occurs in the same execution order as Attention and GDN. Check
full HTTP output, allocations, graph lifetime across reset and
cold 32K/512 performance. Preserve EXP-2101 GPU choice and AVX2.

The component intentionally excludes the variable topology and
pointer updates of full decode. Its timings cannot establish a
request gain. Bonsai remains at 1094.97 prefill /
38.89 decode tok/s in the scored cumulative checkpoint. The saved
fresh PrismML comparison remains 1095.12 / 48.08 tok/s.

## Reproduce

Use the EXP-1973 bootstrap directory and GGUF inventory with
the EXP-2101 executable package's CUBIN. The host helper requires
GCC 13 and the CUDA Driver library and headers.

```bash
exp=experiments/EXP-2026-11-26-2104-bonsai-ffn-graph-probe
out=build/bonsai-ffn-graph-probe-2104
mkdir -p "$out"
gcc-13 -O2 -Wall -Wextra -Werror -fPIC -shared -I/usr/local/cuda/include \
    "$exp/source/launch.c" -l:libcuda.so.1 -o "$out/launch.so"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" CORE_DIRECTORY INVENTORY \
    build/bonsai-cumulative-exp2101/cuda/bonsai-http.cubin "$out/launch.so"
```

[Measured observations](data/component.json) and [input hashes](data/build.json).
