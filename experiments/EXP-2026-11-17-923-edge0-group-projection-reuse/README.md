# EXP-2026-11-17-923: share within-token group D projections

Status: **ACCEPTED FOR THE FIXED75/256 GATE;15-17TARGET OPEN**.
Control is EXP921; EXP922 CUDA remains rejected and absent from this candidate.
Hypothesis, bounds and frozen criteria: integration/edge0/PLAN-923.md.

Goose now reuses D(x) and inverse-scaled D(x) for selected experts from the same
group within one layer/token. It leaves every selected expert's gate/up, SiLU,
down,U, coefficient and rank-order reduction unchanged. Group identity comes
from the checked pinned pack descriptor. It is not cross-token state caching.
All tensor shape/type gates remain. FRANKEN_EDGE0_D_REUSE=0 assigns distinct
projection identities for the control; accepted builds enable reuse by default.
The variant builder now composes current canonical modules rather than a stale
previous composed runtime when --goose is requested. No backend rebuild needed.

The actual top10/shared CUDA-weight fixture passes12 cases/24 callbacks across
disabled,default,rebind-stress modes. Real max_abs4.65661287e-10, stress
9.31322575e-10, fixed2e-5abs+rel and route2e-6 gates unchanged. Repeat results
are bitwise equal. Actual CPU graph matvec count drops440 to380 across8 callbacks,
so duplicate work really disappears. These micro times are not model throughput.

## Manual serial model comparison

Identical75 input/256 output budget,ctx512,b/ub1,t8,ngl48,MTPoff,greedy,
local official rendered template with thinking disabled,normal EOS. All prior
DU,expert and dense caches enabled. Shared warm filesystem cache, not reset.
Attempted A-B-B-A; B1 has process swap0 but global swap-out310 pages and is
excluded solely by the frozen resource gate. Repeat only its affected gate as
B3. Qualified actual order is A1-B2-A2-B3; do not call that a balanced ABBA.

| Run | Prefill tok/s | Decode steps/s | RSS KiB | VRAM MiB | Global swap-out pages |
|---|---:|---:|---:|---:|---:|
| a1-923 | 6.171197 | 6.424526 | 11132704 | 5296 | 0 |
| b1-923 | 6.585832 | 6.754315 | 11132636 | 5296 | 310 |
| b2-923 | 6.713390 | 6.906143 | 11132716 | 5296 | 0 |
| a2-923 | 6.152700 | 6.414496 | 11132644 | 5296 | 0 |
| b3-923 | 6.545277 | 6.848386 | 11132680 | 5296 | 0 |

Qualified A medians: 6.1619485 prefill, 6.4195110
decode. B: **6.6293335 prefill, 6.8772645 decode**.
Relative gains **7.585% prefill/7.131% decode**,
meeting the predeclared5% decode threshold. Own VRAM remains5296MiB. Median
sampled RSS is 11132674 vs11132698KiB (+24KiB,
0.000216%); cache/arena quotas do not increase. Do not claim lower measured RSS.
All four qualified runs have process swap0 and global swap-out delta0.

All five long runs produce exactly the same256 token IDs and answer bytes as
EXP916/920/921, SHA256
8f996f89f2ed891c2e6ff7c059e6f100fab96271e3c8c1ed329da7c3e79d74b9.
The output ends at the256 budget, EOS remains enabled. Separate short arithmetic
returns the original two IDs243429,148279 (Russian answer four),then natural
EOS; own/global swap0. No quality, long-context or concurrent-session claim.

Independent verified six-ELF checkpoint:
/home/end0/AI/LlamaServer/edge0-projection-checkpoint-20261117, with source,
manifest, rendered prompt, command and results. Old checkpoints immutable.
Build: integration/edge0/variant.py --base /home/end0/AI/LlamaServer/edge0-inference-20261117/candidate-1 --out NEW_UNDER_LLAMA_SERVER --goose.
No Apex test, changes to model files, system tuning, foreign process stop or
parallel benchmarks. Baseline C++ replaced/removed0; dead C++ removed0;
retained closure248213lines/350files, necessary minimum UNKNOWN. Native pack,
header and CLI remain integration debt (342lines),
while the projection lifetime/composition decision runs in Goose.

Next EXP924: profile this path at per-operation boundaries and evaluate bounded
read-only expert mappings to avoid positioned-read copies. Expert I/O was the
next measured cost in EXP922; a mapped page fault moves cost into CPU compute,
so only end-to-end improvement can accept it. Preserve all old cache quotas,
controls, exact256 IDs, EOS and zero-swap gates.15-17tok/s remains an objective,
not a measured result or a reason to weaken correctness.
