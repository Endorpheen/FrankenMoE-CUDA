# Edge0 decode profiling and optimisation (EXP920)

Control: immutable EXP916 first-answer checkpoint, commit d02a274a1e3aa03e7dc7d1459d191229bf7f1d7a.
First step is measurement, not an assumed I/O or compute bottleneck.
Instrument positioned expert/D-U reads, PLE provider, CPU tensor copies from GPU,
Goose routing, graph construction, GGML CPU execution and full FFN wall time.
Profile prefill and decode separately. Record process read_bytes/rchar/syscr.
No application retention caches exist in the control, so initial hit counts are0.
Nsight captures CUDA activities for the first16 real decode evaluations; the
answer continues with ordinary EOS and max256 predicted tokens. The trace's
wall time cannot establish unprofiled throughput. CPU compute includes native
GGML work-buffer and threadpool preparation; report that scope explicitly.
Do not add category percentages across overlapping GPU/host intervals.

Fixed longer workload: explain in Russian, in8-10 connected sentences, why RAM
and VRAM are needed during local language model inference and what happens
when weights do not fit. ctx512, b/ub1, t8, ngl48, MTPoff, greedy unchanged.
One process, fifteen-minute wall limit and12GiB RSS guard. Observe process swap;
stop on owned swap. Save token IDs and normal EOS. No Apex tests during this work.

After identifying a measured mechanism, record the optimisation hypothesis,
Amdahl upper bound and memory budget before implementing it. Reuse accepted
Goose math, descriptors and providers. Retain the control and change candidate
files only. Compare exact IDs/text with the control on the identical workload,
then measure unprofiled paired performance and report prefill, decode, RSS,
VRAM, I/O and cache metrics. An insufficiently long naturally stopped response
is an observation, not a reason to suppress EOS.
