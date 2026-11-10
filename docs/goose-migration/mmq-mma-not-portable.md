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
