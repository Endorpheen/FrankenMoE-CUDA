# EXP-2026-09-16-070 — Model-free prefill expert-transfer path: gather removal vs dedicated copy stream

Status: `MODEL-FREE DONE` — Arm B passes the model-free promotion gate by a wide margin; Arm C is a null.
Kind: `kind=prototype-offline` (no model, no server; no server throughput claim is made)
Committed: no (model-free measurements only; this is not an accepted runtime change)

## 1. Question

The EXP-2026-09-07-043 profile of the accepted runtime named the CPU gather (1463.6 ms union,
56.9 % of the measured 395-token prefill window) as the dominant term of the pinned-ring path.
The prefill path copies each selected expert range out of the pageable GGUF mapping into a
16 MiB pinned slot on the CPU, then issues `cudaMemcpyAsync` from the slot to the device
(ring depth 3, one stream). This experiment asks, model-free and with no server:

- Arm A (current): two-thread CPU gather into 3 pinned 16 MiB slots + H2D, one stream.
- Arm B: pin the source ranges once (`cuMemHostRegister`) and copy directly source -> device,
  with no CPU gather and no staging slot.
- Arm C: keep the current gather+slot scheme but issue the device copies on a dedicated copy
  stream, with the compute stream waiting on the copy event before the consumer.

Metrics: the full gather + H2D sequence with a final drain (the EXP-045 primary metric), the
achieved H2D bandwidth, byte parity, and `VmSwap`.

## 2. Environment

| Item | Value |
| --- | --- |
| GPU | NVIDIA GeForce RTX 4070 12 GiB, driver 580.178.04, idle during the runs |
| PCIe | gen 4 x 16 (negotiated) |
| CPU | AMD Ryzen 9 5950X, 32 threads |
| RAM | 63 643 556 kB total MemTotal |
| `RLIMIT_MEMLOCK` | 8192 KiB; `CapEff` has no `CAP_IPC_LOCK` |
| glibc | 2.43 |
| Host compiler | g++-13 13.4.0 |

Toolchain note: the local CUDA 12.6 `nvcc` cannot compile any `.cu` file against glibc 2.43
(`rsqrt`/`cospi`/`sinpi` exception-specification clash), with either g++-12/13 or clang. The
harness therefore targets the CUDA Driver API (`cuda.h`) and is built with a plain host
compiler; no device code is needed for a memcpy-only workload.

## 3. Workload

The real EXP-045 range list is reused verbatim:
`exp070_ranges.txt` (sha256 `8b58328ed61b0bfea88021e7f0a895233a9a56f078fc65a9ababe4da6b3f6f79`,
identical to the EXP-045 file): 17 886 ranges, 23 237 033 984 bytes (21.6412 GiB), min 524 800 B,
max 11 981 312 B, spanning GGUF shards 2 and 3. Shards are mapped `MAP_PRIVATE PROT_READ`; the
destination is a 64 MiB wrapping device arena with the same cursor logic as EXP-045.

`MAP_SHARED` file mappings cannot be registered with the driver
(`CUDA_ERROR_INVALID_VALUE`), so `MAP_PRIVATE PROT_READ` + `CU_MEMHOSTREGISTER_READ_ONLY` is used
for Arm B; all arms read the same pages.

## 4. Arm implementation

- Arm A: the EXP-045 ring loop (`chunk = min(remaining, 16 MiB)`, round-robin slots,
  `cuEventSynchronize` before slot reuse, two-thread gather, `cuMemcpyHtoDAsync` + event record)
  with `GGML_CUDA_EXPERT_RING_SLOTS = 3` (the delivered EXP-062 depth). The gather helper is one
  persistent thread created outside the timed passes.
- Arm B: setup registers every page-aligned interval covering the selected ranges once; the timed
  loop issues one `cuMemcpyHtoDAsync` per range directly from the registered source pointer.
- Arm C: identical to A, but the copies and their per-slot events are on a second `copy_stream`,
  and after each copy the compute stream executes `cuStreamWaitEvent` on the copy event. With no
  consumer kernel in the model-free harness this only models the dependency edge; C therefore
  isolates stream separation without any compute overlap.

## 5. Correctness (gate before the measurements)

- `--selftest` (5/5 PASS): per-chunk `memcmp` of the staged slot against the source on A and C,
  identical chunk hashes A == B == C, source unchanged, device round-trip equal to the source.
  Cases: 1, 63, 64, 65, 127, 524800, 524801, 16 MiB-1, 16 MiB, 16 MiB+1, 16 MiB+65, 17 MiB,
  33 MiB, unaligned offsets 0/1/3, three repetitions (slot reuse).
