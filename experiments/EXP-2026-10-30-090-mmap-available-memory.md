# EXP-2026-10-30-090: remove the raw-free-memory veto

Status: DELIVERED (`kind=delivery-fix`), explicitly requested by Igor after EXP089 fell back on his manual launch.

## Hypothesis and scope before changes

EXP089 requires both MemAvailable>=registered_bytes+8GiB and MemFree>=8GiB. The second condition rejects reclaimable file cache: observed MemFree0.76GiB, MemAvailable48.87GiB, Cached46.85GiB, despite the already mapped expert weights needing no second anonymous copy. Remove only this raw-free-memory veto. Retain the existing available-memory headroom, half-physical-RAM limit, resident-page check, supported-mode checks, registration rollback and explicit disable flags.

Preserve EXP089 sources/binaries/results unchanged. Build the fix separately under /home/end0/AI/LlamaServer/exp090-mmap-available. Change only the budget helper and its caller; no arithmetic, kernels, tensor placement, batch sizes or cache policy changes. Rebuild only the model object/libllama and reuse the exact remaining runtime components. Update the existing model-free budget tests and run their rollback/exception checks with ASan/UBSan. Verify incremental patch replay and runtime component hashes. No model launch, restart or new speed claim in this delivery; Igor controls his running server and will restart manually. EXP089's five-pair measurements retain their original scope.

## Result

Removed the MemFree veto and its input. Available-memory headroom and physical-capacity limits remain. Compilation/linking and existing ASan/UBSan budget/ownership checks PASS; exact patch replay PASS. Only libllama changes. No model runs or server restarts; the running server and EXP089 remain untouched. No new timing/resource claim. Delivery and launch profiles: results/exp090-mmap-available/.

## User manual launch

Igor supplied the subsequent manual-launch log:120 mapped expert ranges registered (18600.47MiB), no weight copy, EXP087 lazy reservation active, context262144 and server ready on8081. Activation is confirmed. No new performance inference or test was initiated by the assistant. See results/exp090-mmap-available/manual-launch.json.
