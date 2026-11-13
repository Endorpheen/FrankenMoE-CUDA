# EXP165: actual Linux registry and dynamic-loader policy in Goose

Solo continuation from verified EXP164.16public registry/loader APIs now select self-hosted Goose by default. Goose owns registry/device storage,registration/dedup/removal,enumeration and case-insensitive name/type lookup,initialization priority,plugin score/init/API validation,handle transfer,search path/directory enumeration,variant scoring/base fallback and load-all ordering. This replaces the real active registry,not an additional unused helper. Non-Linux paths and architecture-specific feature detection remain deferred;universal P4-08 is not falsely closed.

## Implementation and boundaries

`registry.goose` contains the algorithms. The bridge provides typed POD projections/raw byte and memory effects,existing accepted Goose backend-interface calls and POSIX dlopen/dlsym/dlclose/opendir/readdir/stat/readlink/getcwd primitives. It does not call the old registry or search algorithm. Backend tables are raw Goose-owned arrays rather than C++ vectors. A recursive OS mutex serializes registry operations and allows same-thread callback queries. As in the original ABI,borrowed backend/device pointers do not promise safety against caller-unsynchronized unload while other code uses a device.

Native callback ordering matters:publish a backend before dev_count callbacks,and re-read count on each enumeration iteration. An initially cached count was corrected before model testing. Reentrant lookup and callback-order tests now match native. Re-registration preserves insertion order and duplicates do not republish devices. Duplicate dynamic loads close the extra dlopen reference while retaining the original owner. Explicit unload removes associated devices before closing the owned library reference. At process exit,persistent handles are intentionally retained as in native because provider threads/resources may still use them;metadata tables are freed. No thread/resource shutdown guarantee is invented.

Score/init exceptions fail closed inside the typed C++ effect boundary and the caller closes that handle;ordinary rejected score,missing init,NULL init and API mismatch paths close without publication. Allocation guards fail before invalid array access;OOM retains an explicitly owned partial registry that test cleanup can release. This is scoped ownership checking,not universal transactional recovery from every external callback failure. Name folding uses unsigned bytes to avoid native signed-char ctype undefined behavior. Release diagnostics may use a different prefix;selection/API behavior is preserved.

One explicit C/C++ FFI header checks widths before the generated C unit compiles. Native original bodies are retained behind cached `GGML_GOOSE_REGISTRY=0`,and EXP164 remains immutable rollback. Native fallback/diagnostics/platform branches and small OS wrappers remain until source-removal stages. Only `libggml.so` changes;libllama/base/CPU/CUDA artifacts are inherited unchanged. The original registry DSO reproduces SHA `1e52e0dc14ed0c0a21f753031685ffa716107a9d586bdfbf7771b23d1887ceec` before replacement. New public exports are read-only witnesses/counters;mutating/test state release hooks exist only in test images. Production/compiler are untouched.

## Local checks

Native and candidate probes use independent namespaces and no compiled-in real CPU/CUDA providers,so fake provider selection cannot accidentally initialize or substitute hardware.48registration scenarios check dedup,ordering,reentrant count callbacks,name/type lookup,best/name/type initialization,removal and re-registration. Eight real synthetic shared libraries cover positive/zero/negative/missing score,missing/NULL init and incompatible ABI. Dynamic duplicate references,explicit unload,highest score,base fallback,executable/cwd search and fixed cuda/cpu load-all order compare exact state and plugin load/init/close events.345callback events/684checks match native and rollback;rollback witness is zero.

Four threads register128independent providers with repeated queries,without lost/duplicate entries. Six negative cases check bounds,NULL and both registry/device growth OOM paths. Partial owned state remains releasable;test cleanup returns metadata/handle counters to zero. No full sanitizer suite,microbenchmark or tuning series. The current producer reserves `copy` as a builtin and requires an explicit terminal return after `while true`;a foreign alias and unreachable returns adapt the runtime without compiler edits.

## One fixed Apex32K/128 gate

| Metric | EXP164 | EXP165 |
|---|---:|---:|
|Prefill tok/s|2626.103|2633.578|
|Decode tok/s|27.267|27.399|
|Sampled RSS MiB|21315.086|21316.629|
|Sampled VRAM MiB|3246|3246|
|Process swap KiB|0|0|

+0.285%prefill/+0.481%decode is small uncontrolled single-sample variation,not stable acceleration or fresh-C++ parity evidence. Archived same-profile C++2867.846/27.978remains historical. Exactly one request;no concurrent build/delegated workers launched during measurement,external processes uncontrolled.

All128token IDs,text and full saved completion probabilities/top5 match original C++ exactly;127graph reuses. New `libggml.so` mapping and SHA are explicitly checked alongside the four previous runtime DSOs. Actual registry initialization and discovery witness `0x110040000564`;registry and inherited scheduler/buffer/staging/allocator/graph/context ownership counters return to zero,retained plugin handles zero. This fixed profile uses directly linked providers;dynamic score/init/search/unload are verified locally with real plugins,not falsely claimed executed by Apex. Raw request/response/log/memory receipts stay local under `runtime/exp165/results/apex-32k128/`.

## Reproduce and next scope

Run `python3 build.py`,then `python3 check.py` from this experiment directory. The latter constructs independent probes,synthetic libraries/search layouts,parity/rollback/concurrency and bounds/OOM checks. `python3 run_apex.py` performs the fixed profile once and refuses an existing receipt;inspect authoritative state rather than blindly rerunning. Producer,library/source hashes and exact commands are saved. Latest completed full self-hosted EXP301 Stage3 stays pinned;EXP302 is not a completed full replacement producer.

Next:remaining actual Apex CPU/CUDA provider/math and model/KV ownership. Unused training/architecture helpers are not a priority. The universal plan retains non-Linux/feature compatibility and strict native-source removal;this stage does not claim100% Apex. No production changes,auxiliary agents or Qwen work.
