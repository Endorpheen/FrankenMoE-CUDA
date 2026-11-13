# EXP-2026-10-30-092: twelve physical decode workers — REJECTED

No new>=5% improvement is accepted above EXP090. The8-thread/0f0f profile remains working; all old builds, kernels, device placement and model profiles are preserved. The rejection concerns this exact Apex/all-CPU/poll0/12-thread combination, not12 threads on every model.

## Prior-work audit

33 earlier Apex profiles were inspected before any run. The12-thread3f3f variant previously used35 CPU expert layers and default polling; no all40-CPU-expert/poll0/current-checkpoint variant was found. Older Qwen SMT24/32, affinity and batch-pool results were read and not blindly repeated. The new candidate selects six physical cores per CCD rather than four; no SMT siblings are added. Batch pool remains16/ffff.

## Qualification and reused evidence

The candidate reuses the exact EXP090 executable and all nine libraries, changing only -t8/--cpu-mask0f0f to -t12/--cpu-mask3f3f. No kernel, arithmetic, quantization, context, checkpoint, sampling or CPU/GPU offload change.

First minimal screen reused the already collected EXP091 control cold/repeat outputs instead of launching another identical control. One new12-thread startup/two cold32K/repeat requests gave a diagnostic warm-decode27.6524 ->29.1130tok/s (+5.2819%), with slightly slower repeat TTFT. This reused-control comparison was not acceptance evidence. Both numerical cases matched exactly.

Because control timing was from an earlier block and repeat TTFT regressed, one fresh8-thread startup/two identical requests was announced and performed to investigate. This fresh B-A diagnostic pair shows:

| Metric |8 threads|12 threads|Change|
|---|---:|---:|---:|
| Cold decode, single response |29.7668tok/s|30.0209tok/s|+0.8537%|
| Warm-repeat decode, primary |28.7150tok/s|29.1130tok/s|+1.3861%|
| Cold client TTFT |14.333423s|14.450253s|+0.8151% time|
| Repeat client TTFT |163.405ms|197.804ms|+21.0514% time|
| Repeat peak RSS |22209956KiB|22209984KiB|+28KiB|
| Post-request observed VRAM |4428MiB|4428MiB|0|

All256 IDs per arm/ordered top5/probabilities match exactly, delta0; cache splits0+32768 and32764+4. Both fresh arms have process swap0/system swap-out0/major faults0. No semantic quality degradation is claimed. Observed VRAM is a point reading, not a sampled peak. These are qualification observations, not five-pair statistics and not proof that every12-thread workload is slower.

The fresh primary gain is below5%, and repeat TTFT also fails the no-regression screen. Stop without a five-pair series or arbitrary16/24/32-thread sweep. Preserve both initial and fresh results; do not select the earlier favorable control to claim a win. Two test starts/four requests in this experiment, plus one service-only restoration of8-thread EXP090 on8081. All nine mapped runtime hashes verified;120 expert ranges registered, no extra test request.

## Reproduction and retry condition

Read the experiment card for the pre-stated protocol and the audit/control provenance. Raw SSE, traces, resource samples and argv are hashed in local-evidence-manifest.json; prompts and model weights are excluded. No candidate runtime or new default is shipped. Revisit only if the CPU expert implementation or meaningful compute/memory/dispatch balance changes; a new model name alone is not a reason to repeat this combination.
