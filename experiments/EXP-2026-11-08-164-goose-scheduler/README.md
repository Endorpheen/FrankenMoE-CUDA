# EXP164: actual shared backend scheduler in Goose

Solo continuation from immutable EXP163,not EXP153 worker code. All37normal scheduler/layout entry bodies select actual self-hosted Goose by default. The implementation owns placement and buffer compatibility,CPU/GPU expansion/upgrades,split boundaries,input copies/parallel slots,ordered allocator lifetime dependencies,reservation/reallocation,compute/eval/copy/event ordering,expert-pipeline orchestration and scheduler-owned state. This closes functional P4-09 and P4-11;it does not close the full Apex migration or native-source removal.

## Logic,ABI and ownership

`scheduler.goose` performs the algorithms. The separate generated C unit calls explicitly declared typed C/C++ effects;it never delegates scheduling to the old C++ scheduler. The bridge projects POD fields,pointer/int arrays via memcpy,raw allocation/free,selected callbacks,formatting and already accepted Goose core/allocator/backend APIs. Existing provider numerical kernels remain unchanged. The original scheduler is retained only behind cached `GGML_GOOSE_SCHEDULER=0`;previous EXP163 runtime is independently preserved.

The public scheduler pointer remains opaque. Private capacity and transient-owner fields are appended only inside this translation unit;original fields retain their order. A hash-indexed ordered dependency chain preserves per-until insertion and deduplication,without repeatedly scanning every dependency. Checked capacities bound graph ID arrays and metadata construction. Temporary dependency/candidate storage stays attached to the scheduler until released. Constructor-owned allocation failures return NULL and clean all published partial resources. Abort-style failures inside previously accepted components retain their existing semantics;this is not a promise of transactional recovery from arbitrary external exceptions or terminal allocator failures.

Events remain owned by their originating device. Parallel copies preserve source INPUT/OUTPUT flags and slots. Synchronous compute synchronizes even after a returned error. Eval callback stop affects its inner split loop as in native. Copy-input fallback preserves exact native callback/wait/source-sync ordering rather than invoking generic asynchronous copy with different synchronization. Expert QUERY/RESERVE/BIND/BEGIN/END/CANCEL policy is Goose;provider effects remain explicit.

The unchanged EXP163 base relinks to its exact SHA before replacing one backend-control object and adding the new generated C object. CPU,Cuda and libllama artifacts are inherited unchanged. Production exports only two read-only witness/ownership counters;allocation injection and private snapshots exist only in full-object test images. No compiler or production source/build/profile changed.

## Local correctness and memory checks

Independent native EXP163 and candidate base images use separate loader namespaces. The native control has no FG164 witness.44placement/reserve/recompute modes compare18661ordered provider callbacks,normalized split/input/copy/graph/backend/flag state,exact numerical outputs and buffer sizes. They cover automatic/manual placement,CPU/GPU operator fallback,parallel copies,event support/absence,host weights,copy callback handled/declined,raw async copy handled/declined,allocation dependencies/dedup,eval callback reads and early-stop,reset/reuse and split growth beyond16.

Two72input graphs additionally exercise input container growth and parallel storage. Backend compute failure returns the same error and synchronization trace.10expert fixtures compare query/reserve/bind and begin/end/cancel with parallel disabling and decline branches. Total14894checks pass in candidate and native rollback.12constructor allocation sites inject failure and verify NULL/zero owned resources;four threads execute32independent scheduler lifecycles with4128checks and zero heaps.

Harness corrections are recorded:reserve_size measures metadata and must be followed by physical reserve before this fixture allocates;destructive scheduled graph sources must be restored before another plan;the wide graph needs a larger test context. These were not runtime regressions. No full sanitizer rerun,kernel microbenchmark or tuning series was performed. Actual concurrent device execution/cancellation on multiple real GPU streams remains unqualified;mock ordering does not prove that hardware case.

## One fixed real Apex32K/128 gate

| Metric | EXP163 | EXP164 |
|---|---:|---:|
|Prefill tok/s|2631.942|2626.103|
|Decode tok/s|26.956|27.267|
|Sampled RSS MiB|21314.930|21315.086|
|Sampled VRAM MiB|3246|3246|
|Process swap KiB|0|0|

-0.222%prefill/+1.155%decode is small uncontrolled single-sample variation,not stable acceleration or fresh-C++ parity evidence. Archived same-profile original C++2867.846/27.978is historical. Exactly one model request;no parallel compilation/delegated workers during measurement,external host processes remain uncontrolled.

Text,all128token IDs and every saved completion probability/top5 array match original C++ exactly;127graph reuses. Actual scheduler witness `0x1400079fc003bf` includes internal split marker52. All scheduler/buffer/staging/allocator/graph/context ownership counters return to zero. `apex-summary.json` lists actual entrypoints and the mapped library hashes. Raw request/response/log/memory receipts stay local under `runtime/exp164/results/apex-32k128/`,not in published artifacts.

## Reproduce and remaining scope

From this experiment directory:`python3 build.py`,then `python3 check.py`. The latter builds full-object independent probes and runs parity,rollback,OOM and thread cases. `python3 run_apex.py` starts the fixed profile once and refuses an existing process receipt;inspect it instead of automatically rerunning. Paths,commands,compiler/library/source hashes and receipts are retained. Latest completed full self-hosted EXP301 Stage3 remains pinned;EXP302 is still an observer,not a replacement producer.

Global registration/dynamic loading,remaining backend utilities,provider CPU/CUDA implementations,numerical/model/KV/GGUF/tokenizer/server and final native-source closure remain. P4-10 is partial for real asynchronous cancellation;P4-15 still lacks branch/restore/cancel and deferred compatibility evidence. Native rollback/debug diagnostics remain until P4-16. No campaign stage/delivery cycle is falsely declared complete. The current owner rule remains:after each significant change,local correctness plus one fixed real Apex system-speed/memory gate. Next:actual global registry/dynamic loading and residual common backend helpers.
