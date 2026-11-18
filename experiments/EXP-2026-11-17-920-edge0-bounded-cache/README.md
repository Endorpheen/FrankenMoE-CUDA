# EXP-2026-11-17-920: bounded immutable Edge0 RAM caches

Status: **ACCEPTED FOR THE FIXED 75/256 GATE; PERFORMANCE WORK CONTINUES**.
Igor's current cycle is profiling -> targeted change ->256-token parity and
performance ->commit ->next experiment. The15-17 tok/s target is not reached.
No Apex test or modification was performed in this experiment.

## Measured mechanism before implementation

The immutable first-answer control is EXP916, commit
`d02a274a1e3aa03e7dc7d1459d191229bf7f1d7a`. The hypothesis/limits were recorded
in `integration/edge0/PROFILE.md`. Profile one actual75-input/256-output request,
ctx512,b/ub1,t8,ngl48,MTPoff,greedy diagnostic sampling, the exact local Jinja
with thinking disabled. Normal EOS is enabled. The long request reached its
output budget rather than EOS; a separate short Russian request tests EOS.

Decode took192.121669s for255 actual compute steps under instrumentation:

| Non-overlapping host category inside FFN | Seconds | Fraction of request decode |
|---|---:|---:|
| D/U positioned reads | 115.679940 | 60.21% |
| Expert positioned reads | 28.105901 | 14.63% |
| CPU graph compute, including native work/pool preparation | 23.962597 | 12.47% |
| GPU weight copies to CPU | 11.922452 | 6.21% |
| Graph construction | 0.602771 | 0.31% |
| PLE row provider | 0.362683 | 0.19% |

The whole FFN interval180.752106s includes its subcategories; do not add it to
them. Host GPU copy intervals can overlap device intervals and are not added.
Nsight captured the first16 decode evaluations:800 GPU graph intervals total
0.249402601s;5600 device memcpy intervals0.345888163s; host stream-sync API time
0.305133766s. Device D2H copied5926.404 MB and H2D9.682 MB in that window.
The trace uses graph-level CUDA events, not individual kernel events. Its
`cuda_gpu_kern_sum` report therefore says no kernel data; GPU graph intervals
were queried directly from the retained SQLite data. OSRT pread intervals
summed9.096504739s in the same window. These sums are diagnostics, not additive
critical-path percentages or unprofiled throughput.

Initial Nsight resource supervision accidentally followed its launcher. Its
reported46 MiB RSS/zero GPU/IO are invalid and excluded; native `ru_maxrss` is
still available. Target selection now verifies the CLI executable and its exact
owned prompt path if it is detached from the initial launcher. A later proc IO
race at process exit raised PermissionError in B1; that diagnostic is excluded
from acceptance. Missing/permission-denied exit samples are now marked incomplete,
and samples are saved incrementally. No unrelated process is selected or stopped.

## Hypothesis and implementation

Eliminate repeated D/U reads by keeping all1415577600 immutable Q4 D/U bytes
in RAM. Bound expert storage to3072MiB:2765 complete1164800-byte records,
3220672000 bytes. Reuse `goose/model/expert-lru.goose` for query, promotion,
eviction and publication. Native glue only owns byte storage and the physical
slot map. A failed positioned read publishes nothing. Promote all selected hits
before miss eviction, preserving the current ten routed experts. Data, precision,
route order, column scales, shared expert and arithmetic are unchanged.

Amdahl estimate before implementation: eliminating the measured D/U60.21%
alone has an upper bound about2.51x; expert-read elimination can add a separate
benefit, subject to the retained fraction and copy/compute costs. Graph creation
was0.31%, so persistent graphs were deliberately not the first optimisation.
PLE also stays on its already bounded provider. No VRAM cache is added yet.

Caches are enabled by default. `FRANKEN_EDGE0_DU_CACHE=0` disables D/U retention;
`FRANKEN_EDGE0_CACHE_MIB=0` disables expert retention. Other byte quotas are
configuration, not an opt-in speed flag. Only the measured3072MiB configuration
is accepted here. The64MiB arena, ten-record read scratch, metadata and dense
model buffers are additional memory. The debug `external buffers` line reports
D/U+stream+arena, separately from the explicit expert-cache budget line.

## Correctness and isolated build

