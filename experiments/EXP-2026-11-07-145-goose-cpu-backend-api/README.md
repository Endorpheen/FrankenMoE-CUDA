# EXP-2026-11-07-145: Goose CPU backend API and workspace ownership

Status: **PASS_SELECTED_API_AND_FIXED_APEX / B1 INCOMPLETE**. The same shared
runtime now executes context/plan creation,release,reusable scratch growth and
failure recovery,capability policy,extra-buffer selection order,CPU feature
selection and procedure lookup in Goose.19 API paths are routed to the new
module. Native methods are retained for explicit rollback,not called to
implement the new policy. Physical allocation/delete,ABI records,C++ container
storage,CPU description/OS metadata,native extra providers,HBM and full CPU
buffer alignment/copy remain residuals. Do not call this the complete CPU backend.

## Validation

- 167 actual API records and28,280 capability queries across35 types exactly match
 the preserved previous B1;disable flag rollback also matches.
- Real API checks cover device/procedure/feature registries,host/extra buffers,
 direct/planned execution,scratch growth/shrink/reuse,pool changes,abort and
 recovery with 1/2/8/16 workers.
- 9 allocation-policy cases execute the actual Goose module with a mock resource
 provider:allocation failure cleanup,null-after-free,recovery,sufficient-size
 reuse and zero-size plans. These are not OS OOM stress tests.
- New backend Goose and C++ binding object,core and fixtures are instrumented
 with ASan/UBSan/leak checks.167 instrumented API records equal release bytes;
 all 9 allocation contracts and the retained 326/66/960 suites pass. Retained
 native/earlier objects and vendor runtimes are uninstrumented;no TSan or
 whole-backend safety superiority is claimed.

## One fixed Apex integration check

Exact original/previous text,all 128 token IDs and saved top5 probabilities on the
same 32768/128 request/profile. New backend context init/free and5,346 graph calls
were witnessed,plus thread/pool/registry/capability paths. Counters include
normal startup warmup;not every mode is used by this model placement.

| Metric | Previous post-GLU B1 | New API B1 |
|---|---:|---:|
| Prefill tok/s |2638.151|2608.115|
| Decode tok/s |27.078|26.461|
| RSS MiB |21313.887|21313.453|
| VRAM MiB |3348|3348|

Historical controls,one preliminary response,no controlled speed acceptance.
Record -1.139% prefill / -2.280% decode versus previous without delaying migration
or running local tuning/benchmark series. Production and compiler source are
unchanged;no Qwen run.

## Artifacts and rollback

Canonical Goose:`source/backend/kernel.goose`;ABI/resource binding and retained
control:`source/backend/ggml-cpu-backend.cpp`;all primitive calls are explicit.
The working producer remains the pinned self-hosted EXP300 Stage3,recorded in
`source/producer.json`. Source/generation/build/test recipes and results are
preserved;models,binaries and private feedback/instructions are not included.
The C++-insertion script initially mistook calls for definitions;the correction
is recorded as an application error,not a compiler failure.

Disable the new API with`GGML_GOOSE_CPU_BACKEND_API=0`;the preserved pre-API
bundle is an external control. The stable manual-handoff checkpoint is updated
separately only after replay validation. New model raw records are in
`../../docs/goose-migration/apex-results/b1-backend-api-apex-32k128-*`.
