# EXP-2026-11-07-148: common CPU quantization and type-trait batch

Status: **SELECTED FUNCTIONAL/SCOPED SAFETY PASS; FULL B1 AND SPEED PARITY OPEN**.

This single integration combines three related components:Q1_0/Q2_0/Q4_0/Q4_1/Q5_0/Q5_1/Q8_1 arithmetic,existing Goose Q8_0/Q8_K activation reuse,and actual CPU from_float type traits/MoE dispatch. Nine formats are not nine completed requirements. All original160requirements remain;12DONE,148remaining;this batch closes0complete requirements. Native special-value fallback and remaining formats/arithmetic/ISA contracts remain explicit.

## Producer and actual implementation

Latest completed self-hosted EXP301 key-final Stage3 SHA256 f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a is frozen locally with its three-stage/fixed-C receipt. The compiler source task is still open and was not modified. All five inherited generated C units are byte-identical to EXP147;all four inherited common result corpora are byte-identical. New compact encoding arithmetic executes in Goose,not original C++ wrapper calls. Half conversion is an explicit CPU instruction primitive,not a claim that all numeric primitives are ported. Preserved native functions implement disable/unsupported-domain fallback. GGML_GOOSE_CPU_QUANT=0 disables this component;MoE disable restores the previously validated Goose activation route,and trait disable restores prior native trait functions.

## Gates

-893native-byte/canary/concurrent cases across9formats,8threads;actual CPU type-trait invocation and disable rollback checked.
-841retained Q8activation contracts (587Q8_0,254Q8_K),including NaN/Inf/subnormal patterns for Q8_0 and concurrency;selected finite normal compact corpus.
-326common graphs (284byte-exact and42within existing tolerance),66GLU/embedding (39exact),960planning/rollback and174backend/buffer records remain passing.
-Release and ASan/UBSan/LSan use rebuilt base/CPU/quant/core/ABI/fixtures. Retained native/old objects and vendor libraries are not fully instrumented;no whole-runtime safety/TSan claim.
-One fixed Apex32768/128 request:text,all128IDs and saved top5 equal original C++ and EXP147. New quant module witnesses172640Q8_K blocks across456800calls,including startup and zero-length calls. The counter label values denotes blocks for this mode,not tokens. No claim every codec is used by this model.

## Full-system sample

| Metric | Previous EXP147 | EXP148 |
|---|---:|---:|
| Prefill tok/s |2665.514|2680.113|
| Decode tok/s |27.738|27.876|
| RSS MiB |21311.895|21312.617|
| VRAM MiB |3348|3348|

One sample per stage,uncontrolled external load. This is not proof of parity with original C++2918.900/29.589tok/s. No microbenchmark,tuning or repeated model series.Production/default and compiler sources remain unchanged.

## Remaining scope and next batch

Compact nonfinite inputs use explicit native fallback. Extreme finite/subnormal/overflow domains,other quant formats/raw dot/repack/ISA/calibration/serialization and full CPU closure remain open. Selected corpora are not universal proofs. Continue existing B1 shared runtime,aiming at10-15related original requirements per common cycle where safe;keep individual component guards and rollback,do not merge critical/high-risk acceptance gates. Isolate failures instead of blocking independent components. No subagents.

Canonical local root:/home/end0/AI/LlamaServer/franken-goose-exp148-quant-20261107. Source deltas and new generated module are saved here;unchanged generated code is referenced by verified hashes to EXP147,e7190ece4a12541cbb5e7a8600f30accab2a347a. Build commands preserve inherited object provenance. Reproduction is same-host with recorded toolchain/headers/objects,not a full Goose standalone build. Model supervisor is one-shot and its run has already been consumed. Public snapshot excludes models,binaries,private feedback and local execution agreements.
