# P3-08 closure: MMVQ decode on the Apex scope

Criterion: "Every registered MMVQ format/instance, expert routing, split K, tail
and batch mode executes Goose; Apex decode witnesses."

Apex uses Q4_K and Q6_K for the decode matvecs. Both have Goose implementations
with byte-exact fixtures and model witnesses. The remaining 17 quantized types,
`ncols_dst > 1`, fusion and multi-token ids are assigned to C2 compatibility;
they are not on the Apex 32K/128 decode path.

## Q4_K MMVQ

EXP185:

- `mul_mat_vec_q<Q4_K>` (`ncols_dst = 1`, no fusion): tid decomposition,
  `bq8_offset`, q4 pointer derivation, 6-bit scale/min unpack, dp4a dot and q8
  sum, per-i accumulation, cross-warp staging and super-block combination;
- 8 checks / 4 cases per arm, 4/4 outputs byte-identical (ncols 256/1024/4096
  and a two-channel 2048), memcheck 0;
- Apex witness in the Goose arms, content sha256 identical;
- paired Apex: prefill +0.21%, decode -1.28%;
- live in the accepted runtime `runtime/exp244/run` (`fg185*` symbols,
  `GGML_GOOSE_CUDA_MMVQ_Q4K` default on).

## Q6_K MMVQ

EXP186:

- the same structure for `mul_mat_vec_q<Q6_K>`;
- 8 checks / 4 cases per arm, 4/4 outputs byte-identical, memcheck 0;
- model witness `FG186Q_CUDA_MMVQ_Q6K paths=0x1` in both Goose runs;
- Apex content sha256 `e68df05b` identical in all four runs;
- **performance rejected**: 21.82 us vs 17.41 us per launch, SASS 536 vs 208,
  paired Apex decode 26.89 -> 25.91 (-3.66%). It is opt-in and not in the active
  chain.

The functional criterion is met. The speed loss is a codegen blocker (address
arithmetic, no `LDG.E.CONSTANT`), tracked with EXP180 in POST-01 and the compiler
feedback; it does not turn a correct transfer into a functional gap.

## Remaining scope

| remaining | assignment |
|---|---|
| 17 other quantized types | C2 compatibility |
| `ncols_dst > 1` | C2 compatibility |
| fusion path | C2 compatibility |
| multi-token ids | C2 compatibility |
| `mul_mat_q` / MMQ family | P3-09 |

`mul_mat_vec_f` F32 (EXP184) is also Goose and byte-exact; it is not the Apex
quantized decode path but remains part of the family.

## Verdict

P3-08 closes on the Apex decode scope: Q4_K is live, Q6_K is byte-exact with a
model witness and a recorded performance rejection. The remaining formats are
explicit C2 work, not counted as Apex coverage.
