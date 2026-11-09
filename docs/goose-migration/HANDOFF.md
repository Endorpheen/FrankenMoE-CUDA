# Handoff: what is ready, what is not, and what installing would mean

Written for Codex, who owns the final install. Everything below is measured on the artefacts in
this repository; nothing here asks the reader to trust a summary line.

## The candidate runtime

    stage       EXP196
    path        runtime/exp196/run
    DSO         libggml-cuda-pinned.so.0.27.0
    sha256      466a33342f5abfa7…  (build/ and run/ copies verified identical)
    commit      2b38c2e
    rollback    runtime/exp195/run, and every module has its own switch below

Contents: the EXP185 lineage plus the VMM pool arithmetic (EXP195), the pinned-memory and
host-registration policy (EXP196), and `sample_ratio` in both matvec bodies.

## Verified on this exact DSO

    Apex 32K/128, seed 123, fixed profile
      tokens_equal                    True     (all 128)
      content_equal                   True     sha256 e68df05b…
      completion_probabilities_equal  True     (all 128 entries, 5 top_logprobs each)
      prefill 2558.74 / decode 26.07  at load average 1.51

    test-backend-ops test -b CUDA0    rc=0, 0 failures, 2/2 backends passed   (18753 cases)
    compute-sanitizer                 memcheck / initcheck / synccheck / racecheck clean
                                      across every stage that ships a GPU fixture
    per-stage gates                   all green after the last rebuilds
                                      (regression-sweep-20261108.md)

## Rollback

Every module keeps a switch; setting it to `0` restores the native body for that module only:

    GGML_GOOSE_CUDA_{CONCAT,GET_ROWS,CPY,Q8_1,MOE_WEIGHTED,MM_IDS,FWHT,MMVF,MMVQ_Q4K,
                     ACTIVATIONS,ROPE,SET_ROWS,SSM,GDN,TOPK_MOE,NORM,REDUCTIONS,DEQUANT_Q4K,
                     BROADCAST,VMM_POOL,PINNED}

The VMM and pinned switches were each verified to restore the native path by the witness going to
`alloc=0 free=0` and `native=N` respectively. Per-module rollback is therefore the first response
to any problem after install, before anything else is considered.

## What is not done, stated plainly

- **`mul_mat_q` (40.0% of kernel time) and `quantize_mmq_q8_1` (~5.7%) are still native.** Both
  are byte-exact ports that are slower, blocked on the emitter passing addresses as `int64_t`,
  which stops nvcc folding `base + i*k` into a load's immediate offset. Four measured attempts,
  recorded in EXP180/EXP186. This is a compiler change and is out of bounds for this work.
- **52.8% of kernel time is Goose, 47.2% native**, and 45 of those points are the two families
  above. The long tail holds no unported work (remainder-audit.md).
- **P3-20 is not closed.** The link still carries 119 untouched backend objects plus 12 privatised
  rollback objects (link-closure-measurement.md). Claiming otherwise on the strength of the 12
  would be wrong.
- **`test-backend-ops grad` reports four SUM failures.** They reproduce with every Goose switch
  off, so they belong to the baseline (grad-mode-open.md).
- **The census on the current runtime was not re-taken.** Two profiler attempts failed on the
  runner's integrity check rather than on capture; the numbers quoted come from the exp175 trace
  with those limits stated (census-full.md, census-split.md).

## Two measurement defects found here, in case they recur

1. `GGML_GOOSE_CUDA_$k=0 cmd` is not a POSIX assignment, so the command never ran and `grep -c`
   reported zero. It produced a false attribution that was retracted.
2. A capture of the native route synchronised `ctx.stream()` while the work completed on a
   different stream, so the reference dump was garbage and a correct kernel looked wrong.

Both are documented with the controls that exposed them. The general lesson recorded in those
files: check a claim against a control arm before building on it.

## Reading order

1. `docs/goose-migration/census-status.md` — current state, top section
2. `docs/goose-migration/final-coverage.md` — kernel by kernel
3. `docs/goose-migration/correctness-evidence.md` — what is actually compared
4. `docs/goose-migration/test-backend-ops-failures.md` — the five defects and their fixes
5. `GOOSE_MIGRATION_PLAN.md` — per-task status with evidence
