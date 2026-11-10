# Why the MMA path of `mul_mat_q` is not portable with this module format

After establishing that sm_89 takes the MMA branch (`mmq-mma` note in EXP236), the obvious next question is
whether that branch can be ported. Assessed rather than attempted, because the answer is structural.

## What the MMA path is made of

`ggml_cuda_mmq_vec_dot_q6_K_q8_1_mma` (mmq-vec-dot.cuh:1010) is ~120 lines, and its content is:

- `tile<16,4,int>`, `tile<8,4,int>`, `tile<16,8,int>` — register-level matrix fragment types with
  compile-time shapes, indexed by lane through `get_i(l)` / `get_j(l)`;
- `load_ldmatrix(A, ptr, stride)` — the `ldmatrix` instruction, which loadsa a 16×8 fragment across a warp
  from a specific shared-memory layout;
- `mma(C, A, B)` — inline-PTX `mma.sync` with tile-specific operand layouts;
- three separate `#elif` branches for MFMA, WMMA and Turing MMA, each with its own stride and fragment
  rules.

None of that is scalar arithmetic. It is warp-collective tensor-core programming where every value's
position depends on the lane id, and where the operands must arrive in a hardware-defined register layout.

## Why the module format cannot express it

A Goose module receives a base pointer and scalars and returns scalars. It has no notion of:

- a value that differs per lane and is indexed by lane;
- a value whose *shape* is a compile-time matrix fragment;
- an instruction that must be issued by the whole warp with operands spread across lanes.

Primitives could cover `ldmatrix` and `mma` as opaque calls — but then the port would be a sequence of
opaque tensor-core calls with lane-dependent bookkeeping around them, and the bookkeeping is exactly what
the module cannot hold. That is not a port, it is a rename.

## What this means for the census

The remaining 40% attributed to `mul_mat_q` splits into:

| path | share | portable? |
|---|---|---|
| `dp4a` inner products | not taken on sm_89 | ported and bit-exact (EXP235/236), **unreachable** |
| MMA inner products | everything on sm_89 | **not expressible** in this module format |

So on this hardware the largest remaining block is not a porting task at all. Saying otherwise would be
inventing work.

## What could change the picture

1. A module format that can express warp-collective operations — a real language change, not a port.
2. Running on a target without MMA (Pascal, or a CDP/fallback configuration), where the `dp4a` ports that
   already exist and are verified would become the live path.
3. `GGML_CUDA_MMA=0`-style build option, if one exists, to force the `dp4a` path on sm_89 — worth checking
   before anything else, because it costs one build flag and would make EXP235/236 immediately meaningful.

Item 3 is cheap enough that I should check it rather than speculate about it. Recorded as the next concrete
step.

---

# Item 3 checked: the `dp4a` path can be forced, and the port binds

I said I would check this rather than speculate. Done, and it changes the picture.

## `TURING_MMA_AVAILABLE` is one preprocessor condition

    common.cuh:289
    #if !defined(GGML_USE_HIP) && !defined(GGML_USE_MUSA) && __CUDA_ARCH__ >= GGML_CUDA_CC_TURING
    #define TURING_MMA_AVAILABLE
    #endif

No build option exists, but the condition is a single `#define`. In the **copied** tree I added
`&& !defined(GGML_FORCE_DP4A_PROBE)` and rebuilt with `-DGGML_FORCE_DP4A_PROBE=1`. Production sources
untouched.

## What happens

    stock cubin            0 IDP.4A
    forced-dp4a cubin  28672 IDP.4A

The `dp4a` branch is instantiated and the MMA branch is gone. And with the Goose override in place, the
kernel changes:

| build | Q6_K `mul_mat_q` kernel |
|---|---:|
| stock (MMA) | 3625 instructions, 0 `IDP.4A` |
| forced `dp4a`, native inner product | 4079 instructions |
| **forced `dp4a`, Goose inner product** | **4047 instructions** |

So the port **binds** — 32 fewer instructions than the native `dp4a` version — where all three previous
wiring attempts produced byte-identical cubins. The copy-the-directory mechanism was the thing that worked,
exactly as recorded.

## What this is and is not

It is a **probe configuration**, not a promotion: forcing `dp4a` on sm_89 disables the tensor-core path for
*every* K-quantised type, which would be a large performance decision on its own and is not mine to take.
It also does not yet prove correctness in the real kernel — that needs the same GPU differential at the
kernel level, then an Apex run.

What it does establish, and it is the first time in this line:

1. the ported arithmetic is reachable on a real configuration;
2. it is not slower by code size in that configuration;
3. the wiring mechanism is solved, so the remaining work is measurement rather than plumbing.

## Where this leaves the MMQ family

- sm_89 production path: MMA, not expressible in this module format.
- forced `dp4a` path: **the ports fit, verified bit-exact at unit level, bound at kernel level, slightly
  smaller than native.**
- A non-MMA target (Pascal, or this forced configuration) would use them directly.

The honest statement is therefore narrower than "MMQ is unportable" and wider than "MMQ is ported": the
arithmetic is ported, it is unreachable by default on this GPU, and it becomes reachable through one
compile-time flag whose performance implications are a separate decision.

## Verified in the forced configuration

    Q6_K mul_mat_q kernel, forced dp4a
      native inner product   4079 instructions   SASS md5 67525ef7660f
      Goose inner product    4047 instructions   SASS md5 bb713d330a08

Different code, **32 instructions fewer**, and the opcode histogram is identical between the two — so the
difference is scheduling and register allocation, not a different algorithm. Both use `IDP.4A`; the Goose
version simply arranges the surrounding work slightly better.

This is the first configuration in the whole MMQ line where the Goose version is **not larger than native**.
It says nothing about speed: an instruction count difference of 0.8% is far inside the noise this project
has measured, and EXP223 taught me not to extrapolate from counts.

## What is still missing before any claim

1. **Correctness in the real kernel.** The unit differential (EXP236, `worst_rel=0` on 65536 elements)
   validates the function in isolation. It does not validate the tile integration inside this kernel.
   Building that needs input tensors shaped the way `mul_mat_q` expects, which is a harness in itself.
2. **Speed in the forced configuration**, which is a different question from speed in the default one — the
   whole point of forcing `dp4a` would be to trade tensor-core throughput for portability, and that trade
   needs its own measurement.
3. **A decision**, not a measurement: whether forcing `dp4a` on sm_89 is acceptable at all. It affects every
   K-quantised type, not just the ones ported here.

## My recommendation, stated as one

If the goal is "show the arithmetic runs on Goose", this is done and evidenced. If the goal is "make Apex
faster", forcing `dp4a` is almost certainly the wrong direction — tensor cores exist for this workload, and
the ported path is a fallback. Those two goals should not be conflated in the ledger, and I have kept them
separate.
