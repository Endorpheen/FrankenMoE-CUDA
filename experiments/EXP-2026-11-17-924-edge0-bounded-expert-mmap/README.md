# EXP-2026-11-17-924: bounded readonly expert mappings

Status: **ACCEPTED FOR FIXED75/256; HANDOFF READY;15-17TARGET OPEN**.
Reference EXP92344276ead1667a2c9bacfa13c461132b20be55792.
Hypothesis and frozen criteria: integration/edge0/PLAN-924.md.
No further model tests are planned in this session after Igor requested handoff.

## Measurement and mechanism

Current EXP923 host profile:37.835605s decode, CPU22.557858140s, positioned
expert reads5.634450233s, graph0.414696322s. A15% read fraction suggested an
optimistic1.175x bound before accounting for the separate staging-to-private
cache copy and cache handling. Actual expert requests:92385 hits/30015 misses.
D-reuse remains active:606398 matvecs vs673200 without reuse over12240 callbacks.

The candidate replaces copied expert payloads with bounded readonly MAP_PRIVATE
regions. Goose expert-lru still owns admission, promotion, retention and victims.
Selected records stay valid through the synchronous inner graph. Eviction unmaps
the old slot before mapping a replacement; descriptors publish only after mmap
succeeds. A failed mapping aborts the candidate. No model write, full-file mapping,
extra cache quota, activation precision or routing change. Cache0 retains the old
uncached pread path. FRANKEN_EDGE0_EXPERT_MMAP=0 disables mappings for control;
accepted builds enable the change by default. All prior caches/D-reuse stay on.

With4KiB pages the maximum span is1171456 bytes/record, so3072MiB holds2749
mapped slots rather than2765 copied slots. Worst virtual mapping3220332544 bytes
stays below3221225472 budget. These are limits, not a claim all pages are always
resident. Alignment/tail access remain within the validated complete pack.

The20 real top10/shared fixture cases (40 callbacks) pass default,legacy,
16MiB eviction-cache,cache0 and material-rebind modes. Existing tolerances remain
unchanged, real max_abs4.65661287e-10 and stress9.31322575e-10; repeat outputs
are bitwise equal. Those checks preceded a diagnostic-only timer relabel to
separate whole mapping preparation (including munmap) from legacy pread. The
arithmetic and ownership path did not change; the full model gates below use
the final instrumented binary. Rebuild was one changed native unit, not backend.

One final candidate host profile has31.696422s decode: CPU23.345116274s,
mapping preparation1.538717188s, graph0.407109483s, PLE0.014758114s. CPU includes
mapping faults; zero pread time does not imply zero weight-access cost. Decode
cache92276 hits/30124 misses; mapped address spans total35197034496 bytes over
the request (turnover, not resident memory). Process/global swap0, IDs exact256.
The diagnostic result8.045072 decode is not used for performance acceptance.

## Manual serial75/256 comparison

Same ctx512,b/ub1,t8,ngl48,MTPoff,greedy,official local Jinja with thinking off,
normal EOS, host cache quotas and warm shared filesystem cache. No reset.
Attempted A1-B1-B2-A2. A2 global swap-out11109 pages disqualifies it despite
process swap0; preserve it and repeat only the affected gate as A3. Qualified
chronological order A1-B1-B2-A3. No fastest-result selection.

| Run | Prefill tok/s | Decode steps/s | RSS KiB | VRAM MiB | Global swap-out pages |
|---|---:|---:|---:|---:|---:|
| a1-924 | 6.484816 | 6.832269 | 11132972 | 5296 | 0 |
| b1-924 | 8.117328 | 8.235440 | 11113248 | 5296 | 0 |
| b2-924 | 8.193994 | 8.134234 | 11113252 | 5296 | 0 |
| a2-924 | 6.746964 | 6.900757 | 11132960 | 5296 | 11109 |
| a3-924 | 6.517436 | 6.856384 | 11133108 | 5296 | 0 |

| A median | 6.501126 | 6.844327 | 11133040 |5296|0|
| B median | **8.155661** | **8.184837** | 11113250 |5296|0|

Relative gains: **25.450% prefill/19.586% decode**.
RSS falls0.1778% (19790KiB); VRAM unchanged. Load median
3.408586s to2.2273335s. All four qualified processes have swap0 and global
swap-out delta0. Warm process read_bytes are0; no cold-SSD claim.

Mappings increase total minor faults1380043 to1689667.5 median, major faults
0 to224.5; system CPU10.6914175 to16.2268715s, user CPU380.5670885 to307.6451135s.
These counters cover the process through inference (including startup), while
prefill/decode intervals exclude model load. They demonstrate moved fault/kernel
work rather than its disappearance. Complete request still improves.

All five long requests and the diagnostic produce the same256 IDs/text SHA
8f996f89f2ed891c2e6ff7c059e6f100fab96271e3c8c1ed329da7c3e79d74b9.
They stop at output budget256, EOS is enabled. Short arithmetic returns the
original two IDs243429/148279, then natural EOS, own/global swap0. No general
quality, long context, concurrency, server or15-17tok/s acceptance claim.

## Preserved result and continuation

Independent verified six-ELF checkpoint:
/home/end0/AI/LlamaServer/edge0-mapped-checkpoint-20261117 with source snapshot,
manifest, exact rendered prompt, results and COMMAND.txt. Active experimental
build /home/end0/AI/LlamaServer/edge0-expert-mmap-20261117. Previous checkpoints
and Apex remain immutable. Build inputs plus incremental timer build log retained.
The supervisor now records only relevant FRANKEN_EDGE0/LD_LIBRARY_PATH environment
values explicitly, and CLI logs own fault/CPU counters after the timing window.
No private credentials, downloaded/copied weights, system settings or foreign
services changed. No parallel benchmarks or other agents.

Baseline C++ replaced/removed0; dead C++ deleted0; retained closure248213 lines/
350files; minimum necessary UNKNOWN. Native integration pack/header/CLI is
372lines of retained debt. Projection decisions/LRU/arithmetic composition
remain Goose; the server and whole runtime are not100%Goose.

Igor ended this Edge0 testing cycle and set the primary next task to continue
the100%Goose runtime migration in OpenCode. Preserve this checkpoint and keep
Edge0 performance as a deferred regression/reference surface, not the primary
assignment. The main Goose lineage is codex/goose-graph-entry-20261117 at
c43fb4b78cc617cdd001e383f80abc849738f9a4 (EXP919); it contains the real GDN,
full CPU trunk/logits and GGUF model reader/ownership from EXP917-919. These
are accepted components, not yet an independent text-generation/server runtime.
The next integration boundary is real tokenizer/sampler/text generation using
that lineage, then OS/session/CLI/backend/server closure. Do not blindly merge
or substitute the Edge0 native-backed branch for the main Goose port.

Deferred Edge0 direction if later requested: profile CPU FFN per operation;
CPU accounts23.345/31.696s in the diagnostic. Do not repeat the rejected EXP922
ordinary CUDA switch:90 primitive tolerance cases passed but full model
sequence diverged at token61 and was slower. Any new GPU attempt needs changed
numerical/dispatch conditions plus exact256 model parity. Preserve normal EOS,
pack revision, accepted defaults, memory caps and zero-swap gates.
