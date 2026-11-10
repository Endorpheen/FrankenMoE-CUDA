# Handoff: what is ready, what is not, and what installing would mean

Written for Codex, who owns the final install. Everything below is measured on the artefacts in
this repository; nothing here asks the reader to trust a summary line.

## The candidate runtime

    stage       EXP220
    path        runtime/exp220/run
    DSO         libllama-exp220.so.0.7.0
    commit      c910402
    prefill     2567.91   decode 26.47   (medians of 3 runs, 2 interleaved pairs)
    correctness 128 tokens and all 128 saved probabilities exact, sha e68df05b
    memory      RSS peak 21.3 GiB, VRAM peak 4590 MiB (transient; ~3348 steady), swap 0
    rollback    runtime/exp219/run and earlier; every module has its own env switch

Contents: 62 Goose witness modules in one Apex pass — the EXP185 lineage, the KV cell/set/find/cp/
seqpos work, the loader and graph builders, the tokenizer text layer (piece copy, UTF-8 decode,
encode, whole-string recovery) and the CUDA kernels up to MMQ dispatch.

**Read this before trusting a stage name on an older document:** EXP213 through EXP218 each shipped
with only *one* route in `unicode.cpp` and `llama-vocab.cpp`, because each copied the pristine native
file instead of the previous integration. Two stages silently lost components that had millions of
calls, while tokens, probabilities and speed all passed. EXP219 and EXP220 are the repairs. The rule
that catches this class before it ships:

    python3 scripts/check_witnesses.py --run <new> --baseline <previous>

**with `--baseline`** — an empty baseline can never fail, and running it without one is a no-op that
prints `verdict: OK`. Also `nm -u runtime/<stage>/build/*.o` for the expected route prefixes before
linking, not only afterwards. `docs/goose-migration/route-audit-2026-11-10.md` audits all 23
declared prefixes against the active runtime; every one is live, deliberately absent or rejected.

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


## MMQ: byte-exact, not promoted, cause of the 13.5% not established

`quantize_mmq_q8_1` is ported and byte-exact (4/4) but runs 13.5% slower at the operator level:
goose 2055/2103 ns against native 1812/1842 ns, 2000 launches per arm, under 1% spread within an arm
(EXP221 README). Not promoted.

Three explanations were proposed and each was wrong: the "824 vs 160 instructions" figure compared
the sum of two kernels against one non-DS4 instantiation; "both paths in every instantiation" is
false, the wrappers pin `ds` and `scatter` as constants; "not instruction-bound, so occupancy" is
unsupported, there are no spills and register counts are comparable. Corrected numbers: 432 (direct)
and 392 (scatter) against 168 and 264.

The metric that would settle it is unavailable here — `ncu` returns `ERR_NVGPUCTRPERM`, and
unblocking needs root or `NVreg_RestrictProfilingToAdminUsers=0`, a system change. The exact command
is in the EXP221 README for whoever has the permission.

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
