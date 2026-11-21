# Apex 32K/512 latency map

## Current snapshot

EXP-1618 compares frozen [Optimized Hybrid](OPTIMIZED-HYBRID.md)
against independent Goose EXP-1617 (`efa4c82c`) with its cumulative
experimental graph, including the 18-kernel FFN. Both use RTX 4070,
the same Apex Q4_K_M weights and request, context 262,144, Q4_0 K/V,
4096/4096, CPU limits 8/16 with masks `0f0f` and `ffff`, and
a 5,632 MiB expert-cache budget. Each request evaluates 32,768
prompt IDs, reuses zero prompt tokens and returns 512 IDs.
The prompt and sampling controls are equal. The two runtimes
generate different continuations, so their expert routes can differ.
Each runtime's CUDA and CPU passes return the same complete
response and probability rows within that runtime.

| Profiled request | Optimized Hybrid | Goose EXP-1617 |
|---|---:|---:|
| CUDA-pass prefill, ms | 12,524.921 | 28,925.247 |
| CUDA-pass decode, ms | 8,639.381 | 17,574.224 |
| CUDA-pass prefill / decode, tok/s | 2616.22 / 59.15 | 1132.85 / 29.08 |
| CPU-pass prefill / decode, tok/s | 2665.32 / 64.35 | 1123.09 / 31.95 |
| CUDA-pass peak process VRAM, MiB | 8870 | 7966 |
| CPU-pass peak process VRAM, MiB | 8842 | 7936 |
| CUDA-pass peak RSS, KiB | 22517792 | 22076156 |
| CPU-pass peak RSS, KiB | 21876644 | 21490272 |
| Process swap and request system swap-out growth | 0 | 0 |

## Attribution and refresh

