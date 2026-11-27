# Goose tensor migration

## Measured call counts

RTX 4070, GigaChat4 Q4_K_M, 32768 input tokens and 512 generated tokens.
The diagnostic binary used the EXP-2214 source with the
[tensor counters](../experiments/EXP-2026-11-27-2214-goose-operand-span-reuse/source/tensor-counts.patch)
enabled in an isolated worktree. Model data was resident before startup.
All 512 output IDs, text and stop type matched v0.15.0. The diagnostic
binary is for counting only.

| Operation | Saving per call | Prefill calls | Decode calls | Prefill estimate | Decode estimate |
| --- | ---: | ---: | ---: | ---: | ---: |
| Tensor span | 2.54 ns | 58,688 | 4,045,076 | 0.149 ms | 10.274 ms |
| Graph node description | 8.51 ns | 22,229 | 1,511,538 | 0.189 ms | 12.863 ms |
| Check contiguity | 0.88 ns | 17,796 | 1,268,813 | 0.016 ms | 1.117 ms |
| Reuse destination bytes | 7.18 ns | 17,796 | 1,268,813 | 0.128 ms | 9.110 ms |
| Reuse operand bytes in alias checks | 7.91 ns/node | 28,921 operands | 2,108,386 operands | 0.057–0.141 ms | 4.169–10.036 ms |

Each estimate multiplies a measured call count by a measured microbenchmark
difference. Some paths overlap. In particular, the graph description already
includes tensor-span computation. The operand microbenchmark measures a whole
node. The number of touched nodes is bounded below by the operand count divided
by four, rounded up, and above by the recorded overwrite-eligible node count.
The range applies the per-node microbenchmark saving to those bounds. Use the
non-instrumented full-model measurements to establish request throughput.

The non-instrumented EXP-2214 candidate measured 292.50 prefill and 12.52
decode tok/s, versus 294.48 and 12.63 for v0.15.0. Output matched. The
microbenchmark gains have not yet translated into a throughput increase on
this workload.

## Additional microbenchmark savings

| Operation | Saving per node | Prefill calls | Decode calls |
| --- | ---: | ---: | ---: |
| Batch graph node traversal (EXP-2215) | 0.89 ns | Pending | Pending |

## Remaining C code

| Linux x86-64 build | C inputs | C source lines | Retained GGML text sections | Retained GGML text bytes |
| --- | ---: | ---: | ---: | ---: |
| v0.15.0 | 3 | 13,871 | 182 | 63,712 |
| EXP-2214 | 3 | 13,871 | 182 | 63,712 |
| EXP-2215 | 3 | 13,871 | 180 | 63,581 |
| EXP-2216 | 3 | 13,871 | 179 | 63,497 |

The C inputs are `ggml.c`, `ggml-quants.c` and `ggml-system.c`. Source
lines include all three files. Retained text counts non-empty input sections
from their objects in the GNU linker map after section garbage collection.
Both map-producing relinks matched the corresponding executable byte for
byte. Both measurements exclude headers and other C adapters. A section
count is not a function count. EXP-2215 removed 131 bytes by eliminating all linked calls to the two GGML
graph node accessors. The arithmetic microbenchmark savings above are kept
separate from this reduction in linked code.

EXP-2216 removed 84 more bytes by replacing scalar FP16 expansion with
Goose. The scalar microbenchmark improved from 2.43–2.51 to 1.85–1.86
ns/value. The row conversion remains linked.