- Full real-list parity: `--hash` over all 17 886 chunks for A, B and C produced identical
  hash lists, sha256 `5bbf98f3f18049105f5fe7e758a6bda033c67d2fc1accf3bf5b7b528e90dcde8`
  (17 886 hashes + header), which is exactly the EXP-045 verified hash-list sha256.
- `VmSwap = 0` in every measured run.

## 6. Pin feasibility for Arm B

`cuMemHostRegister` of the full source succeeds despite the 8 MiB memlock and no `CAP_IPC_LOCK`:
17 886 intervals, 23 311 929 344 bytes (21.7109 GiB, 71.4 MiB page-alignment overhead),
0 failures, `complete = true` (`pinprobe-full.txt`). Cold registration cost is about 36 s (one-time
page-in plus pinning). No smaller-window fallback was needed.

## 7. Raw results

All arms: 17 886 calls/chunks, 23 237 033 984 bytes, `VmSwap = 0`. Rounds r1–r4 ran A,B,C;
r5–r8 ran C,B,A (reversed) to cancel ordering effects. Each invocation: warmup 1 + 1 measured pass.

| run | wall ms | gather ms | slotwait ms | span ms | bw GiB/s | run | wall ms | gather ms | bw GiB/s |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: | ---: |
| A-r1 | 1948.0 | 1796.5 | 104.1 | 1947.9 | 11.11 | B-r1 | 1015.5 | 0.0 | 21.31 |
| A-r2 | 2008.0 | 1834.0 | 125.7 | 2007.9 | 10.78 | B-r2 | 1032.5 | 0.0 | 20.96 |
| A-r3 | 1872.3 | 1715.3 | 108.3 | 1872.3 | 11.56 | B-r3 | 986.3 | 0.0 | 21.94 |
| A-r4 | 1893.0 | 1739.9 | 106.6 | 1893.0 | 11.43 | B-r4 | 992.4 | 0.0 | 21.81 |
| A-r5 | 1953.2 | 1787.1 | 117.8 | 1953.1 | 11.08 | B-r5 | 1000.7 | 0.0 | 21.63 |
| A-r6 | 1898.4 | 1726.9 | 124.3 | 1898.3 | 11.40 | B-r6 | 1001.8 | 0.0 | 21.60 |
| A-r7 | 1732.0 | 1576.2 | 107.5 | 1732.0 | 12.49 | B-r7 | 1000.6 | 0.0 | 21.63 |
| A-r8 | 1991.3 | 1838.5 | 104.5 | 1991.3 | 10.87 | B-r8 | 993.7 | 0.0 | 21.78 |
| C-r1 | 1665.4 | 1488.2 | 125.8 | 1665.4 | 12.99 | | | | |
| C-r2 | 1674.3 | 1504.9 | 117.5 | 1674.3 | 12.93 | | | | |
| C-r3 | 1871.1 | 1711.7 | 108.2 | 1871.0 | 11.57 | | | | |
| C-r4 | 1912.7 | 1753.7 | 108.2 | 1912.6 | 11.31 | | | | |
| C-r5 | 2012.7 | 1860.9 | 102.0 | 2012.6 | 10.75 | | | | |
| C-r6 | 2020.5 | 1867.0 | 100.0 | 2020.5 | 10.71 | | | | |
| C-r7 | 1859.7 | 1700.8 | 107.4 | 1859.7 | 11.64 | | | | |
| C-r8 | 1919.6 | 1764.8 | 103.9 | 1919.6 | 11.27 | | | | |

Depth control (Arm A with 2 slots, matching the task wording): 2044.1, 1959.0, 1997.2, 1984.5 ms
(median 1990.8 ms) — the slot count is not the limiter; the gather is.

## 8. Medians, deltas and bandwidth

| Arm | n | wall median ms | wall min..max | effective bw median GiB/s | gather median ms | slotwait median ms |
| --- | ---: | ---: | --- | ---: | ---: | ---: |
| A (3 slots) | 8 | 1923.2 | 1732.0 .. 2008.0 | 11.25 | 1763.5 | 107.9 |
| B (pinned source) | 8 | 1000.7 | 986.3 .. 1032.5 | 21.63 | 0.0 | 0.0 |
| C (copy stream) | 8 | 1891.9 | 1665.4 .. 2020.5 | 11.44 | 1732.7 | 107.8 |
| A2 (2 slots) | 4 | 1990.8 | 1959.0 .. 2044.1 | 10.87 | 1577.5 | 361.6 |

- B vs A: median paired delta **-47.72 %**, B wins **8/8** rounds (per-round
  -47.9, -48.6, -47.3, -47.6, -48.8, -47.2, -42.2, -50.1 %).
- C vs A: median paired delta **+0.49 %**, C wins **4/8**; the per-round deltas change sign with
  run order (-14.5, -16.6, -0.1, +1.0, +3.0, +6.4, +7.4, -3.6 %) — the apparent C advantage is a
  run-order artifact, not an effect of the copy stream.
