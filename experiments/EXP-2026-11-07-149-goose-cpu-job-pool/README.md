# EXP-2026-11-07-149: complete CPU graph/job/pool control contract

Status: **P2-15 FUNCTIONAL ACCEPTANCE COMPLETE / STAGE1OF13 STILL OPEN**.
This closes one original requirement,not the complete CPU backend,not a whole campaign stage,and not performance or universal safety acceptance.

## Actual Goose ownership

The new pool module owns allocation/free ordering and partial-failure cleanup,mask selection,priority/NUMA placement decisions,OMP/pthread barriers and chunks,pause/resume,worker readiness/poll/wait loop,and generation-safe kickoff. The existing Goose control/executor now run both OpenMP and pthread graph jobs,including reuse,abort status and allocation failure. Generic graph-wide workspace planning no longer calls the original tiled sizing helper;eligibility/arithmetic are Goose. No original native planner/job loop implements the default path. Original bodies remain only for explicit disable rollback.

The source audit checks21entrypoints and maps exact native boundaries. Numerical/kernel-specific scratch/provider callbacks,ABI records/parameter constructors,topology discovery and temporary allocator adapters remain outside this control requirement and assigned to explicit unfinished requirements. In particular MoE-local sizing/provider implementations are not falsely counted as migrated. Those owned adapters are not declared unavoidable final dependencies. Other OS/hardware paths retain separate compatibility gates.

## Independent gates

-404routing cases cover all101operation entries and96leaf callbacks,empty/takeover cases. Expected routes derive from the unchanged native switch,not the Goose implementation. Stub callbacks prove dispatch policy only;actual numerical graphs are separate.
-326actual graph cases (284byte-exact,42within retained tolerance),66GLU/embedding (39exact),960planning/rollback and174backend/buffer records pass OMP and pthread paths. Pthread lacks the documented OPENMPfeature pair;all other data are compared.
-78OMP and81pthread actual lifecycle/graph cases include1/2/8/16threads,pause/resume,reuse,abort/recovery,allocator failures and partial worker-start cleanup. Actual allocated pool/worker counts equal frees;injected failures are not OS OOM.
-494actual-Goose policy cases check masks,NUMA/priority and graph-generation wrap with mock hardware primitives,not multi-NUMA hardware qualification.
-ASan/UBSan/LSan cover rebuilt units/core/base/bridges/fixtures in both paths. Scoped TSan passes on new core/pool/control/executor and real pthread graph/failure fixture. Retained numerical/base/vendor objects are not all instrumented;no full-runtime race/safety superiority claim.
-The non-Linux priority adapter correction is uncompiled on this host;active Linux preprocessed bytes were proved identical. Other platform execution remains unqualified.

## Real Apex32768/128,one request per implementation

| Variant | Prefill tok/s | Decode tok/s | RSS MiB | VRAM MiB |
|---|---:|---:|---:|---:|
| Saved fresh C++ control |2918.900|29.589|21309.152|3348|
| EXP149 OpenMP |2646.558|28.462|21311.797|3348|
| EXP149 pthread |2669.811|26.424|21313.313|3348|

Text,all128token IDs and all saved top5 probabilities equal C++ across both actual model paths. New pool/job and executor traces are saved. OMP remains primary;pthread is a verified compatibility path,not a speed recommendation. These are single uncontrolled-load samples,not fresh repeated pairs or a stable parity claim. Relative to C++:OMP approximately-9.3%pp/-3.8%tg;pthread-8.5%pp/-10.7%tg. No process swap/system swapout during requests.

Owner intermediate >3000prefill/>30decode target is NOT achieved. Future claims require repeated paired runs against a freshly rebuilt isolated C++ control on identical settings and comparable load,with correctness/safety preserved. This target does not replace the full GOAL or justify extra reloads/tuning while functional work is incomplete.

## Reproduction,rollback and next campaign work

Canonical local root:/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107. Self-hosted EXP301 Stage3 remains pinned,compiler source unchanged. Source deltas/generated pool+control and exact commands/results are retained;unchanged modules inherit EXP148. Reproduction is same-host with preserved toolchain/objects,not a standalone native-free runtime. GGML_GOOSE_CPU_POOL=0 disables the new pool policy;CPU_CONTROL=0 and CPU_EXECUTOR=0 restore their respective original policy components. Graph/math remain governed by their independent flags. Do not launch consumed one-shot model receipts again without a new justified gate.

Current campaign stage1has P1-08,P1-09,P1-10,P2-14 still open. Current totals13DONE/160,147remaining,8.125percent by requirement count. All148remaining-at-regroup requirements were assigned exactly once to13dependency-checked stages;no scope was dropped. Production/default and compiler sources were not modified. Continue full stage1rather than starting a fresh architecture or treating this experiment number as a completed stage.
