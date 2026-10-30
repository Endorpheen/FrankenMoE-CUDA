# EXP-2026-10-30-100: host state-transfer batching

Status: REJECTED (`kind=real-model-qualification`). Reference EXP090, projectd3c15de465bf2b310a00363de9982ae8b72f3b71. Independent unchanged DeepSeek patch; no EXP096 dependency. Hypothesis and code prepared before application: batch host get/set operations and synchronize each backend once, preserving state/public synchronous contract. Historical restore-cost upper bound~4.8% of older repeat TTFT; no expected gain claimed.

One candidate start/two real-Apex requests cold32768/repeat, up to128 outputs, seed123, accepted placement/context/sampling/EOS/top5 unchanged. Reuse fresh EXP096 control, stop if no promising>=5% repeat-TTFT improvement. Only libllama changes, EXP090 model object and the other eight runtime files retained.

All256 selected IDs/top5/probabilities per arm exact, delta0. Repeat TTFT186.377 ->195.786ms, observed5.0484% longer; candidate cold14.287778s, single-response decode29.9720/28.8326tok/s. Candidate process swap/system swap-out/major faults0, observed VRAM4428MiB, repeat peak RSS22210240KiB. Control cold has152 swap-out pages, so its cold timing is not clean evidence. No formal statistics or causal regression claim.

Reject after qualification: formal pairs and further serialization/capacity/Flint/lifetime gates skipped, not passed. No promotion. EXP090 restored with health/nine hashes/120 expert registrations verified. Commands, unchanged rejected patch, metrics and raw-data hashes in results/exp100-state-transfer-batch/. New>=5% goal remains unmet; further work will be handed to OpenCode as requested.