The CUDA pass uses NVIDIA Nsight Systems CLI 2026.6.1 with
node-level graph tracing. The full hybrid trace and the stopped
Goose trace include all 511 decode evaluations. A combined
collector run failed, and two Goose traces kept only 496 full
decode steps. They are excluded. Stop the named Nsight session
while the server is still alive to capture the end of the request.
The CPU passes use separate GNU gprofng clock sampling at 2 ms
with signals bracketing the request. CPU phase splits are
estimated from the request-start signal and each CPU pass's
reported prefill duration. These are sampled CPU milliseconds
across threads, separate from CUDA API wall time.
[Nsight documentation](https://docs.nvidia.com/nsight-systems/UserGuide/)
[GNU gprofng documentation](https://sourceware.org/binutils/docs/gprofng.html)

Device windows begin at the first 4,096-row RMSNorm and split at
the first one-token GDN execution after prefill. The hybrid request
trace has 90 earlier warm-up and tail-prefill GDN markers. The
window ends at the final output D2H or kernel, whichever is later.
Some host work before and after these landmarks sits outside the
device windows. Server timing rows keep that distinction visible.

GPU sum counts every clipped kernel or copy interval in its row.
Exclusive GPU time measures the interval during which only that
family is active. The busy union counts shared regions once. The
idle remainder is time without recorded GPU activity. The
exclusive differences locate measured exposure on the device
timeline. A speedup of any particular size still requires an
ablation because overlap and scheduling can change. CUDA API
overlap with GPU idle time identifies work to investigate, with
no causal attribution to the API itself. Graph-node rows are
subsets of kernel activity and never added to the GPU total.

Recompute the report after a change in the active graph or the
hybrid reference. Use the same settings, retain raw traces and
sampling data, record exact executable and request hashes and
qualify responses. Keep the previous snapshot and the unprofiled
acceptance measurements. Use the map to choose changes after
the Goose graph reaches the hybrid's next-Top-K boundary.

The [reproducible analyzer](../../tools/apex-latency-map.py)
checks the request bytes and step counts and produces JSON with
per-kernel and per-API rows. EXP-1618 evidence is under
`/home/end0/AI/LlamaServer/nsight-profile-exp1618.sNdw9P`.
It contains the reports, SQLite exports, CPU experiments, exact
commands, comparison JSON, and detailed tables. Accepted CUDA
report SHA-256 values are
`2a9ec4183759817ba32dacc52fa595f5cd1065464ed1793c26b639cc2585dac6`
for Optimized Hybrid and
`ec496f90713eb093840a9da008625c12c3dcdfcb485bcdc99b2ec210c8298ad2`
for Goose.

## Disjoint trace windows

| Phase / interval | Hybrid, ms | Goose, ms | Goose minus Hybrid, ms |
|---|---:|---:|---:|
| Prefill device window | 12,146.247 | 28,907.250 | +16,761.003 |
| Prefill busy GPU union | 11,036.994 | 25,407.219 | +14,370.226 |
| Prefill no recorded GPU activity | 1,109.254 | 3,500.030 | +2,390.777 |
| Prefill server time outside the device window | 378.674 | 17.997 | -360.676 |
| Decode device window | 8,632.618 | 17,571.051 | +8,938.433 |
| Decode busy GPU union | 6,610.551 | 10,113.189 | +3,502.637 |
| Decode no recorded GPU activity | 2,022.067 | 7,457.862 | +5,435.795 |
| Decode server time outside the device window | 6.763 | 3.173 | -3.590 |

## Ranked measured excess
Positive excess is ranked by exclusive GPU time or the idle
remainder. The ranking is a priority for investigation. It
does not predict the saving from changing any one operation.

| Phase | Family or gap | Hybrid exclusive / idle, ms | Goose exclusive / idle, ms | Excess, ms |
|---|---|---:|---:|---:|
| Prefill | MMQ | 3,142.227 | 11,638.307 | +8,496.080 |
| Decode | No recorded GPU work | 2,022.067 | 7,457.862 | +5,435.795 |
| Prefill | No recorded GPU work | 1,109.254 | 3,500.030 | +2,390.777 |
| Decode | Vector Attention | 1,221.339 | 2,838.630 | +1,617.290 |
| Prefill | Flash Attention MMA | 1,422.438 | 2,976.752 | +1,554.315 |
| Prefill | Pointwise and GLU | 491.680 | 1,648.458 | +1,156.778 |
| Prefill | GDN | 470.970 | 1,360.016 | +889.046 |
| Decode | Dense and generic matmul | 148.121 | 798.974 | +650.854 |
| Decode | Pointwise and GLU | 139.757 | 579.186 | +439.429 |
| Prefill | Dense and generic matmul | 86.341 | 499.674 | +413.333 |
| Decode | MMVQ | 2,485.502 | 2,815.432 | +329.930 |
| Prefill | H2D bulk | 3,497.644 | 3,775.733 | +278.089 |
| Decode | MoE router | 48.592 | 199.804 | +151.212 |
| Decode | RMSNorm | 124.993 | 273.368 | +148.375 |
| Prefill | Tensor gather and blit | 375.620 | 493.316 | +117.695 |
| Decode | H2D packets up to 4 KiB | 7.468 | 114.832 | +107.363 |
| Prefill | RMSNorm | 242.274 | 345.781 | +103.507 |
| Prefill | KV FWHT | 47.670 | 150.539 | +102.868 |
| Decode | GDN | 57.224 | 116.157 | +58.932 |
| Prefill | Other Attention | 0.000 | 58.055 | +58.055 |
| Decode | MMVQ pack | 184.144 | 233.103 | +48.959 |
| Prefill | D2D | 44.153 | 90.345 | +46.191 |
| Decode | KV FWHT | 18.108 | 54.504 | +36.396 |
| Decode | SSM convolution | 21.127 | 51.244 | +30.117 |
| Prefill | RoPE | 12.097 | 41.509 | +29.412 |
| Decode | KV writes | 14.544 | 34.848 | +20.304 |
| Decode | MoE weighted reduction | 29.555 | 46.858 | +17.302 |
| Decode | RoPE | 10.798 | 27.254 | +16.456 |
| Prefill | MoE router | 8.014 | 17.094 | +9.080 |
| Prefill | KV writes | 5.593 | 11.768 | +6.175 |
| Prefill | MoE weighted reduction | 242.257 | 243.543 | +1.286 |
| Prefill | Memset | 0.233 | 0.284 | +0.050 |

Goose overlaps about 1.475 s of additional prefill MMQ
with other GPU activity. The family sum grows by 9.971 s
while exclusive time grows by 8.496 s. Bulk H2D grows
by 1.753 s in the sum and by 0.278 s in exclusive time.
Use the disjoint window before attributing the full gap.

The decode CUDA API union overlaps 5.576 s of the 7.458 s
Goose idle remainder. Synchronous CUDA copy calls alone
overlap 3.665 s of that idle time. The analogous CUDA API
union overlaps 1.168 s of the 2.022 s hybrid remainder.
These observations include waits. A larger graph can remove
some handoffs. The map does not assign the entire idle delta
to graph replay or to the copy engine.

## GPU families and transfers

### Prefill

| Family | Hybrid count | Goose count | Hybrid sum, ms | Goose sum, ms | Δ sum, ms | Δ exclusive, ms |
|---|---:|---:|---:|---:|---:|---:|
| MMQ | 3,392 | 5,410 | 3,142.227 | 13,112.908 | +9,970.682 | +8,496.080 |
| Flash Attention MMA | 440 | 240 | 1,422.438 | 2,976.752 | +1,554.315 | +1,554.315 |
| Pointwise and GLU | 3,614 | 4,867 | 491.680 | 1,648.458 | +1,156.778 | +1,156.778 |
| GDN | 270 | 270 | 470.970 | 1,360.016 | +889.046 | +889.046 |
| Dense and generic matmul | 743 | 720 | 86.341 | 499.674 | +413.333 | +413.333 |
| H2D bulk | 58,075 | 158,212 | 3,497.644 | 5,250.378 | +1,752.734 | +278.089 |
| Tensor gather and blit | 1,216 | 632 | 375.620 | 493.316 | +117.695 | +117.695 |
| RMSNorm | 1,706 | 1,714 | 242.274 | 345.781 | +103.507 | +103.507 |
| KV FWHT | 360 | 360 | 47.670 | 150.539 | +102.868 | +102.868 |
| Other Attention | 0 | 20 | 0.000 | 58.055 | +58.055 | +58.055 |
| D2D | 15,894 | 320 | 44.153 | 90.345 | +46.191 | +46.191 |
| RoPE | 180 | 360 | 12.097 | 41.509 | +29.412 | +29.412 |
| MoE router | 352 | 1,080 | 8.014 | 17.094 | +9.080 | +9.080 |
| KV writes | 180 | 180 | 5.593 | 11.768 | +6.175 | +6.175 |
| MoE weighted reduction | 352 | 5,160 | 242.257 | 243.543 | +1.286 | +1.286 |
| Memset | 80 | 100 | 0.233 | 0.284 | +0.050 | +0.050 |
| MMVQ pack | 433 | 314 | 0.468 | 0.406 | -0.062 | -0.062 |
| SSM convolution | 271 | 271 | 203.979 | 203.799 | -0.180 | -0.180 |
| MMVQ | 433 | 314 | 8.311 | 8.050 | -0.261 | -0.261 |
| MMQ fixup | 480 | 0 | 3.134 | 0.000 | -3.134 | -3.134 |
| H2D packets up to 4 KiB | 6,685 | 4,030 | 4.868 | 1.729 | -3.139 | -3.151 |
| MMQ pack | 3,392 | 4,200 | 458.957 | 365.324 | -93.633 | -93.638 |
| D2H | 473 | 361 | 113.556 | 2.138 | -111.418 | -111.418 |
| MoE grouping | 936 | 0 | 154.508 | 0.000 | -154.508 | -154.508 |

### Decode

| Family | Hybrid count | Goose count | Hybrid sum, ms | Goose sum, ms | Δ sum, ms | Δ exclusive, ms |
|---|---:|---:|---:|---:|---:|---:|
| Vector Attention | 10,220 | 15,330 | 1,221.340 | 2,838.630 | +1,617.290 | +1,617.290 |
| Dense and generic matmul | 40,880 | 40,880 | 148.129 | 798.979 | +650.849 | +650.854 |
| Pointwise and GLU | 148,698 | 276,445 | 139.787 | 579.204 | +439.418 | +439.429 |
| MMVQ | 199,798 | 220,238 | 2,485.531 | 2,815.461 | +329.930 | +329.930 |
| MoE router | 20,440 | 61,320 | 48.592 | 199.810 | +151.218 | +151.212 |
| RMSNorm | 97,598 | 97,598 | 124.999 | 273.368 | +148.368 | +148.375 |
| H2D packets up to 4 KiB | 23,504 | 312,213 | 7.468 | 114.832 | +107.363 | +107.363 |
| GDN | 15,330 | 15,330 | 57.226 | 116.157 | +58.931 | +58.932 |
| MMVQ pack | 199,798 | 158,918 | 184.174 | 233.118 | +48.944 | +48.959 |
| KV FWHT | 20,440 | 20,440 | 18.111 | 54.504 | +36.393 | +36.396 |
| SSM convolution | 15,329 | 15,329 | 21.130 | 51.244 | +30.115 | +30.117 |
| KV writes | 10,220 | 10,220 | 14.544 | 34.848 | +20.304 | +20.304 |
| MoE weighted reduction | 20,440 | 20,440 | 29.556 | 46.860 | +17.304 | +17.302 |
| RoPE | 10,220 | 10,220 | 10.798 | 27.254 | +16.456 | +16.456 |
| D2H | 20,951 | 20,951 | 40.106 | 39.637 | -0.469 | -0.469 |
| H2D bulk | 67,626 | 120,311 | 1,850.626 | 1,806.046 | -44.580 | -44.580 |
| Tensor gather and blit | 87,888 | 35,768 | 208.506 | 83.284 | -125.222 | -125.206 |

## MMQ by recorded weight format
Format comes from the hybrid template or the specialized Goose
kernel name. The generic Goose MMQ path remains unresolved.
These are format-wide totals on the same Apex workload. A
launch grid is not a complete matrix shape, so these rows do
not isolate matched M/N/K problems or individual optimizations.

| Format | Hybrid calls | Goose calls | Hybrid sum, ms | Goose sum, ms | Δ sum, ms | Δ exclusive, ms |
|---|---:|---:|---:|---:|---:|---:|
| Q4_K | 2,928 | 4,050 | 2,387.148 | 9,718.617 | +7,331.470 | +5,856.868 |
| Q6_K | 464 | 1,020 | 755.079 | 2,413.051 | +1,657.972 | +1,657.972 |
| Unresolved format | 0 | 340 | 0.000 | 981.240 | +981.240 | +981.240 |

The archive's mmq-launch-geometry.tsv keeps exact template names,
grid and block dimensions, count and mean duration. The next
optimization pass needs matching logical tensor dimensions and
actual dispatch to turn this census into a per-shape comparison.

## CUDA Graph replay and host submission

Graph sizes count kernels sharing a replay correlation inside
the decode window. The results show the hybrid's dominant
43–44-kernel scope and the experimental Goose graph's 2-, 3-
and 18-kernel scopes. Zero in this table means no replay of
that size. Node durations are already in the family totals.

| Kernels per replay | Hybrid replays | Goose replays | Hybrid kernel sum, ms | Goose kernel sum, ms | Δ, ms |
|---:|---:|---:|---:|---:|---:|
| 2 | 0 | 97,411 | 0.000 | 1,714.379 | +1,714.379 |
| 3 | 0 | 5,110 | 0.000 | 2,838.630 | +2,838.630 |
| 18 | 0 | 20,400 | 0.000 | 2,049.064 | +2,049.064 |
| 20 | 510 | 0 | 487.006 | 0.000 | -487.006 |
| 27 | 510 | 0 | 48.352 | 0.000 | -48.352 |
| 43 | 4,581 | 0 | 1,699.184 | 0.000 | -1,699.184 |
| 44 | 14,763 | 0 | 2,268.157 | 0.000 | -2,268.157 |
| 46 | 509 | 0 | 192.333 | 0.000 | -192.333 |

Both timelines contain more than half a million nodes produced
by graph launches. Goose nevertheless submits 422,229 direct
CUDA calls in decode, versus 6,736 in Optimized Hybrid. The
dominant Goose 18-node group replays 20,400 times. The remaining
target is the chain through the following layer's recurrent or
attention work and next host-visible Top-K.

### Prefill CUDA APIs

| API family | Hybrid calls | Goose calls | Hybrid wall sum, ms | Goose wall sum, ms | Δ wall, ms | Hybrid / Goose overlap with GPU idle, ms |
|---|---:|---:|---:|---:|---:|---:|
| All CUDA APIs, union | 103,206 | 220,716 | 11,495.934 | 22,926.822 | +11,430.887 | 596.548 / 2,203.205 |
| Asynchronous memcpy API | 81,127 | 103,672 | 414.206 | 314.489 | -99.717 | 81.670 / 170.628 |
| CUDA allocation and mapping | 84 | 151 | 0.135 | 22.810 | +22.675 | 0.121 / 8.233 |
| CUDA synchronization and waits | 1,728 | 15,831 | 10,788.984 | 2,974.623 | -7,814.360 | 297.601 / 8.221 |
| Direct launch | 18,751 | 26,112 | 83.922 | 107.471 | +23.549 | 11.286 / 43.654 |
| Other CUDA API | 1,516 | 15,699 | 208.688 | 33.759 | -174.930 | 205.870 / 18.983 |
| Synchronous memcpy API | 0 | 59,251 | 0.000 | 19,473.670 | +19,473.670 | 0.000 / 1,953.487 |

### Decode CUDA APIs

| API family | Hybrid calls | Goose calls | Hybrid wall sum, ms | Goose wall sum, ms | Δ wall, ms | Hybrid / Goose overlap with GPU idle, ms |
|---|---:|---:|---:|---:|---:|---:|
| All CUDA APIs, union | 199,771 | 1,040,944 | 7,677.909 | 15,036.544 | +7,358.635 | 1,167.945 / 5,575.641 |
| Asynchronous memcpy API | 112,081 | 312,213 | 425.953 | 910.937 | +484.985 | 232.821 / 224.582 |
| CUDA allocation and mapping | 0 | 1 | 0.000 | 0.015 | +0.015 | 0.000 / 0.015 |
| CUDA synchronization and waits | 59,766 | 26,060 | 5,963.481 | 1,105.418 | -4,858.062 | 395.938 / 220.557 |
| Direct launch | 6,736 | 422,229 | 16.568 | 1,269.278 | +1,252.709 | 1.851 / 480.956 |
| Graph launch | 20,873 | 122,921 | 1,256.972 | 1,361.493 | +104.522 | 529.579 / 947.275 |
| Graph setup and update | 314 | 16,257 | 14.038 | 38.791 | +24.753 | 6.859 / 37.109 |
| Other CUDA API | 1 | 1 | 0.898 | 0.012 | -0.886 | 0.898 / 0.012 |
| Synchronous memcpy API | 0 | 141,262 | 0.000 | 10,350.600 | +10,350.600 | 0.000 / 3,665.137 |

No target-process OS wait rows matched the named wait filters
inside these device windows. This leaves unobserved scheduling
and waiting open. API wall sums can include earlier GPU work and
must remain separate from sampled CPU time and exclusive GPU time.

## Sampled CPU execution

Clock samples assign exclusive time to the leaf function. Counts
are exclusive CPU milliseconds divided by the 2 ms interval.
Inclusive stack context is retained in the raw report. CPU
phases are estimated on separate runs and do not establish
the causal owner of the idle GPU gaps.

### Prefill

| CPU category | Hybrid samples | Goose samples | Hybrid exclusive CPU, ms | Goose exclusive CPU, ms | Δ CPU, ms |
|---|---:|---:|---:|---:|---:|
| Total | 138 | 463 | 276 | 926 | +650 |
| System libraries | 31 | 175 | 62 | 350 | +288 |
| CUDA driver | 94 | 189 | 188 | 378 | +190 |
| CPU gather | 0 | 80 | 0 | 160 | +160 |
| Other CPU functions | 9 | 19 | 18 | 38 | +20 |
| Graph construction and dispatch | 4 | 0 | 8 | 0 | -8 |

### Decode

| CPU category | Hybrid samples | Goose samples | Hybrid exclusive CPU, ms | Goose exclusive CPU, ms | Δ CPU, ms |
|---|---:|---:|---:|---:|---:|
| Total | 100 | 189 | 200 | 378 | +178 |
| CUDA driver | 60 | 114 | 120 | 228 | +108 |
| System libraries | 24 | 45 | 48 | 90 | +42 |
| Other CPU functions | 11 | 27 | 22 | 54 | +32 |
| Graph construction and dispatch | 1 | 2 | 2 | 4 | +2 |
| Sampling and token selection | 4 | 1 | 8 | 2 | -6 |

## Exact recorded CUDA kernel and transfer names

An empty side has zero events with that exact spelling. The
GPU family table compares equivalent operations across names.
The per-symbol delta is an inventory difference. The exclusive
delta counts only time with that symbol active alone.

### Prefill

| CUDA kernel or transfer | Hybrid count | Goose count | Hybrid sum, ms | Goose sum, ms | Δ sum, ms | Δ exclusive, ms | Hybrid mean, µs | Goose mean, µs |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `H2D bulk` | 58,075 | 158,212 | 3,497.644 | 5,250.378 | +1,752.734 | +278.089 | 60.226 | 33.186 |
| `goose_mmq_q4_dense_x128` | 0 | 2,000 | 0.000 | 5,309.750 | +5,309.750 | +5,309.750 | 0.000 | 2,654.875 |
| `goose_mmq_q4_routed_x128` | 0 | 640 | 0.000 | 3,546.564 | +3,546.564 | +2,357.982 | 0.000 | 5,541.506 |
| `mul_mat_q` | 3,392 | 0 | 3,142.227 | 0.000 | -3,142.227 | -3,142.227 | 926.364 | 0.000 |
| `goose_flash_causal_mma` | 0 | 80 | 0.000 | 2,966.034 | +2,966.034 | +2,966.034 | 0.000 | 37,075.425 |
| `goose_vector_ops` | 0 | 4,867 | 0.000 | 1,648.458 | +1,648.458 | +1,648.458 | 0.000 | 338.701 |
| `flash_attn_ext_f16` | 90 | 0 | 1,402.655 | 0.000 | -1,402.655 | -1,402.655 | 15,585.056 | 0.000 |
| `goose_gdn_mma` | 0 | 240 | 0.000 | 1,359.241 | +1,359.241 | +1,359.241 | 0.000 | 5,663.503 |
| `goose_mmq_q6_dense_x128` | 0 | 320 | 0.000 | 1,248.969 | +1,248.969 | +1,248.969 | 0.000 | 3,903.028 |
| `goose_mmq_matmul` | 0 | 340 | 0.000 | 981.240 | +981.240 | +981.240 | 0.000 | 2,886.000 |
| `goose_mmq_q6_routed_x128` | 0 | 320 | 0.000 | 927.675 | +927.675 | +927.675 | 0.000 | 2,898.986 |
| `goose_mmq_q4_routed_x64` | 0 | 720 | 0.000 | 769.678 | +769.678 | +483.659 | 0.000 | 1,068.997 |
| `goose_tensor_blit` | 0 | 632 | 0.000 | 493.316 | +493.316 | +493.316 | 0.000 | 780.563 |
| `gdn_single` | 240 | 0 | 470.785 | 0.000 | -470.785 | -470.785 | 1,961.605 | 0.000 |
| `quantize_mmq_q8_1` | 3,392 | 0 | 458.957 | 0.000 | -458.957 | -458.957 | 135.306 | 0.000 |
| `goose_dense_mma_reference` | 0 | 280 | 0.000 | 407.036 | +407.036 | +407.036 | 0.000 | 1,453.698 |
| `goose_rms_reference` | 0 | 1,714 | 0.000 | 345.781 | +345.781 | +345.781 | 0.000 | 201.739 |
| `concat_non_cont` | 270 | 0 | 339.794 | 0.000 | -339.794 | -339.794 | 1,258.498 | 0.000 |
| `goose_mmq_pack_q8` | 0 | 3,480 | 0.000 | 311.752 | +311.752 | +311.752 | 0.000 | 89.584 |
| `unary_gated_op_kernel` | 1,064 | 0 | 296.873 | 0.000 | -296.873 | -296.873 | 279.016 | 0.000 |
| `goose_moe_weighted` | 0 | 5,160 | 0.000 | 243.543 | +243.543 | +243.543 | 0.000 | 47.198 |
| `rms_norm_f32` | 1,706 | 0 | 242.274 | 0.000 | -242.274 | -242.274 | 142.013 | 0.000 |
| `moe_weighted_reduction_f32` | 352 | 0 | 242.257 | 0.000 | -242.257 | -242.257 | 688.231 | 0.000 |
| `goose_mmq_q6_routed_x64` | 0 | 340 | 0.000 | 224.291 | +224.291 | +224.291 | 0.000 | 659.681 |
| `ssm_conv_long_token_f32` | 240 | 0 | 203.931 | 0.000 | -203.931 | -203.931 | 849.714 | 0.000 |
| `goose_ssm_conv_long` | 0 | 240 | 0.000 | 203.645 | +203.645 | +203.645 | 0.000 | 848.522 |
| `k_bin_bcast` | 1,597 | 0 | 193.310 | 0.000 | -193.310 | -193.310 | 121.046 | 0.000 |
| `mm_ids_helper` | 936 | 0 | 154.508 | 0.000 | -154.508 | -154.508 | 165.073 | 0.000 |
| `goose_kv_fwht` | 0 | 360 | 0.000 | 150.539 | +150.539 | +150.539 | 0.000 | 418.163 |
| `D2D` | 15,894 | 320 | 44.153 | 90.345 | +46.191 | +46.191 | 2.778 | 282.327 |
| `D2H` | 473 | 361 | 113.556 | 2.138 | -111.418 | -111.418 | 240.077 | 5.922 |
| `goose_mmq_q4_reference` | 0 | 690 | 0.000 | 92.625 | +92.625 | +92.625 | 0.000 | 134.240 |
| `Kernel2` | 312 | 0 | 59.211 | 0.000 | -59.211 | -59.211 | 189.780 | 0.000 |
| `goose_q4_attention_tiles` | 0 | 10 | 0.000 | 56.578 | +56.578 | +56.578 | 0.000 | 5,657.838 |
| `goose_dense_mma` | 0 | 40 | 0.000 | 55.244 | +55.244 | +55.244 | 0.000 | 1,381.091 |
| `goose_mmq_pack_shared` | 0 | 720 | 0.000 | 53.571 | +53.571 | +53.567 | 0.000 | 74.405 |
| `fwht_cuda` | 360 | 0 | 47.670 | 0.000 | -47.670 | -47.670 | 132.418 | 0.000 |
| `goose_quantized_matmul` | 0 | 400 | 0.000 | 37.395 | +37.395 | +37.395 | 0.000 | 93.487 |
| `cpy_scalar` | 361 | 0 | 32.937 | 0.000 | -32.937 | -32.937 | 91.240 | 0.000 |
| `goose_rope_apply` | 0 | 180 | 0.000 | 29.356 | +29.356 | +29.356 | 0.000 | 163.087 |
| `mul_mat_vec_f` | 314 | 0 | 26.643 | 0.000 | -26.643 | -26.643 | 84.850 | 0.000 |
| `dequantize_block_q4_0` | 180 | 0 | 12.878 | 0.000 | -12.878 | -12.878 | 71.543 | 0.000 |
| `goose_rope_phases` | 0 | 180 | 0.000 | 12.153 | +12.153 | +12.153 | 0.000 | 67.518 |
| `goose_mmq_q6_reference` | 0 | 40 | 0.000 | 12.115 | +12.115 | +12.115 | 0.000 | 302.878 |
| `rope_multi` | 180 | 0 | 12.097 | 0.000 | -12.097 | -12.097 | 67.207 | 0.000 |
| `goose_set_q4_rows` | 0 | 180 | 0.000 | 11.768 | +11.768 | +11.768 | 0.000 | 65.376 |
| `goose_flash_q4_stage` | 0 | 160 | 0.000 | 10.718 | +10.718 | +10.718 | 0.000 | 66.988 |
| `goose_route_reduce` | 0 | 360 | 0.000 | 8.488 | +8.488 | +8.488 | 0.000 | 23.577 |
| `goose_mmvq_matvec` | 0 | 314 | 0.000 | 8.050 | +8.050 | +8.050 | 0.000 | 25.637 |
| `topk_moe_cuda` | 352 | 0 | 8.014 | 0.000 | -8.014 | -8.014 | 22.766 | 0.000 |
| `H2D packets up to 4 KiB` | 6,685 | 4,030 | 4.868 | 1.729 | -3.139 | -3.151 | 0.728 | 0.429 |
| `k_set_rows_quant` | 180 | 0 | 5.593 | 0.000 | -5.593 | -5.593 | 31.073 | 0.000 |
| `goose_route_topk` | 0 | 360 | 0.000 | 5.149 | +5.149 | +5.149 | 0.000 | 14.302 |
| `mul_mat_vec_q_moe` | 117 | 0 | 4.669 | 0.000 | -4.669 | -4.669 | 39.910 | 0.000 |
| `flash_attn_mask_to_KV_max` | 80 | 0 | 4.293 | 0.000 | -4.293 | -4.293 | 53.661 | 0.000 |
| `mul_mat_vec_q` | 316 | 0 | 3.641 | 0.000 | -3.641 | -3.641 | 11.523 | 0.000 |
| `goose_route_norm` | 0 | 360 | 0.000 | 3.457 | +3.457 | +3.457 | 0.000 | 9.603 |
| `mul_mat_q_stream_k_fixup` | 480 | 0 | 3.134 | 0.000 | -3.134 | -3.134 | 6.530 | 0.000 |
| `flash_attn_stream_k_fixup_general` | 80 | 0 | 2.571 | 0.000 | -2.571 | -2.571 | 32.134 | 0.000 |
| `k_get_rows_float_vec` | 271 | 0 | 2.003 | 0.000 | -2.003 | -2.003 | 7.392 | 0.000 |
| `goose_q4_attention_merge` | 0 | 10 | 0.000 | 1.477 | +1.477 | +1.477 | 0.000 | 147.653 |
| `unary_op_kernel` | 893 | 0 | 1.175 | 0.000 | -1.175 | -1.175 | 1.316 | 0.000 |
| `k_get_rows_float` | 313 | 0 | 0.884 | 0.000 | -0.884 | -0.884 | 2.824 | 0.000 |
| `goose_gated_delta_net` | 0 | 30 | 0.000 | 0.775 | +0.775 | +0.775 | 0.000 | 25.830 |
| `Memset` | 80 | 100 | 0.233 | 0.284 | +0.050 | +0.050 | 2.917 | 2.837 |
| `quantize_q8_1` | 433 | 0 | 0.468 | 0.000 | -0.468 | -0.468 | 1.080 | 0.000 |
| `goose_mmvq_quantize` | 0 | 314 | 0.000 | 0.406 | +0.406 | +0.406 | 0.000 | 1.293 |
| `mul_mat_f` | 39 | 0 | 0.386 | 0.000 | -0.386 | -0.386 | 9.894 | 0.000 |
| `scale_f32` | 60 | 0 | 0.322 | 0.000 | -0.322 | -0.322 | 5.360 | 0.000 |
| `gated_delta_net_cuda` | 30 | 0 | 0.184 | 0.000 | -0.184 | -0.184 | 6.147 | 0.000 |
| `goose_ssm_conv` | 0 | 31 | 0.000 | 0.154 | +0.154 | +0.154 | 0.000 | 4.957 |
| `dot_kernel` | 39 | 0 | 0.053 | 0.000 | -0.053 | -0.053 | 1.370 | 0.000 |
| `ssm_conv_f32` | 31 | 0 | 0.048 | 0.000 | -0.048 | -0.048 | 1.541 | 0.000 |
| `reduce_1Block_kernel` | 39 | 0 | 0.047 | 0.000 | -0.047 | -0.047 | 1.214 | 0.000 |
| `flash_attn_stream_k_fixup_uniform` | 10 | 0 | 0.041 | 0.000 | -0.041 | -0.041 | 4.131 | 0.000 |
| `concat_cont` | 1 | 0 | 0.001 | 0.000 | -0.001 | -0.001 | 1.408 | 0.000 |

### Decode

| CUDA kernel or transfer | Hybrid count | Goose count | Hybrid sum, ms | Goose sum, ms | Δ sum, ms | Δ exclusive, ms | Hybrid mean, µs | Goose mean, µs |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `H2D bulk` | 67,626 | 120,311 | 1,850.626 | 1,806.046 | -44.580 | -44.580 | 27.366 | 15.011 |
| `goose_mmvq_matvec` | 0 | 220,238 | 0.000 | 2,815.461 | +2,815.461 | +2,815.432 | 0.000 | 12.784 |
| `goose_flash_vec_q4_packed` | 0 | 5,110 | 0.000 | 2,779.986 | +2,779.986 | +2,779.986 | 0.000 | 544.029 |
| `mul_mat_vec_q` | 199,798 | 0 | 2,485.531 | 0.000 | -2,485.531 | -2,485.502 | 12.440 | 0.000 |
| `flash_attn_ext_vec` | 5,110 | 0 | 1,212.987 | 0.000 | -1,212.987 | -1,212.987 | 237.375 | 0.000 |
| `goose_quantized_matmul` | 0 | 40,880 | 0.000 | 798.979 | +798.979 | +798.974 | 0.000 | 19.544 |
| `goose_vector_ops` | 0 | 276,445 | 0.000 | 579.204 | +579.204 | +579.186 | 0.000 | 2.095 |
| `goose_rms_reference` | 0 | 97,598 | 0.000 | 273.368 | +273.368 | +273.368 | 0.000 | 2.801 |
| `goose_mmvq_quantize` | 0 | 158,918 | 0.000 | 233.118 | +233.118 | +233.103 | 0.000 | 1.467 |
| `quantize_q8_1` | 199,798 | 0 | 184.174 | 0.000 | -184.174 | -184.144 | 0.922 | 0.000 |
| `mul_mat_vec_f` | 40,880 | 0 | 148.129 | 0.000 | -148.129 | -148.121 | 3.624 | 0.000 |
| `rms_norm_f32` | 97,598 | 0 | 124.999 | 0.000 | -124.999 | -124.993 | 1.281 | 0.000 |
| `H2D packets up to 4 KiB` | 23,504 | 312,213 | 7.468 | 114.832 | +107.363 | +107.363 | 0.318 | 0.368 |
| `goose_gdn_decode` | 0 | 15,330 | 0.000 | 116.157 | +116.157 | +116.157 | 0.000 | 7.577 |
| `k_get_rows_float_vec` | 15,329 | 0 | 99.514 | 0.000 | -99.514 | -99.511 | 6.492 | 0.000 |
| `goose_route_reduce` | 0 | 20,440 | 0.000 | 88.636 | +88.636 | +88.636 | 0.000 | 4.336 |
| `goose_tensor_blit` | 0 | 35,768 | 0.000 | 83.284 | +83.284 | +83.284 | 0.000 | 2.328 |
| `goose_route_topk` | 0 | 20,440 | 0.000 | 81.158 | +81.158 | +81.158 | 0.000 | 3.971 |
| `D2H` | 20,951 | 20,951 | 40.106 | 39.637 | -0.469 | -0.469 | 1.914 | 1.892 |
| `k_bin_bcast` | 56,720 | 0 | 61.664 | 0.000 | -61.664 | -61.655 | 1.087 | 0.000 |
| `k_get_rows_float` | 36,791 | 0 | 57.277 | 0.000 | -57.277 | -57.275 | 1.557 | 0.000 |
| `gated_delta_net_cuda` | 15,330 | 0 | 57.226 | 0.000 | -57.226 | -57.224 | 3.733 | 0.000 |
| `goose_kv_fwht` | 0 | 20,440 | 0.000 | 54.504 | +54.504 | +54.504 | 0.000 | 2.667 |
| `goose_ssm_conv` | 0 | 15,329 | 0.000 | 51.244 | +51.244 | +51.244 | 0.000 | 3.343 |
| `unary_gated_op_kernel` | 56,209 | 0 | 51.017 | 0.000 | -51.017 | -51.004 | 0.908 | 0.000 |
| `goose_flash_vec_combine` | 0 | 5,110 | 0.000 | 50.277 | +50.277 | +50.277 | 0.000 | 9.839 |
| `topk_moe_cuda` | 20,440 | 0 | 48.592 | 0.000 | -48.592 | -48.592 | 2.377 | 0.000 |
| `goose_moe_weighted` | 0 | 20,440 | 0.000 | 46.860 | +46.860 | +46.858 | 0.000 | 2.293 |
| `goose_set_q4_rows` | 0 | 10,220 | 0.000 | 34.848 | +34.848 | +34.848 | 0.000 | 3.410 |
| `cpy_scalar` | 20,439 | 0 | 31.178 | 0.000 | -31.178 | -31.168 | 1.525 | 0.000 |
| `goose_route_norm` | 0 | 20,440 | 0.000 | 30.016 | +30.016 | +30.010 | 0.000 | 1.469 |
| `moe_weighted_reduction_f32` | 20,440 | 0 | 29.556 | 0.000 | -29.556 | -29.555 | 1.446 | 0.000 |
| `goose_rope_decode` | 0 | 10,220 | 0.000 | 27.254 | +27.254 | +27.254 | 0.000 | 2.667 |
| `unary_op_kernel` | 35,769 | 0 | 27.105 | 0.000 | -27.105 | -27.092 | 0.758 | 0.000 |
| `ssm_conv_f32` | 15,329 | 0 | 21.130 | 0.000 | -21.130 | -21.127 | 1.378 | 0.000 |
| `concat_cont` | 15,329 | 0 | 20.537 | 0.000 | -20.537 | -20.528 | 1.340 | 0.000 |
| `fwht_cuda` | 20,440 | 0 | 18.111 | 0.000 | -18.111 | -18.108 | 0.886 | 0.000 |
| `k_set_rows_quant` | 10,220 | 0 | 14.544 | 0.000 | -14.544 | -14.544 | 1.423 | 0.000 |
| `rope_multi` | 10,220 | 0 | 10.798 | 0.000 | -10.798 | -10.798 | 1.057 | 0.000 |
| `goose_flash_vec_pack_query` | 0 | 5,110 | 0.000 | 8.367 | +8.367 | +8.367 | 0.000 | 1.637 |
| `flash_attn_combine_results` | 5,110 | 0 | 8.353 | 0.000 | -8.353 | -8.352 | 1.635 | 0.000 |
