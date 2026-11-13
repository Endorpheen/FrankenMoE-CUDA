# EXP-2026-10-30-092: twelve physical decode workers

Status: REJECTED. New>=5% goal above EXP090; no old gains are recounted. Reference project6b429b40f2b19aa8a1f38be2405d2d5c4375e4aa, engine EXP090.

## Prior-work audit before selecting a candidate

33 Apex tuning result configurations examined. The same Apex model previously tested12 workers/3f3f with35 CPU expert layers and the runtime-default polling policy; it did not test all40 CPU expert layers with poll0, registered mmap experts and the current checkpoint profile. The old all-CPU no-MTP entry used8 workers, unbound affinity/default poll/checkpoints0. No exact all-CPU/12/poll0 combination is found. This is a changed CPU-load/polling combination, not a repeat justified merely by a model name. Older Qwen SMT24/32 and batch-pool12/16 rejections are retained; those variants are not rerun.

## M0 and hypothesis

Keep the exact nine EXP090 binaries, weights, quantization, CPU/GPU decisions, context262144, batch/ubatch4096, batch threads16/maskffff, poll0, sampler/EOS and cache boundaries. Change generation-pool -t8/mask0f0f to -t12/mask3f3f. Topology readout confirms cores0..15 are physical cores, SMT siblings16..31;0..7 share L3A and8..15 L3B. Old mask selects4 physical cores per CCD; new mask selects6 per CCD. No extra SMT workers and no source/kernel change.

Hypothesis: extra physical workers may shorten the all-CPU expert decode path. Upper bound: ideal8->12 scaling gives1.5x on its CPU-parallel fraction; at least1/7 of total latency would need that scaling for5% overall throughput. Actual fraction/scaling are UNKNOWN, and memory-bandwidth/dispatch contention can make this slower. This does not promise a gain. Unlike EXP091, arithmetic/device placement remains unchanged; numerical identity is a regression check, not a claim about semantic quality.

## Minimal probe and acceptance fixed before launch

Reuse BOTH existing EXP091 control responses from EXP090 (cold32K then identical repeat) for numerical comparison and the repeat's27.65243459tok/s as a diagnostic warm-decode reference. Its repeat resource gate passes; its cold's1 page swap-out means that cold timing is not acceptance evidence. Reuse avoids another identical control startup/request pair. One candidate startup/two sequential identical requests, up to128 outputs, seed123/sampling unchanged, n_probs5, EOS respected. Expect cache0 then32764, actual prompt32768 then4. Stop if selected/ordered-top5 IDs differ or probability delta>0.001. Advance only if repeated-request decode shows at least5% diagnostic gain, resource gate passes and no material other-metric regression appears; a single reused-control comparison is not acceptance.

If qualified: five fresh balanced serial matched pairs, identical cold+repeat sequence per arm, primary warm-repeat single-response decode median paired>=5% and>=4/5 wins. Require no median cold-prefill/TTFT regression>3%, preserve measured32K VRAM/RAM and process swap0/request-window swap-out0. Record startup, faults and input-cache split. Existing prefix/capacity/Flint/rollback gates apply before profile promotion. No broad sweep of other thread counts is authorized by this hypothesis. One atomic final result commit; old launchers/builds stay preserved.

## Outcome

Initial reused-control warm-decode screen+5.2819% did not establish acceptance. Repeat TTFT regression triggered one announced fresh control to investigate before promotion. Fresh B-A pair: warm decode28.71499 ->29.11302tok/s (+1.38612%), cold decode+0.85371%; repeat TTFT163.405 ->197.804ms (+21.0514%). Both fresh resource gates pass, all selected/top5/probabilities match exactly. The candidate does not reach5%; no formal series or other-count sweep follows. Two test starts/four requests and one service restoration; working EXP0908-thread profile preserved. Public evidence: results/exp092-cpu-twelve-workers/.
