# EXP-2026-11-24-1885: Schedule shared FFN before the route read

**Rejected for baseline promotion.** EXP-1880 remains the accepted baseline.
One sequential A/B pair was run. The isolated positive decode delta has no
reverse-order confirmation and did not compensate for the measured prefill loss.

## Mechanism and result

In `fg_cuda_routed_graph` (`cuda-driver.c`), the fused shared up/gate/SwiGLU
kernel needs normalized input and dense weights. The eight selected expert IDs
are irrelevant to this computation. Append one direct Q8_1 pack and the
existing fused shared kernel after the router publishes its mailbox sequence,
within the preceding buffered region. The later routed FFN can reuse the same
packed activation range in stream order. The existing stage and wait for
expert-bank copies stay in place. The original path is used until the scratch
allocation and mailbox are available. The preliminary Nsight timing budget
was 81.923 ms of shared up/gate/SwiGLU kernel time across 20,440 decode calls
(EXP-1870 control); overlap can recover only a fraction and the extra pack
also has a cost.

RTX 4070, Apex 32K/512, 262144 context, 6290 MiB SLRU, 4096/4096 batch,
Q4_0 KV. One fresh server, one warmup and one measured request per arm.
Control and candidate use the same test binary and frozen CUDA images.
The only A/B switch is `GOOSE_EARLY_SHARED_FFN=0/1`.

| Arm | Prefill tok/s | Decode tok/s | Decode ms | Request s | Peak process VRAM MiB | Peak RSS KiB |
| --- | ---:| ---:| ---:| ---:| ---:| ---:|
| A | 3506.111 | 83.609 | 6111.749 | 15.5451 | 8774 | 21491504 |
| B | 3478.879 | 83.824 | 6096.138 | 15.6027 | 8774 | 21492444 |

Decode +0.26% (15.611 ms); prefill -0.78%; client time +57.6 ms.
Both requests in each arm matched all 512 reference token IDs and the exact
text. `/props` was byte-identical, and no process swap or system swap-out
occurred. The candidate logged at least 30,000 early submissions across both
requests. Start GPU temperatures differed (54 °C and 46 °C). This experiment
establishes feasibility; it does not establish a positive full-workload
effect from early shared scheduling.

## Reproduction

Use EXP-1880's accepted driver, headers, `lib.a`, argv and environment and
the [same workload](../EXP-2026-11-24-1880-fwht-fixed/README.md). In an isolated
build directory, apply `patch -o cuda-driver.c "$CONTROL_DRIVER" driver.patch`.
Use the EXP-1880 GCC 13 host-adapter build and link commands with the unchanged
image headers and `lib.a`; no Goose or device-image recompilation is needed.
Start the resulting executable once per arm with the recorded argv and
environment, adding the switch above. Send two requests serially and compare
the second with the exact reference. The [machine-readable receipt](results.json)
records artifact hashes, memory, token hashes and timing for both arms.

The ordering can be revisited if a related CUDA change eliminates the extra
pack or exposes a longer independently schedulable chain. Keep the original
expert-selection and stream-ordering contracts.
