# EXP169: shared CUDA reductions and actual fused Apex MoE routing

Status: functional scoped acceptance;single preliminary whole-system measurement;not full Apex/source closure or stable C++ performance parity. Source checkpoint7959318 and its compatibility-only receipt are preserved. The first Apex request bypassed standalone reductions;the second request after fused-routing integration proves actual new Goose numerical execution without disabling fusion.

## Ownership and producer

Current full self-hosted Fried Goose Stage3 f849233b999c emits separate host C and device POD functions. Mechanical CUDA qualification retains emitted arithmetic,indexing/control and fixed local arrays. No legacy C++ Goose compiler or source/compiler edits. Four reduction TUs(sumrows,mean,argmax,sum),two raw helpers and the fused topk-MoE TU are replaced in an isolated CUDA DSO. Goose owns row reductions/eight-accumulator order,warp/block reductions,tie/NaN/index rules,host projection/checked planning,CUB lifecycle,softmax/sigmoid/sqrt-softplus,group mask,expert selection/normalization/delayed-softmax and stores. Scalar CUDA ISA,storage,launch/PDL and external NVIDIA CUB large/device-wide sums remain explicit dependencies. Private original implementations are hidden rollback/oracles;not removed yet.

## Checks and fixes

326native paired reduction GPU cases/15977360values/5028independent checks/8graph cases pass;24bad-input/pool failures reject before effects.1436routing cases/2496independent oracle rows/16eligibility cases,original rollback and30before-stream rejections pass. Exact finite native comparisons retain104explicit NaN-policy cases.16small-expert NaN cases repair68native invalid-ID rows:inactive warp lanes must not beat valid sanitized NaNs. Native bug evidence remains local. All candidate IDs valid/distinct;do not label native invalid IDs as correctness. Output guards/input bytes checked. No microbenchmark,timing series or broad sanitizer run.

Useful GLM drafts retained with parent fixes:raw byte vs element pointer bases,sqrt-softplus missing sqrt,no-bias selection state,real public-argument ABI instead of fictional node fields,fixture4-byte loops/padding metadata,independent-oracle scope and actual host checks. Static GLM host audit is supplementary only;its blanket claim that a block barrier makes arbitrary output/logit aliases safe is not a cross-block safety proof. Arbitrary shifted/compacting aliases are not a claimed supported contract;Apex uses the measured actual graph layout. Host validation checks tensor-declared spans,not provenance of arbitrary fabricated CUDA pointers.

## Real Apex gate

One fixed32K/128 request after real routing integration:2645.921prefill/28.038decode;exact text,all128token IDs and complete saved probabilities;127graph reuses;RSS21329.273MiB,VRAM3246MiB,swap0;all owned exit heaps0. Actual FG169T host0x3,experts0x100(E256),flags0x9(normalized softmax),cuda_status0. The new fused math executes inside Apex;standalone reductions are local-GPU tested only. Relative to accepted EXP168:-0.080percent prefill/+1.812percent decode. Preliminary uncontrolled sample,not stable acceleration/fresh-C++ parity. Saved C++2918.900/29.589 remains archive only. Raw requests/responses/worker conversations stay local.

## Build,recovery and limits

Run `python3 build.py`,then `python3 build_topk.py`,then `python3 check.py` and `python3 check_topk.py` here. The first builder alone selects the reductions-only DSO;the second creates/selects the combined runtime. Qualification receipt pins the exact measured DSO hash;do not overwrite immutable measured binaries. `run_apex_topk.py` is a single-use gate with distinct immutable receipts,not a benchmark loop. Reproducing already-owned result directories requires an explicit new destination. Whole-stage rollback uses immutable EXP168;per-module disable flags GGML_GOOSE_CUDA_REDUCTIONS=0 and GGML_GOOSE_CUDA_TOPK_MOE=0 restore original implementations.

Verified version-map inspection hides internal fg168/169/169t FFI and native oracles;EXP168 historical export-check incorrectly claimed all internals hidden and is corrected as metadata only. Remaining sort/argsort/cumsum/count,CPU/CUDA/provider/model/KV/loading/server and native removal stay open. P3-05 is IN_PROGRESS,not DONE. No production or compiler changes.
