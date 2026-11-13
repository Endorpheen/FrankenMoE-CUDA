# P2-16 inventory: CPU audit rows against existing evidence

Criterion: "All CPU audit rows assigned and zero owned native executable logic remains in candidate; hardware-unavailable variants stay
explicitly incomplete."

## What already exists as evidence

| clause | evidence | state |
|---|---|---|
| all CPU audit rows assigned | P2-18 link map: 33 objects, every one classified by a mechanical marker (`gg_function_N` present or absent) -- 13 Goose-emitted, 6 bridges, 14 native primitives | assigned |
| audited primitives rather than silent remainder | P1-11: backends enumerated (base/cpu/cuda built, Metal/Vulkan/SYCL/OpenCL/ROCm absent and recorded as hardware-unavailable); SIMD kernels named with the compiler-scope reason | audited |
| hardware-unavailable variants stay incomplete | P1-11 table, plus the CUDA long-tail audit in `remainder-audit.md`, which ends in "no unported work except CUTLASS/cuBLAS internals" | explicit |
| zero **owned** native executable logic | P2-18 shows the candidate's own C++ is limited to six integration bridges (`core.o`, `backend-cpp.o`, `backend-traits.o`, two `bridge.o`, `cpu-bridge.o`), which project native structures into the modules; the executable logic is either Goose-emitted or upstream | needs a definition |

## The one clause that is ambiguous

"Owned" is not defined anywhere in the plan. Two readings give opposite answers for the same evidence:

1. **Owned = written by this project.** Then the six bridges are project C++, but they are projections and plumbing, not computation;
   every computation is either Goose-emitted or upstream. Under this reading the clause holds.
2. **Owned = shipped in the candidate.** Then the fourteen native primitives are native executable logic in the candidate, and the clause
   can never hold without porting SIMD kernels the compiler does not support.

P1-11 already settled the same question for the backends by naming every native kernel and its reason, which matches reading 1. But the
plan's wording does not say so, and closing P2-16 on my own reading would be a scope decision taken by the agent.

## Where this leaves the item

Everything except that clause is evidenced above. The remaining question is a definition, and it belongs next to the other decisions in
`OWNER-DECISIONS-20261112.md` rather than to a rebuild.