- Effective bandwidth (bytes / full-path wall): A 11.25 GiB/s (12.08 GB/s), C 11.44 GiB/s
  (12.28 GB/s), B 21.63 GiB/s (23.23 GB/s) — the pure H2D ceiling of this link, about 74 % of the
  31.5 GB/s PCIe gen4 x16 theoretical. Arm C does not raise the achieved H2D bandwidth.
- Spread: B 986.3–1032.5 ms (4.6 % of median); A 1732.0–2008.0 ms (14.4 %). Removing the CPU
  gather also removes most of the path's sensitivity to CPU contention.

## 9. Verdict

- **Arm B — PASS the model-free promotion gate.** Removing the gather cuts the full
  gather+H2D+drain by 47.7 % (median 1923.2 -> 1000.7 ms), far above the >= 5 % threshold and with
  8/8 wins, byte parity and `VmSwap = 0`. The direction is worth a model-level integration
  experiment.
- **Arm C — REJECT.** A dedicated copy stream with no compute overlap is statistically
  indistinguishable from the current path (+0.49 % paired median, 4/8 wins) and does not raise the
  achieved H2D bandwidth. Stream separation alone is not a lever.

## 10. Integration caveats (not measured here)

1. Arm B pins the whole source working set: 21.7109 GiB of non-pageable RAM for this single
   395-token prompt. On the 64 GiB host this fits, but pinning is not reclaimable and interacts
   with swap safety (`VmSwap` must stay 0).
2. In production the selected expert ranges change per prompt and per token; pinning on first
   touch means the pinned set grows over a session. Over many distinct prompts the union can
   approach the whole 82 GB model, which cannot be pinned on this host. A bounded pinned window
   (direct-copy the ranges inside the window, gather the rest) is the obvious mitigation but was
   not measured here.
3. `cuMemHostUnregister` / process exit releases the pins; nothing in this harness changes the
   accepted runtime or its builds.
4. The model-free numbers are a transfer-path result only; they are not a server throughput claim.

## 11. Files and commands

Artifacts in `results/archive/EXP-2026-09-16-070/`:

- `exp070_transfer3.cpp` — sha256 `0d3bee424f0d561eb4d07c5c2aff56152eaa020e5cd837fa4dc2bd18681d8ba7`
- `exp070_pinprobe.cpp` — sha256 `d1221c1021af9cf36ae9c7371e6bc490836668d5675a4405c39a93ede9cebefb`
- `exp070_ranges.txt` — sha256 `8b58328ed61b0bfea88021e7f0a895233a9a56f078fc65a9ababe4da6b3f6f79`
- `gen_exp070_ranges.py` — sha256 `864b7d74e9d338f7a17baae8436d2a85239ee53b962b98c74b551ceda3326c39`
- `build.sh` — sha256 `19853de0ad0e0611b353101e5afd27edd53c5fe64eeb9835391f9b86d30cf5e3`
- `run_arm.sh` — sha256 `fc5921c66a17d67e6bb0f749c414185762ba97fe470246a3fd94823c1c98ba71`
- `selftest.txt`, `pinprobe-full.txt`, `environment.txt`, `summary.json`
- `run-{A,B,C}-r1..r8.json`, `run-A2s-r1..r4.json` — raw per-run telemetry
- `rev-hash-{A,B,C}.txt` — full-list hash verification (identical to EXP-045)

Built binaries (in gitignored `build/exp070-transfer3/`): `exp070_transfer3` sha256
`c59c359a6b901c8dc074a073021440790b521a7ce7fb1651af7d80df3b9b6fe9`, `exp070_pinprobe` sha256
`2dd3cc97a219b3eaa9e13cee8b2ce376ee2899c3ecdb5b087ed7d58935425d96`.

Commands:

```bash
# build (one build directory, own tree; accepted builds untouched)
results/archive/EXP-2026-09-16-070/build.sh

# correctness
build/exp070-transfer3/exp070_transfer3 --selftest
build/exp070-transfer3/exp070_pinprobe --ranges .../exp070_ranges.txt --shard <3 shards>

# one measured run of one arm, strictly sequential (warmup 1 + 1 measured pass)
./run_arm.sh A A-r1 --passes 1
./run_arm.sh B B-r1 --passes 1
./run_arm.sh C C-r1 --passes 1

# full-list byte parity
build/exp070-transfer3/exp070_transfer3 --ranges .../exp070_ranges.txt --shard <3 shards> \
    --mode A --slots 3 --warmup 0 --passes 1 --hash rev-hash-A.txt   # and B, C
```

No repository tree, patch, accepted build or `work/llama.cpp-integration` was modified; nothing was
committed.
