# EXP-2026-11-24-1872 — Cap registers in the accepted decode Attention kernel

**REJECTED at the isolated GPU probe.** EXP-1870 traced 839.89 ms
in 5110 four-head decode Attention kernels. The archived EXP-1765
manual2 cubin matches the accepted EXP-1863 embedded image
byte-for-byte. This experiment applies ptxas register caps to its
exact PTX and keeps the source, launch grid, Q4_0 format, query
packing, value order and softmax merge unchanged.

| Cubin | Registers/thread | Stack | Static spill stores/loads | Active CTAs/SM |
| --- | ---: | ---: | ---: | ---: |
| Accepted | 142 | 0 B | 0 / 0 B | 3 |
| Cap 128 | 128 | 0 B | 0 / 0 B | 4 |
| Cap 112 | 112 | 8 B | 24 / 16 B | 4 |

On RTX 4070 the synthetic CUDA Graph repeats pack, attention and
merge for ten distinct layers. Two measurements per arm use A-B-B-A
order, with eight warm-ups and 96 measured replays per block.
The final output matched all 40960 FP32 words. The last layer's
partial outputs and metadata also matched byte-for-byte for each
shape. Values below are mean microseconds for the ten-layer graph.

| KV count / partitions | Cap 128 control → candidate | Cap 112 control → candidate |
| --- | ---: | ---: |
| 17 / 2 | 116.41 → 126.76 | 103.29 → 128.12 |
| 129 / 2 | 175.66 → 195.80 | 155.86 → 198.23 |
| 32769 / 64 | 1757.81 → 1958.10 | 1696.28 → 2044.89 |
| 33279 / 64 | 1700.09 → 1961.48 | 1705.30 → 2046.37 |
| 65537 / 64 | 3293.41 → 3824.87 | 3286.67 → 4001.76 |

The occupancy gain does not offset changes in the compiled
instruction schedule and, for 112 registers, spilling. The
measurements do not isolate each contributing cause. No full
Apex runtime was built, so there are no model memory or throughput
claims. EXP-1863 remains the accepted checkpoint.

## Reproduce

Use `manual2/attention.ptx` and `manual2/attention.cubin` from
the frozen EXP-1765 attention V-unroll build. Copy the latter to
`control.cubin` and verify [SHA256SUMS](SHA256SUMS). With CUDA 12.6:

```sh
ptxas -arch=sm_89 -v --maxrregcount=128 attention.ptx -o cap128.cubin
ptxas -arch=sm_89 -v --maxrregcount=112 attention.ptx -o cap112.cubin
gcc-13 -O2 -Wall -Wextra -Werror -I"$cuda/include" probe.c -Wl,-l:libcuda.so.1 -o probe
./probe "$exp1786/device/goose-cuda-mmq.ptx" control.cubin cap128.cubin
./probe "$exp1786/device/goose-cuda-mmq.ptx" control.cubin cap112.cubin
```

See [cap 128 samples](cap128.log), [cap 112 samples](cap112.log)
and [results.json](results.json).
