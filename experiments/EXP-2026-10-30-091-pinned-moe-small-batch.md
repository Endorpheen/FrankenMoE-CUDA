# EXP-2026-10-30-091: pinned MoE small-batch offload

Status: REJECTED. Goal: a NEW>=5% improvement above EXP090 (project28a854aac223e930336b780e4511d60bf817c46b). EXP089's prior61% prefill gain is not counted again.

## Reference and M0

Read-only process inspection finds PID2708064 on8081 executing /home/end0/AI/LlamaServer/exp090-mmap-available/build/bin/llama-server; all nine mapped runtime hashes match results/exp090-mmap-available/runtime-manifest.json. Current profile: Apex Q4_K_M, context262144, CPU MoE, FA/Q4_0 KV, batch/ubatch4096, threads8/16, masks0f0f/ffff, strict1, poll0, one slot, cache-ram0/checkpoints32, mmap, speculation off. User supplied a successful120-range expert registration log. EHS remains off. Installed profiles and the live server remain untouched.

The legacy scripts/run_qwen38_server.sh selects EXP063 for a different Qwen3.8/MTP/IQ3 profile. It is not this Apex reference, and must not be switched to an unvalidated runtime for that model. The external profile catalogue also contains earlier commands; neither overrides the measured process identity.

CPU expert weights remain host buffers, physically registered by EXP090 (19503513600 logical bytes); GPU owns the remaining model tensors, full configured KV and recurrent state. Scheduler offload is consulted for operations whose weights are host-backed. CUDA currently requires get_op_batch_size>=32; MUL_MAT_ID uses token count ne[2]. Thus large prefill already runs on CUDA, ordinary1-token decode stays CPU, and the recurrent checkpoint tail of4 tokens also stays CPU. GPU/CPU moves go through the existing scheduler; this candidate changes placement only for that intermediate MoE case, not quantization or checkpoint boundaries.

MEASURED HISTORICAL (not a fresh EXP090 benchmark): EXP089 gateApexrepeat has cache32764/prompt4, prompt172.154ms after registered-weight loading. Therefore roughly8.6ms saved in a similar processing interval could suffice for5%; actual acceptance uses CLIENT TTFT. ESTIMATE: faster registered H2D may change the CPU/GPU crossover; gain and transfer cost remain UNKNOWN. No bandwidth or timing estimate is presented as a measurement.

## Why revisit the previously rejected direction

EXP058 rejected a GLOBAL threshold4 on a different82GB Qwen3.8 IQ3/MTP runtime and included a changed decode completion. Do not repeat that policy. Conditions changed: Apex has registered in-place expert pages, no MTP, and this candidate lowers the threshold ONLY for CUDA MUL_MAT_ID with actually registered host source weights and4..31 tokens. Other operations, pageable weights and1-token decode retain legacy decisions. Explicit GGML_OP_OFFLOAD_MIN_BATCH settings retain precedence. This is the exact changed condition permitting one bounded revisit, not an arbitrary sweep.

## Candidate and gates fixed before implementation

Separate source/build/bin only under /home/end0/AI/LlamaServer/exp091-pinned-moe-tail. Add a narrow CUDA offload predicate: original>=threshold decision first; otherwise require legacy default32 with no explicit threshold override, feature not disabled, MUL_MAT_ID, batch>=4, host weight buffer and successful cudaPointerGetAttributes==cudaMemoryTypeHost. HIP/MUSA keep the legacy predicate. Disable with GGML_CUDA_PINNED_MOE_TAIL=0. Preserve all EXP087/089/090 code and binaries. Only rebuild the CUDA master host-dispatch object/shared library; reuse every device kernel object and all other runtime components.

CPU and CUDA can accumulate differently. Numerical equivalence is UNKNOWN until tested; identical weights/quantization are not proof. First future qualification: two serial fresh starts (EXP090 and candidate), cold32K then identical repeat, up to128 outputs, seed123/sampler unchanged, EOS respected, top5 probabilities. Require cold cache0 and repeat cache32764/prompt4; exact selected/ordered-top5 IDs and probability delta<=0.001. Stop on mismatch; do not relax the gate or change checkpoint shapes. Confirm the candidate activation log before attributing anything to the mechanism.

Only after qualification: five balanced manual matched pairs (AB,BA,AB,BA,AB), same two-request sequence per arm. Primary: repeat client TTFT median paired reduction>=5%,>=4/5 wins; preserve context, old optimizations, primary32K memory and no median decode/cold-prefill regression>3%. Require process swap0/request-window system swap-out0, record faults/RSS/VRAM and startup separately. Further prefix/capacity/Flint/disable correctness gates are required before promotion. Do not claim full-request or aggregate-throughput gains from TTFT alone.

## Build outcome

Candidate built without tests/model starts. Runtime hashes, source patch and explicit control/candidate launch commands are in results/exp091-pinned-moe-tail/. No default launcher was switched. Correctness/performance remain UNKNOWN; goal remains active.

## Qualification result and closure

Two serial starts/three requests complete; cold candidate fails the unchanged numerical gate. First top5 divergence0, first selected-ID divergence65; shared-history common-ID probability delta0.2053055685. Control cold/repeat and accepted EXP089 control agree exactly. Existing cold processing already creates the32764 checkpoint and a final4-token chunk; the candidate changes that tail too. Candidate repeat/formal performance are skipped. Both cold resource windows have nonzero global swap-out (1/4 pages), process swap0/major faults0; no gate is waived. One extra service startup restores EXP090 on8081 with all nine runtime hashes verified. Evidence and limits: results/exp091-pinned-moe-tail/. No rejected feature patch is promoted.

## Interpretation and follow-up

The verdict rejects promotion under the original numerical-equivalence gate. Different generated sequences alone do not prove lower semantic quality. Task-solving quality was not evaluated, and that separate investigation is deferred. The next direction changes CPU worker utilization while retaining CPU/GPU placement and arithmetic.
