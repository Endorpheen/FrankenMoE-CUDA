# Apex small plan: CLOSED (2026-11-12)

All 15 rows are DONE in `apex-status.json`. Final run on the accepted 32K/128 profile:
`first_sha = e68df05b4829865dcf8cd2424febfaded15b055f56fec36fefc002650ef8e7c9`, probabilities equal, slot restore ok,
`content_equal = true`.

## What is Goose on the executed path

| area | pieces |
|---|---|
| CPU math | Q4_K and Q6_K dots with the dispatched AVX2 association: 160/160 and 240/240 bit-exact, 7/7 and 6/6 mutations, CPU bench run identical (`d13cf4cf`), execution proven by defect tests |
| CPU control | expert lifecycle routes FG143/147/149/150 |
| CUDA control | FG196P pinned policy (executes, `host_plan=2`) and FG195V VMM arithmetic (fixture 5606/0) |
| CUDA matvec | Q6_K matvec: 636 sub-calls = all 324 calls, defect test `e4de6b44` |
| CUDA MMQ | dispatch (8550 calls), Q8_1 quantizer (`paths=0x3`), Q4_K and Q6_K MMA tile loaders (0 mismatches in A/B, defect tests `1a8543b0` and `22ea97d3`) |
| model graph | gating, weight normalisation, top-k selection, w_scale -- four decisions, each defect-tested |
| request/session | slot budget, request-field bounds, context-shift amount, sampler choice -- each defect-tested live |
| everything else (CORE, LOAD, TOKENIZER, KV, RECURRENT_MEMORY, BATCH, OUTPUT) | closed in earlier experiments |

## Boundaries recorded, not hidden

- tensor-core instructions (`mma`, `ldmatrix`), cuBLAS, cuBLASLt, memcpys, the allocator, the HTTP transport and the
  JSON library are primitives by the plan's definition;
- the fused `gate_up_exps` path is dead for this architecture (forcing `if (false)` changes nothing);
- the MMA dot's last multiply was deliberately not routed through Goose (EXP256): the register round-trip costs more
  than the two multiplications it replaces.

## Checkpoints (same profile, one per closed item)

prefill 1658-1689 tok/s, decode 23.0-24.5 tok/s, peak RSS ~22.9 GiB, VRAM attributed 6412 MiB. Recorded in
`metrics/apex-checkpoints.json`; a single quick checkpoint per item is not evidence of a regression or an improvement,
and optimisation is a separate later stage.

## Reproduction

`experiments/EXP-2026-11-08-186-goose-cuda-mmvq-q6k/build_integrated.sh` (CUDA backend) and
`experiments/EXP-2026-11-12-250-goose-server-nremaining/build_server_integrated.sh` (server) rebuild the integrated
state from the flint tree under /tmp. Preserved, /tmp-independent copies with SHA manifests live in
`runtime/checkpoints/`.
