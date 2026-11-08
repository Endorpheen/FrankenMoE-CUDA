# EXP173: coherent Apex FlashAttention on current self-hosted Goose

**Functional Apex check passes; material performance regression remains. Not activated.** Immutable EXP172 is still the active experimental runtime and rollback. Production and compiler sources are unchanged.

Goose now implements actual D256/QF32/KVQ4_0 vector decode, Q4 cache staging, async loaders, TensorCore prefill iteration/process/stream-K distribution, mask scan, both fixups/combine and host validation/launch/buffer planning. Raw CUDA ISA, SDK launches/properties, tensor field access and existing pool storage effects are explicit. Original numerical kernels are never launched on the enabled in-scope path. Other head/types/architectures remain explicit compatibility fallbacks.

Current numerical planning additionally queries the original private kernels' **SDK occupancy metadata only**. This is a temporary native-artifact dependency, not native arithmetic execution, and must be removed before source closure. Actual register use differed: original vector255registers gives2blocks/SM, Goose79/110registers gives6/4;MMA remains2/2. Replanning with the new occupancy changed floating-point partitions. The short periodic corpus missed this; a 32K randomized-KV case reproduced the difference. Preserving the original partition fixes the random corpus and actual Apex without changing any comparison tolerance. No artificial model math fallback is used.

## Checks and retained failures

The earlier component checkpoint is preserved in commit97ca220. Additional process8 and whole-MMA/stream-K8 native-bitwise cases pass. Whole ordinary API has8default+8original-rollback cases with one capture per mode and real host/device witnesses. The improved nonperiodic,non-dyadic,long-KV corpus has8default+8rollback cases and one capture per mode. Host has27mocked cases,including original GQA/scratch/effect ordering,locks,bounds/alias failures and the actual2GiB unused mask singleton stride. Mocking does not prove concurrency. Loader/staging guards do not prove all read-only bounds. A concrete ldmatrix-address failure previously justified a local memcheck;its corrected iteration corpus reports0errors,not a repository-wide sanitizer claim.

A startup initially failed before inference because an unused mask nb32>INT_MAX was rejected despite ne2==1. The guard was corrected and its real reserve metadata was added to host tests;only host C/relink was needed. The next complete response differed from token19 because of the occupancy/partition change. Both failures and diagnostic receipts are retained. There were one failed startup and two completed model requests,not a performance series.

## Actual Apex32768/128 after partition fix

2167.819prefill /22.639decode tok/s;all128tokenIDs,text and every saved top5log-probability match the unchanged C++ reference;127graph reuses;actual FA host/vector/prefill/staging/mask/fixup witnesses read successfully. See apex-summary.json for RAM/VRAM and exact deltas. EXP172 was2603.792/27.402. The slowdown is material in this preliminary sample,so speed acceptance and activation are withheld. External host/GPU applications and two authorized external workers were not stopped. C++2918.900/29.589 is a saved comparable archive,not a fresh paired control. No isolated operation timing or microbenchmark was performed.

SDK metadata observes increased MMA local storage(528B vs16B per thread) and vector dynamic-loop/register differences. Their contribution to whole-system slowdown is a hypothesis requiring efficient Goose-code changes and a new system gate;do not attribute all slowdown to the compiler without evidence.

## Reproduction and state

`python3 verify_generated.py` verifies all8modules against the pinned complete self-hosted Stage3 f849 and checks mechanical compute-header bodies. `python3 build_runtime.py` verifies this source,builds one isolated CUDA/host candidate,reproduces the unchanged EXP172 link SHA,then privately renames the original host object and links the replacement. It does not activate a runtime. Existing component checks remain available. Generated compute and raw adapters are tracked;no old C++ Goose compiler is used. Raw model requests/responses and worker conversations stay local.

Successful actual-model receipt is runtime/exp173/results/apex-32k128-post-partition-fix. Earlier startup and response directories are preserved. Runners refuse to overwrite an existing receipt. Use GGML_GOOSE_CUDA_FA=0 or immutable EXP172 for rollback. Public exports are only the established backend API and two readonly FA diagnostics;internal FFI/oracles are hidden.

No universal requirement,Apex batch,full-Apex migration or native deletion is claimed complete. The next work is removing the observed whole-system regression while retaining exact numerical partition and closing remaining Apex runtime paths. Two Command Code workers now use deepseek/deepseek-v4.1-flash in separate directories;their SET_ROWS drafts have not been integrated. The GLM outputs/history are retained;switch back only if reviewed results are clearly materially worse.
