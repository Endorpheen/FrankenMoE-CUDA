# EXP-2026-11-24-1859 — Split interior and boundary causal Flash tiles

**INCONCLUSIVE for Apex throughput.** The full-tile candidate passed the
standalone correctness and timing probe. The i32 candidate is rejected
at the kernel-probe stage. The runtime checkpoint stays at EXP-1856.

## Mechanism

The Apex `process_tile_apex_nc8_np1` loop applies a lane-local causal
predicate to every score. When the last key in the 32-row tile is visible
for both query halves, all these predicates select positive zero.
The full-tile branch uses the same FP32 FMA with positive zero and leaves
all boundary predicates, reductions and accumulation order intact. The i32
variant instead compares local 32-bit key offsets against the visibility
counts relative to the tile base. Neither changes the query or KV layout.

The selected SM89 entry keeps 255 registers and a single block barrier.
The full-tile variant reduces the entry stack frame from 264 to 120 bytes.
Static spill bytes are reported per function by ptxas and are separate
from the timings below.

## Evidence

The previous [full-window trace](../EXP-2026-11-24-1858-prefill-consumer-fence/README.md)
measured 1711.6 ms in this entry across 80 launches. The last 10
launches accounted for 402.7 ms. The standalone RTX 4070 probe launches
the exact entry at q=4096, kv=32768, prefix=28673 and alternates the arm
order. Timings are CUDA events, six blocks of three launches per variant.

| Variant | Median ms/launch | Change vs control | FP32 output word differences |
| --- | ---: | ---: | ---: |
| Archived PTX control | 41.530 | — | — |
| i32 tile-relative predicates | 45.778 | +10.23% time | 0 |
| Full visible tile branch | 31.491 | -24.17% time | 0 |

All FP32 output buffers matched byte for byte in five deterministic
fixtures covering early, tile-boundary, partial-query and late shapes.
See [probe.py](probe.py) and [probe.log](probe.log). The late-shape saving
of 10.04 ms per launch suggests roughly 100 ms in the last group if it
transfers unchanged. Earlier groups have different visibility ratios.
The probe does not measure model output, peak RAM/VRAM or throughput.

## Reproduce

Use the EXP-1786 frozen device tree and its archived MMQ PTX for the
control. Recover the historical `flash-mma.goose` by matching the source
hash in [SHA256SUMS](SHA256SUMS) and checking the two generated header
hashes before patching. The exact recovered sources and binaries from this
run are in the local EXP-1859 artifact directory. For each variant, use
a separate copy of the device tree. From each variant directory:

```sh
patch flash-mma.goose < "$card/source/full.patch"
"$keelwing/bin/goose" --standalone --emit-provenance flash-mma.c flash-mma.goose
python3 "$exp1786/tools/frankenmoe-fragment.py" "$PWD" flash-mma
cp flash-mma-device.h flash-mma-types.h device/
/usr/bin/clang-18 -target nvptx64-nvidia-cuda -nocudalib -O2 -x cl -cl-std=CL1.2 \
  -include device/device-prelude.cl -I device -Xclang -target-cpu -Xclang sm_80 \
  -DGOOSE_CUDA_MMQ_IMAGE -S device/glue/cuda-device.cl -o flash.ptx
/usr/local/cuda-12.6/bin/ptxas -arch=sm_89 -v --entry goose_flash_causal_mma flash.ptx -o full.cubin
/usr/local/cuda-12.6/bin/ptxas -arch=sm_89 -v --entry goose_flash_causal_mma \
  "$exp1786/device/goose-cuda-mmq.ptx" -o control.cubin
```

KeelWing is the frozen `fried-goose-v0.3.0-preview-keelwing` build. Use
`i32.patch` and output `i32.cubin` for the second variant. Place all three
cubin files next to `probe.py`, then run `python3 probe.py` with NumPy
installed and an idle RTX 4070.

For end-to-end A/B, apply [flash-overlay.patch](source/flash-overlay.patch)
to the EXP-1856 checkpoint driver and link its original archive with
only the driver object replaced. Use the same executable for both arms.
Set `GOOSE_EXPERIMENT_FLASH_OVERLAY` to the control or full cubin and
keep decode split 4/count 1, mailbox and the EXP-1856 fence defaults.
Use Apex 32K/512, context 262144, batch and ubatch 4096, Q4_0 KV and
expert bank 6290 MiB. One warm-up and one scored request per arm in
A-B-B-A order. Compare against the accepted checkpoint for all 512 IDs,
text, SLRU counters, prefill, decode, end-to-end time, VRAM, RAM and swap.
At the time of the probe only 9136 MiB was free before a model launch,
below the 9700 MiB preflight threshold. The model A/B was not started.
