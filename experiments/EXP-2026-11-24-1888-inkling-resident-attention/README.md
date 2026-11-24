# EXP-2026-11-24-1888 — Keep Inkling attention KV on the device

**Diagnostic, no production promotion.** The standalone, architecture-specific
Attention module runs within the existing Goose CUDA context. A cache slot's
first call uploads its K/V, position array and relative bias table.
Subsequent calls publish one new K/V row and its position. The scores and V
accumulation run on device. Query preparation, history, gating and the final
attention vector remain on the host in this experiment. F32 K/V arrays
remain mirrored in RAM. Context reset, rollback, eviction, quantized KV
and multi-query prefill are outside this fixture.

RTX 4070, Inkling-2-Mini-AD-IQ4_XS, context 128, three prompt IDs and
17 greedy output IDs. The test has 912 Attention calls over 19 evaluated
positions. One serial request per process, order A-B-B-A. Same binary,
two-plane SLRU and quant math. Only `I2_DEVICE_ATTN=0/1` differs.

| Diagnostic | A1 CPU | B1 CUDA | B2 CUDA | A2 CPU |
| --- | ---: | ---: | ---: | ---: |
| Decode, tok/s | 3.110 | 3.188 | 3.186 | 3.133 |
| Attention accumulated ms | 176.344 | 100.075 | 100.422 | 175.582 |
| Prefill seconds (3 tokens) | 1.133 | 1.125 | 1.147 | 1.135 |

Mean decode 3.122 → 3.187 (+2.1%), Attention 176.0 → 100.2 ms.
All 17 IDs and expert cache counters match. Residual and full-logit
fingerprints differ between the CPU and CUDA models. In a separate shadow
run, CPU attention received exactly the CUDA run's inputs. Across
2,801,664 values, 805,759 differed bitwise, with maximum absolute error
1.90735e-6 and mean 7.47360e-9. This does not establish bitwise
equivalence or general sampling equivalence. The shadow time is excluded.

Each CUDA arm transferred 73,306,368 bytes H2D and 11,206,656 bytes D2H
through the Attention adapter, including initialization. Its live slot
and working buffers total 53,589,696 bytes. The process-level CUDA
allocation delta grew about 93 MiB. Peak RSS was about 6.41 GiB and
there was no swap-out. See [raw results](results.json).

## Reproduce

The source set is included for inspection. The timed prototype links
against the frozen Inkling diagnostic archive. It cannot be reproduced
by linking a generic release archive from the repository. Verify the
control CUDA driver against `driver-control.o` in the receipt before
replacing its member. Start from `db08ad6c2` and apply
`source/cuda-driver-reference.patch` to its standalone CUDA driver.
KeelWing standalone provenance, the EXP-1786 fragment adapter,
Clang 18 SM80 NVPTX, CUDA 12.6 libdevice and SM89 ptxas produce
the four-entry device module in a cubin. Use `-O3 -ffp-contract=off`
for device and `-O3 -march=native -fno-fast-math -ffp-contract=off`
for the separate numeric object. Compile the bridge with GCC 13
`-O1`. The adapter includes the pinned driver and the cubin resource.
See `probe.c` for the harness and `shadow-probe.c` for the shadow checks.

Set `I2_DIAGNOSTIC_EXPERT_BANK=1 I2_CHECK_STATE=1 I2_TEST_DECODE_STEPS=16`
and send bare input IDs using the local GGUF file. For the separate
shadow binary set `I2_VERIFY_DEVICE_TENSOR=1`. The production 32K/512
comparison requires a bounded 40-ring/eight-global cache planner,
multi-query prefill, fewer projection boundaries, and a correctness
gate for model sampling. Apex EXP-1880 is the independent control.
