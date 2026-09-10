# The decision on a checkpoint after a full prefill — 2026-09-08

Status: Igor's decision is recorded. A functional compromise has been identified;
the senior helper's recommendation is to keep the current checkpoints.
Igor approved the recommendation and instructed to preserve the latest findings.
Moving the checkpoint to after the full prompt is not accepted for implementation.
This is a decision about functionality, not the result of a performance A/B.

## Hypothesis and expected benefit

Remove the 391+4 prompt split, process it as a single whole GPU batch, and save the
checkpoint after decode. The possible benefit is a shorter wait for the first response.
EXP-043 attributed about 129.5 ms to the four-token CPU pass,
and 16.3 ms to the checkpoint save. The last four tokens still have to be computed,
and the checkpoint save remains. These numbers were obtained under the profiler and
are not a measurement of the candidate's speedup.

## Results of the local agent's investigation

The author of the investigation is the local agent; Igor passed its report to the senior
helper for review. The report's conclusions are preserved below together with the senior
helper's caveats.

- In EXP-041 the checkpoint is created before the decode of the final batch. The offsets
  `{4 + n_ubatch, 4}` give points before the end of the prompt; the nearest one does not
  include the mutable tail of the template. Restore checks the usability of the checkpoint
  position relative to the divergence point and the SWA/hybrid state constraints.
- A checkpoint after the whole prompt may end up later than the divergence point when the
  end of the request changes. Then it is unusable; if there are no other suitable points,
  a full recomputation of the prompt is required. This is a conditional scenario,
  not a mandatory outcome of every dialogue.
- According to the report, the EXP-043 raw-completion request contains no
  `message_delimiters`; a checkpoint at a user-message boundary does not serve as a backup
  for it. For chat, the presence and usability of such boundaries must be verified
  separately.
- Removing the deep break also loses the spare point before the last ubatch.
  The losses of the shallow/deep checkpoints are different; keeping one does not
  guarantee a replacement for the other. The state before the tail cannot be obtained by
  simply saving only the final state of the whole decode.
- A possible implementation would need to save consistent target, draft, and speculative
  state after the corresponding draft processing and before sampling. This is an
  implementation requirement, not a passed correctness gate.

## The primary source cited by the local agent

The local agent reported the following MarkShark2/llama.cpp commits:

- `59b26b6170e4638c8bf50346ca349638d2878f08` — removing the deep prompt break
  by default, option `LLAMA_CKPT_DEEP_BREAK=1`.
- `bc33554dde4d12a2e0c94173aa4d38c628ebed93` — saving the checkpoint after
  decode, variants `LLAMA_CKPT_PROMPT_BREAKS=0|1|2`.

According to its report, the measurements concern DeepSeek-V4-Flash, a 10-stage RPC
pipeline on 8× BC-250 and shredder x2, a 15360-token prompt: 180.3→205.4 tok/s
for deep break and 180.3→239.8 tok/s for the deferred save. The numbers are given
in the commit messages; a reproducible testbed is not presented in the report.
The senior helper did not re-verify the external commits when preserving this record.
The claimed +13.9%/+33% do not carry over to our single-GPU runtime:
RPC-pipeline drain and CPU processing of a short MoE batch are different causes of cost.

## The senior helper's caveats

1. The 110–125 ms saving estimate is scenario-based, not a proven upper bound.
   The increment of unique experts for 395 instead of 391 tokens has not been measured;
   scaling the total kernel time by the 4/395 proportion is not justified.
2. "Zero replayed tokens after restore" is not confirmed: it must be
   reconciled with `TAG_PROMPT_LOGITS` and the need to obtain logits.
   Exact replay on a live context and restore from a checkpoint are different paths.
3. A full recomputation arises when there are no other usable checkpoints.
4. The correctness of short, multi-batch requests and of target/draft restoration
   was not verified by an implementation. The wording "there are no uncovered facts"
   was not accepted.

## The outcome and the boundaries of the decision

Igor approved the senior helper's recommendation: keep the current checkpoints.
The small unconfirmed prefill gain does not justify the identified loss of
restore points. No additional runs are required for this decision.

The EXP-041 runtime, launcher, offload threshold, prompt split, and checkpoint
settings are unchanged. This record's impact on RAM/VRAM and execution
correctness is nil. The server, model, build, and tests were not launched when
this was recorded. This is a conclusion separate from the earlier prepared EXP-044
about the tail of 32. The next experiment is not assigned. Permission to commit
this record was not requested separately, and no commit was made while preparing it.
