# P3-11, P3-12, P3-13 closure: Apex CUDA scope

The three requirements are written for every expert/fusion/FA/recurrent variant
upstream supports. The Apex profile uses one concrete combination. This note
closes the Apex scope and records the remaining variants as C2 compatibility;
none of the remaining variants is counted as tested.

## P3-11 -- CUDA MoE expert matrix, weighted reduction and fused path

| clause | Apex state | evidence |
|---|---|---|
| weighted reduction | Goose, live | EXP179: 12 checks/6 cases, destinations float-identical, Apex pairs +0.11%/-0.29%, witness |
| expert routing / MM IDs | Goose for `n_expert_used = 8` | EXP181: warp/token slot decode, 8-wide any reduce, shfl scan, packed store, forward/inverse maps; 15/15 byte-identical |
| expert cache/admission/copy decisions | Goose, live | EXP150/262-265: bank/slot planning, admission, upload-run coalescing, A/B witnesses |
| expert matrix multiply | tensor-core MMQ/MMVQ/MMA primitive | P3-08/P3-09: Q4_K/Q6_K loaders and feeder Goose; `mma`/`ldmatrix` declared primitive |
| fused `gate_up_exps` | dead for this architecture | APEX-FINAL: forcing `if (false)` changes nothing |
| cold experts | not present in the Apex profile | no cold route in the loader/profile |
| other `n_expert_used` widths | C2 compatibility | EXP181 ports the Apex width 8 only |

Verdict: P3-11 closes on the Apex MoE scope. The expert decision layer is Goose,
the matrix kernel is the declared tensor-core primitive, and the dead fused path
is recorded rather than ported.

## P3-12 -- complete CUDA Flash Attention

| clause | Apex state | evidence |
|---|---|---|
| standard Q4 attention path | Goose, accepted | EXP173: numerical/host modules, corrected Apex 32K/128, exact IDs/text/probabilities, 127 graphs, `FG175FA host=0xf` |
| cache type | Q4_0 K/V in the agreed profile | accepted argv `-ctk q4_0 -ctv q4_0` |
| mask/alibi/heads/tails | the profile's shapes covered | EXP173 ordinary and long-random corpus, API/long-KV/capture/rollback |
| performance | not activated as a speed win | EXP173 2533.002/25.522 final, exact; stable parity with native unproven |
| other cache types, head/group configs, architectures | C2 compatibility | not exercised by this profile |

Verdict: P3-12 closes on the standard Q4 Apex attention path. The broader cache
type/architecture matrix is explicit C2 work; no universal-FA claim is made.

## P3-13 -- CUDA recurrent, SSM, gated-delta and sparse indexing

| clause | Apex state | evidence |
|---|---|---|
| SSM convolution | Goose, live | EXP170: serial/tiled, six `d_conv` sizes, fused SiLU/bias, 2.93M fp64 references, Apex witness `host=2/device=7/size=2` |
| GDN / gated-delta | Goose, live | EXP172: scalar+MMA host/device units, 25 native-bitwise cases, 9.97M checks, Apex `FG174S/FG174M`, exact output |
| state ownership / stream ordering | Goose where the Apex hybrid path uses it | EXP170/172 cache/once/error policy, 16-thread once check |
| RWKV6/7 `wkv.cu`, GLA `gla.cu` | C2/compiler compatibility | per-thread `float state[head_size]` register array; Goose has no local array type and the device backend is kept free of the stack allocator |
| other recurrence/sparse variants | C2 compatibility | not exercised by the Apex profile |

Verdict: P3-13 closes on the Apex recurrent scope: SSM and GDN are Goose and
live; RWKV/GLA are recorded as a compiler-language limitation, not silently
dropped.

## Common boundary

`mma`, `ldmatrix`, cuBLAS and driver primitives remain the declared primitive
boundary. The remaining variants are visible in C2 and in the compiler feedback,
and they do not block the Apex functional closure.
