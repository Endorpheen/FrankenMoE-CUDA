# EXP-2026-10-30-096: pinned prefix-checkpoint storage

Status: REJECTED (`kind=real-model-qualification`). Control EXP090; current project f078a8f19430b9c60234d42a9b167c047bb02bda. The new>=5% goal remains active.

## Pre-existing hypothesis and protocol

DeepSeek prepared the independent patch and hypothesis in results/opencode-prep/EXP-2026-10-30-096 before application. Replace checkpoint vector storage by a backend host allocation, without extra persistent copies or serialized-format/arithmetic/restore-order changes. Existing measured restore cost7.338/7.931ms is only~4.8% of the older163.405ms repeat TTFT even if removed entirely: an upper bound, not an expected gain. This is not the rejected EXP085 weight-staging ring or EXP089 weight registration.

Following the requested real-model-first screen, apply unchanged to an isolated EXP090 composition and build only affected common/server units. Compare cold32768 and identical repeat with up to128 outputs, seed123, top5 and unchanged sampling/EOS. Reject on failed numerical/resource qualification or no promising>=5% primary repeat-TTFT gain; do not perform formal pairs after clear failure. Old kernels, working builds and defaults stay immutable.

## Observations and verdict

Two test starts/four requests, candidate then fresh control. Repeat client TTFT186.377 ->185.485ms,0.4786% reduction; repeat decode28.50864 ->26.78994tok/s (-6.0287%). Observed VRAM4428MiB both, repeat sampled peak RSS+72KiB. All256 IDs per arm/ordered top5/probabilities exact, delta0. Candidate both requests and control repeat: process swap/system swap-out/major faults0. Control cold has152 system swap-out pages; its timings are not clean performance evidence. This is qualification, not statistical or causal regression acceptance.

No formal series or additional capacity/serialization/Flint/model-free tests after this stop. Actual checkpoint pinning not independently established; no mechanism success claimed. EXP090 restored on8081 with health/nine mapped hashes/120 expert ranges verified. Only diagnostic candidate evidence retained in results/exp096-checkpoint-pinned/. Candidate is not a production feature. Raw-data hashes and complete limitations are recorded there.