The variant builder reuses unchanged native model objects and recompiles only
the changed native unit, CLI and composed Goose C. Seven accepted modules plus
the existing LRU compose/check/emit with the pinned compiler:123 specialisations.
No compiler/backend source or accepted binary is overwritten. The reproducible
full-source build script includes the LRU and the CLI's explicit dl dependency.

The independent native FFN fixture checks0,0,47,0 in each of three modes:
default3072MiB, a12MiB cache with ten slots forcing eviction/reload, and both
caches disabled. All12 cases pass, including hit/reload behaviour. Combined F32
output maximum absolute error remains4.65661287e-10; predeclared route2e-6 and
output2e-5abs+rel tolerances were not changed. Default small gate:20 hits/20
misses; forced eviction:10 hits/30 misses; disabled:0 hits/40 misses.

All four accepted real runs produce the exact same256 token IDs and answer
bytes. SHA256:
`8f996f89f2ed891c2e6ff7c059e6f100fab96271e3c8c1ed329da7c3e79d74b9`.
No token hiding, expert dropping, EOS masking, speculation or quant change.
The short arithmetic request still returns `четыре`, exact IDs243429,148279,
then normal EOS. That short run is functional evidence, not a speed benchmark.

## Paired real results, no Nsight

One process at a time, balanced A-B-B-A order. A disables both caches; B uses
the default caches. Application caches start empty in every new B process. OS
page cache is shared/reclaimable; do not label these cold-disk results. The
extra B1 with the supervisor race is retained separately and not selected as a
winner. Same binary, workload, placement, sampling and cache state rules.

| Run | Prefill tok/s | Decode steps/s | RSS KiB, max of sampled/native | Process VRAM MiB |
|---|---:|---:|---:|---:|
| A1 | 1.322962 | 1.357226 | 6284372 | 5296 |
| B2 | 4.175996 | 4.608564 | 10783112 | 5296 |
| B3 | 4.181252 | 4.578667 | 10783184 | 5296 |
| A2 | 1.323200 | 1.346233 | 6284336 | 5296 |
| A median | 1.323081 | 1.351730 | 6284354 | 5296 |
| B median | 4.178624 | 4.593616 | 10783148 | 5296 |
| Relative | +215.83% (3.158x) | +239.83% (3.398x) | +71.59% | 0% |

All four child exit codes0; process swap0 and system swap-out delta0. RSS rises
about4.29GiB to10.28GiB. It excludes global filesystem page cache. Median model
load1.287284 ->3.363135s, +2.075851s: D/U loading and cache allocation are
required startup work and are not hidden in the request timing.

At330 total evaluated tokens the B expert cache has118199 hits/40201 misses,
74.62058% hit rate. The integer miss count is uniquely reconstructed from the
final rounded byte counter: one record is1.11083984375MiB, much larger than its
0.01MiB rounding interval. Subsequent profiling will also expose direct counters.
All per-token D/U accesses hit after the initial full read. PLE is unchanged.

Logical rchar at completion falls651.657GB ->48.254GB (~13.50x less, including
startup data). Physical read_bytes rises: A1/A2 about6.112/5.790GB total versus
B2/B3 about8.996/8.950GB. Per-phase counters are in evidence.json. Do not call
logical pread bytes disk I/O. Private cache residency competes with the OS page
cache, so physical reads may grow even though explicit repeated copies and
syscalls disappear. This is an accepted memory/I/O trade for a3.40x decode gain,
not a claim of disk bandwidth improvement.

## Checkpoint, audit and continuation

Verified independent six-ELF checkpoint:
`/home/end0/AI/LlamaServer/edge0-cached-checkpoint-20261117/`.
It includes hashes, source snapshot, generated Goose input, successful request
and exact command. No model copy or full PLE mapping. The earlier first-answer
checkpoint stays immutable. Runtime/source controls are never replaced in place.

C++ baseline lines replaced-and-removed0; dead C++ removed0. Retained baseline
closure248213 lines/350 files; necessary minimum UNKNOWN. Native resource,
callback and CLI glue remains debt, not Goose-only migration completion.
The ordinary qwen4exp implementation and other models remain in the native
library. The accepted Apex binary set and profile are untouched.

Igor explicitly requested continued cycles toward15-17tok/s, not stopping here.
Next EXP921: profile this cached path, then select the next measured bottleneck.
Investigate remaining expert reads, immutable GPU-to-CPU dense-weight copies,
CPU FFN and GPU execution/synchronisation. Set resource and numerical gates
before changing that next candidate; do not silently loosen256-token parity.
