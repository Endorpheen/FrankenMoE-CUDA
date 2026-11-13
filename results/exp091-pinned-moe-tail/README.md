# EXP-2026-10-30-091: pinned MoE small-batch offload — REJECTED

This candidate is not accepted toward the new>=5% goal above EXP090 under its original numerical-equivalence gate. It is not a baseline. No rejected feature patch is shipped; the isolated experimental build remains under /home/end0/AI/LlamaServer/exp091-pinned-moe-tail for local diagnosis only. The working EXP090 runtime is restored on8081, all nine mapped hashes verified,120 expert ranges registered. Qwen3.8's separate EXP063 launcher and installed application profiles are untouched.

## Qualification and stop gate

Two serial test starts, three requests: EXP090 cold32K/repeat; EXP091 cold32K. Candidate repeat and five-pair acceptance are skipped after the numerical gate fails. One additional service startup restores EXP090 without test requests.

- First ordered-top5 divergence: position0. Control first top5 IDs198/271/220/256/695; candidate198/271/220/256/2228.
- First selected-token divergence: zero-based65 (the66th emitted token).
- Maximum probability difference on common IDs through the still-shared input histories:0.2053055685. The legacy comparator reports0.0718894568 on matching whole-record IDs, including possible positions after histories diverge; do not use it as the shared-context error magnitude.
- Control cold and its repeated prefix produce exact selected IDs/ordered top5/probabilities, delta0. Control also exactly matches the checksum-preserved accepted EXP089 cold result. The failed candidate cannot be explained by an observed control inconsistency.
- Client TTFT: control cold14.529798s, repeat184.993ms; candidate cold14.389025s. These are qualification observations, not accepted speed evidence; no candidate-repeat timing exists.
- Observed post-request model VRAM4428MiB in both, not a sampled lifetime peak. Process swap/major faults0. Conservatively bracketed system swap-out is1 page for control cold,0 for control repeat and4 pages for candidate cold. The cold resource gates therefore also do not pass and are not waived. Numerical failure already stops promotion.

The physical small-batch predicate activated. The server log first checkpoints32764 tokens, then logs pinned small-batch offload before first output. Existing checkpoint logic splits even the cold prompt's final chunk into4092+4. Moving that4-token tail from CPU to CUDA changes the numerical route, despite identical weights and quantization. This gate concerns preservation of numerical behavior; it does not measure semantic answer quality. Output divergence alone does not establish worse answers. Semantic evaluation is deferred, and the next investigation uses a different mechanism.

## Exact candidate scope

Default legacy threshold32 remained for all operations except CUDA MUL_MAT_ID with4..31 tokens, host source weights and actual registered-host pointer attributes. Explicit threshold overrides, pageable weights,1-token decode and HIP/MUSA retained old decisions. Only libggml-cuda changed; eight other runtime components matched EXP090. Reused kernel objects and the rebuilt master device fatbin remained identical. Identical kernel bytes do not imply equivalent graphs/placement.

Source hashes, build logs/commands and rejected local patch hash are retained. Raw SSE/traces/resources and official Apex model/GGUF/generation configuration reads are hashed in local-evidence-manifest.json; prompts, model weights and private instructions are excluded from the public artifacts. Existing launch-control/candidate files are reproduction commands for this rejected experiment, not accepted defaults.

Retry only if CPU-equivalent arithmetic can be preserved while moving this tail; do not sweep other small thresholds or loosen output/top5 tolerances. The next different direction may investigate CPU worker utilization with placement unchanged, subject to checking prior rejected combinations first.
