# EXP-2026-11-25-1917: Measure the Inkling 2048/256 prefill gap

## Baseline

The two local binaries completed the same frozen 2048 input IDs, 256
output tokens, sampling options, context 4096, batch 2048, ubatch 512,
and Q4_0 K/V cache on RTX 4070 using Inkling-2-Mini AD-IQ4_XS. Both
engines recomputed the full prompt. Each server handled its first
request and one repeat. The system file cache was warm. Decode uses 255
inter-token intervals. Device placement and cache implementation are
engine-specific.

| Engine | First prefill | Repeat prefill | First decode | Repeat decode | Board peak | Process peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Goose | 165.29 | 174.79 | 13.893 | 13.929 | 9882 MiB | 12.62 GiB |
| llama.cpp | 280.84 | 281.97 | 13.943 | 13.992 | 10987 MiB | 14.31 GiB |

Goose repeat prefill took 11716.6 ms versus 7263.1 ms upstream,
a 4453.5 ms gap. Repeat requests completed in 30.240 and 25.488
seconds. Both pairs matched all 256 output IDs. Process swap and
system swap-out were zero. See [raw metrics](comparison.json).

Goose is the EXP-1916 experimental binary with SHA-256
`99b1b2b54a84a4a56e32805833ae2e98b014c5f2e0de65e1f7ea62c1e164b336`.
The llama.cpp revision is `4c92c5bfb7cab395af9fc0fe0939e34ae5083345`
with binary SHA-256
`ebcb6013d6c4fc3347758d03c6edf61e7c04f9d2971edb1326766ff16f4c3256`.
The normalized request SHA-256 is
`bd145888a4775eca2eb547b0145f1b5c0157bfdab8f5e3f6bbcba2cb948f38ed`.
[Launch arguments](argv.json) list explicit environment overrides and
all process arguments. The local artifacts are under
`build/inkling-exp-baseline/device-attention/inkling-local-parity2048`.

## Timing map

Independent Nsight Systems traces used the same launches. Windows end
at the first vocabulary readback of the repeat and extend backward by
the reported prefill duration. The two request boundaries are confirmed
by the large readback interval at the expected index, 256 for Goose
and 258 for upstream after two startup readbacks. The Goose trace
contains 253 of the 256 repeat readbacks, including the first. Both
HTTP requests completed. The trace analysis is restricted to prefill.

The elapsed-time classes below are disjoint. Rows show milliseconds.

| Activity | Goose | llama.cpp | Goose minus llama.cpp |
| --- | ---: | ---: | ---: |
| No recorded GPU activity | 3653.7 | 365.0 | +3288.7 |
| Kernels without copies | 4539.9 | 2080.6 | +2459.2 |
| More than one activity class | 654.8 | 0.0 | +654.8 |
| D2H only | 470.2 | 4.6 | +465.6 |
| D2D only | 1.4 | 10.0 | -8.6 |
| H2D only | 2482.3 | 4890.7 | -2408.5 |
| Total | 11802.3 | 7351.1 | +4451.2 |

1. Attribute the idle gaps first. Of the GPU-inactive intervals,
   Goose spends 3129.5 ms outside recorded CUDA API activity, versus
   263.1 ms upstream. Copy API calls alone, excluding concurrent
   GPU activity, take 483.5 versus 18.7 ms. Explicit sync alone takes
   13.7 versus 16.7 ms. These data do not attribute all the gaps to
   CPU work. Profile the Inkling host preparation, graph rebuilding,
   cache scheduling and worker waits before changing synchronization.
2. Remove the full activation round trips at the attention and MoE
   boundaries. 384 D2H copies of 4718592 bytes account for 469.9 ms
   of summed copy activity. The size matches 512 FP32 rows of width
   2304, and the count matches two copies per layer and microbatch
   across 48 visits and four microbatches. Trace correlation and host
   scopes should confirm each producer/consumer. The prefill driver
   in `inkling-prefill-team/prefill.h` contains host post-convolution,
   preparation and activation copies. Start with an isolated resident
   transition that preserves rolling state and intermediate state hashes.
3. Address Q8 and grouped MoE compute after measuring the resident
   path. The summed Goose durations are 2082.8 ms for
   `goose_i2_matrix_q8_0`, 1718.8 ms for
   `goose_i2_xs_mmq16` plus `goose_i2_nl_mmq16`, and 826.5 ms
   for `goose_i2_flash_mma`. These sums are part of the timeline
   above. Use per-shape evidence to separate reuse, resident
   scheduling, and changes to the kernel math.
4. Keep traffic and request timing separate. Goose and upstream trace
   H2D totals were 49.62 GB and 50.86 GB. Goose has less exposed H2D
   time in this pair. The 41.10 GB repeated expert-cache payload is
   a subset of Goose H2D bytes and still matters to Goose scaling.

See [the detailed timeline](paired-prefill.json). To regenerate it,
export the two CUDA reports to SQLite and run
`python3 compare-prefill-traces.py` with `profile/goose` and
`profile/upstream` alongside the analyzer, each with `inkling.sqlite`
and matching `cold.json` and `warm.json`. Keep the full trace files
in the local build directory.

## Diagnostic larger microbatch

One additional Goose launch changed only ubatch 512 to 1024. First
prefill was 182.83 tok/s and repeat prefill was 192.28 tok/s. Repeat
prefill saved 1065.4 ms and moved 20.24 GB of expert weights instead
of 41.10 GB. First/repeat decode was 13.981/13.793 tok/s. The repeat
decode took 180.4 ms longer. Peak board memory rose 36 MiB and peak RSS
rose 101.4 MiB. Swap and swap-out stayed at zero. All 256 IDs and text
matched the Goose control in both requests. This single comparison is
diagnostic. Keep ubatch 512 as the baseline. See
[the microbatch probe](ubatch1024-probe.json).

The frozen Apex checkpoint checksum manifest passes. This experiment
changes only measurements and analysis. A combined-runtime Apex
regression gate and the Inkling 32K/512 targets remain open.
