# EXP-2026-11-17-921: retain immutable dense FFN bytes

Status: **ACCEPTED FOR THE FIXED256-TOKEN GATE; TARGET NOT REACHED**.
Reference EXP920 is commit4f4cdeb235178475bba61a8444aeb0cdd3b0a1bd.
No Apex binary, profile or service was changed or tested.

## Profile before the change

One cached75/256 request, unchanged ctx512,b/ub1,t8,ngl48,MTPoff,greedy and
local official Jinja with thinking disabled. Profiled decode58.075204s:
CPU FFN24.574678s, GPU weight copies11.220692s, expert reads7.914849s,
graph creation0.510233s,PLE0.355369s,D/U reads0. Expert decode cache counts:
92385 hits/30015 misses. There were59925 GPU dense-weight requests during
255 decode evaluations, repeatedly copying94069785600 bytes,368901120 per step.

Nsight now traces individual CUDA graph nodes for the first16 evaluations.
The CUDA kernel report is present: Q8_0 matrix-vector kernels account for51.5%
of GPU kernel time, BF16 matvec variants another30.6%. Those are fractions of
GPU time, not request time. CPU FFN remains a major request cost. Host CUDA
memcpy API time0.726204728s and stream sync0.288053237s overlap device intervals;
do not add them. Full report, log and resource data are retained.

The diagnostic process itself had swap0; global swap-out grew1005 pages with
profiling. It is not a zero-swap performance acceptance run. The owner-PID fix
correctly observed native RSS/VRAM this time. Profile output token IDs match the
control256 IDs. Nsight stdout adds banners, so its raw stdout is not used as
an answer-string checksum.

## Hypothesis, bound and implementation

Record before implementation: `integration/edge0/NEXT-PROFILE.md`.
The five router/shared-expert weights per layer are immutable after binding.
Retain their exact raw CPU bytes after the first GPU-to-CPU read, rather than
copying on every token. CPU-resident weights stay borrowed, without a copy.
No quantisation, activation, route or sum order changes. The new storage is
cleared on an explicit layer rebind; it is not a cache for mutable/LoRA weights.
The qualified layout is pinned BF16 router/shared-router and Q8_0 shared FFN.
A per-layer8MiB ceiling bounds48 layers to384MiB; the actual GPU-resident dense
weights total368901120 bytes. Old D/U and expert caches remain enabled.

Amdahl estimate from the profiled copy fraction19.32% was about1.24x. Allocation
and synchronisation effects also need measurement; the estimate was not a15tok/s
promise. `FRANKEN_EDGE0_DENSE_CACHE=0` disables this feature; new builds enable it
by default. Native resource storage remains explicit C++ debt.

## Correctness probes

Reuse unchanged Goose output/math and native model objects. Rebuild only the
changed native unit and CLI; link the accepted Goose object with recorded input
paths. The fixture creates actual CUDA weight buffers, then compares the real
copy/retention callback against an independent CPU graph. It tests disabled,
enabled and material rebind-stress modes, four cases0,0,47,0 each, with two
callback evaluations per case. Repeated results are bitwise equal.

All12 reference cases/24 callbacks pass. Real-weight output maximum absolute
error4.65661287e-10; stressed shared-router maximum9.31322575e-10. The test
alternates aligned positive/negative BF16 shared gates in private fixture copies
so an ignored rebind would be material. No model file is modified. Predeclared
route2e-6 and F32 output2e-5abs+rel tolerances stay unchanged. Enabled mode records
20 dense hits/20 initial or rebind misses versus0/40 disabled; copied bytes halve
for this two-evaluation fixture. This is mechanism evidence, not a speed claim.

## Real256-token comparison, no Nsight

The attempted A-B-B-A was extended only to replace a failed resource gate:
A1 had process swap0 but global swap-out+1785 pages and is diagnostic. Keep its
numbers,5.098041 decode/4.944480 prefill, but exclude it from acceptance.
B1,B2,A2,A3 are the four qualified runs. Their actual order is B-B-A-A, not a
fully balanced quartet. All use identical binary/profile/prompt; only dense
retention toggles. Filesystem cache is warm/shared, not reset. These conditions
are warmer than EXP920's earlier block: compare this cycle with its own A.

| Run | Prefill tok/s | Decode steps/s | Max RSS KiB | Process VRAM MiB |
|---|---:|---:|---:|---:|
| A2 | 4.961570 | 5.092904 | 10783112 | 5296 |
| A3 | 4.981758 | 5.049534 | 10783112 | 5296 |
| B1 | 6.252056 | 6.465331 | 11132628 | 5296 |
| B2 | 6.242830 | 6.572063 | 11132708 | 5296 |
| A median | 4.971664 | 5.071219 | 10783112 | 5296 |
| B median | 6.247443 | 6.518697 | 11132668 | 5296 |
| Relative | +25.66% | +28.54% | +3.24% | 0% |

Four accepted processes have swap0 and global swap-out delta0. Every256-ID
array and all answer bytes match the original uncached control, SHA256
`8f996f89f2ed891c2e6ff7c059e6f100fab96271e3c8c1ed329da7c3e79d74b9`.
Long runs stop at the256 output budget; EOS is not masked. The separate short
request still returns `четыре`, IDs243429,148279, then normal EOS. Its process
swap is0 but global swap-out+70 pages; it is functional EOS evidence and not
an additional clean-resource speed sample. All exclusions remain in evidence.

## Saved state and continuation

Independent verified six-ELF checkpoint:
`/home/end0/AI/LlamaServer/edge0-dense-checkpoint-20261117/`, with source snapshot,
manifest, generated Goose input, request and COMMAND.txt. The EXP916/920 sets
remain immutable. No weights are copied to disk or included in Git.
The host supports this pinned readonly pack/profile; general LoRA, concurrent
sessions, long context and a standalone Goose server are not accepted here.

Baseline C++ replaced-and-removed0; dead C++ deleted0. Retained closure248213
lines/350 files; necessary minimum UNKNOWN. Native glue340 lines remains debt.
This result does not complete the full-Goose goal or the15-17tok/s target.

Next EXP922: profile the newly accepted path, identify the largest remaining
CPU FFN/weight I/O/placement/synchronisation cost, and probe a targeted solution.
Any GPU arithmetic path must pass the existing numerical and256-token gates;
do not loosen them after observing a mismatch. Continue serial cycles under
Igor's explicit authorisation, with the accepted controls preserved.
