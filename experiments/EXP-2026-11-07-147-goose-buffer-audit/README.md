# EXP-2026-11-07-147: independent EXP146 audit and real Goose buffer policy

Status: **FUNCTIONAL PASS / SCOPED MEMORY CHECKS PASS / SPEED PARITY UNPROVEN**.
B1 remains incomplete. Production/default binaries and compiler sources are unchanged.

## Confirmed corrections

1. EXP146 buffer Goose functions returned constants or asserted while C++ still performed alignment,allocation/free,memset/copy. This did not satisfy the migration contract. EXP147 executes checked address/range/ownership/primitive-dispatch policy in Goose. C++ marshals ABI records and calls explicit libc/system primitives; the retained native branch is rollback only.
2. EXP146 unit-base.c declared fg145 metadata/action before macro-renaming calls to fg146,leaving fg146 prototypes absent. GCC accepted implicit int returns; high pointer bits were lost. Real Goose free reproduced SIGSEGV on a truncated pointer before the correction. Exact int64 prototypes plus -Werror=implicit-function-declaration fix the integration defect; this is not a compiler codegen defect.
3. EXP146 safety script instrumented fixture executables but reused uninstrumented runtime units. EXP147 rebuilds new base/CPU units,generated code/core/bridges/fixtures under ASan/UBSan/LSan. Inherited older native/Goose and vendor libraries remain uninstrumented;no full safety superiority or TSan claim.
4. EXP146 physical-buffer fixture wrote256bytes at offset128 into a256-byte tensor by directly calling its vtable,while the backing buffer was4096bytes. The corrected valid case writes at offset0. New contract tests cover high-address pointers,allocation failure,borrowed/owned cleanup,zero lengths,span overflow and rejection before memory primitive calls.
5. EXP146 canonical history copied EXP145 values into prefill/decode/rss/raw_summary while appending differently named EXP146 fields. Corrected canonical values from actual raw EXP146 summary;original evidence retained. EXP146 snapshot omitted response/memory/summary despite their existence in live results;retained them here in the shared history. No evidence fabrication allegation is inferred.
6. EXP146 from_ptr and HBM Goose modes were not hooked. EXP147 hooks and witnesses from_ptr. HBM is compiled out on this host and remains explicitly native/unverified,not completed.

## Gates

326common graph cases (284byte-exact,42within abs2e-5+rel2e-5 versus C++);66GLU/embedding (39exact);960planning/rollback exact;174backend-buffer records exact versus preserved B1 math/control and rollback;28280capability queries. Original C++ floating softmax may differ in final bits from pre-existing B1,so the backend-policy exact oracle is the preceding B1,not a fabricated full-C++ bit-exact claim.

148actual Goose memory-policy cases and9graph-allocation contracts pass release and sanitized. Mock system failures are not OS OOM. New base and CPU DSOs are instrumented,not merely executables. One fixed Apex request per C++/EXP146/EXP147 arm;all text,128token IDs and saved top5/logprobs exactly equal. Set276,from_ptr124,clear1 and get_base10561Goose model calls witnessed,plus graph/control/executor. Allocation/free and all other modes are not all exercised by this model placement;dedicated contracts cover selected paths. Correctness on this prompt is not a general quality benchmark.

## Fresh sequential full-system measurements

| Variant | Prefill tok/s | Decode tok/s | RSS MiB | VRAM MiB |
|---|---:|---:|---:|---:|
|Original C++|2918.900|29.589|21309.152|3348|
|Preserved EXP146|2648.645|28.608|21311.754|3348|
|Fixed EXP147|2665.514|27.738|21311.895|3348|

Fixed versus EXP146:prefill+0.637%,decode-3.041%. Versus fresh C++:prefill-8.681%,decode-6.257%. Single samples under external load are preliminary;no claim of speed parity,causal attribution or production acceptance. No tuning or repeated benchmark series. None of these processes swapped and no system swapout occurred during requests.

## Reproduce and rollback

Canonical isolated source root:`/home/end0/AI/LlamaServer/franken-goose-exp147-audit-20261107/source/cpu-backend`;candidate:`variants/cpu-backend/run`;sanitized:`variants/sanitized/run`;original EXP146 libs:`reference-exp146`. Frozen compiler SHA256454151b3449b208b62335d942dc2226eb19fdd3bfccec3c0a42ecdc13ef71e61. No old C++ Goose producer used. Source and inherited object provenance/build commands are recorded;this is not a wholly Goose standalone runtime yet.

`GGML_GOOSE_CPU_BACKEND_API=0` rolls API/buffers/traits back. `source/backend/request.h` is shared by three C++ adapters. `reproduce/build.py` and `tests.py` rebuild/test the selected same-host checkout;`--sanitize` rebuilds runtime units too. `model_check.py` is one-shot with per-arm receipts;the three measurements have already been consumed and must not be repeated without a new justified stage. Histories/requests/responses/hash receipts/traces are under results and shared apex-results. Model binaries,selected embedding fixture bytes and private instructions/feedback are excluded.

Security diff review found no demonstrated newly reachable external-input exploit in the frozen EXP146 scope. This does not dismiss the reproduced ABI/validation/functional defects above. Whole-runtime/vendor/GPU/TSan audit is outside this diff gate. Continue P1-06/P2-13 coherent quantized CPU math work using the existing shared runtime,not a new architecture. Full task counts stay160/12DONE/148remaining.
