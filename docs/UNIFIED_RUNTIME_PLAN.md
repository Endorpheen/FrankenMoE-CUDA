# Unified runtime and model profiles — design proposal

**Historical reference.** This proposal predates the standalone chat host. See [current state](CURRENT_STATE.md) for the current runtime.

Status: DESIGN. No universal default is promoted by this document. Immediate performance work still targets a new>=5% gain above EXP090; model-specific fallback bundles remain preserved.

## What is currently verified

| Model/profile | Preserved working bundle | Evidence and limit |
|---|---|---|
| Qwen3.8 Flash Next mixed IQ, split-MTP | EXP063, scripts/run_qwen38_server.sh | Accepted legacy ring/gather/IQ2_S/MTP chain on its older source/ABI; not validated on EXP090 |
| Apex35B Q4_K_M, all CPU experts | EXP090 under LlamaServer | Manual activation and fresh numerical control confirmed; EXP089's prior performance applies to its measured conditions |
| Flint Q4_1 | Preserved prefix/EXP087/EXP089 bundles | Earlier four-case correctness gates exist; no fresh EXP090 Flint performance claim |

The new source contains QWEN3NEXT and QWEN35MOE implementations. Source-level architecture support is not proof that the legacy Qwen model, MTP options, quality or speed regressions pass. Legacy ring/gather/IQ2_S tuning environment identifiers were not found in the inspected new CPU/CUDA source; equivalent mechanisms must be audited rather than assumed absent or blindly reapplied. Different runtime library ABIs must never be mixed.

## Desired structure

1. One maintained engine source and one coherent executable/library bundle per build. Common optimizations dispatch by actual operation, tensor shape/quantization, backend/device capabilities and memory ownership. Do not special-case new model names in the numerical engine.
2. Model profiles contain resource and workload choices: context budget, threads/affinity, batch, expert placement, KV precision, speculation, checkpoint budget and documented inference requirements. Match exact model identity and GGUF properties; readable names are labels, not correctness predicates.
3. Keep each model's last verified best bundle/profile as a fallback. A proposed common build does not replace that fallback until its own gates pass on that model. Previously accepted optimizations are inventoried and mapped to their mechanisms before rebasing/porting.
4. One visible launch entry should report the selected executable, library directory, build manifest and profile. It resolves only to a verified model/bundle mapping; an unsupported new architecture needs an explicit compatibility path. This avoids mistaking EXP063's legacy Qwen launcher for the manually selected Apex EXP090.
5. Improvements default on within their supported capability predicates. Explicit disables and failure fallbacks remain available. Avoid one global threshold that makes all formats/devices use an unverified path.

## Two distinct kinds of validation

Transport, allocation, cache and scheduling changes intended to preserve arithmetic use numerical/state invariance gates. A different generated sequence alone does not establish worse semantic quality, but can show this intended invariance was lost.

Device/kernel/activation-precision changes can legitimately change probabilities. Define a numerical reference/error budget and representative task/tool-use quality criteria BEFORE implementing/promoting such a change. Use correctness of solutions and tool contracts rather than identical prose as the semantic criterion. Do not retroactively relax a failed experiment's gate to turn it into a success. EXP091's separate quality investigation is deferred.

For every model/profile, separately track loadability, numerical/state behavior, task quality where needed, cold/warm client TTFT, actual prefill, single-response decode, complete request time, startup and RAM/VRAM/swap. A green row for Apex does not fill Flint/Qwen cells. Performance claims require repeated matched measurements; architecture support/startup alone does not qualify a speedup.

## Incremental implementation order

1. Inventory exact legacy/current source chains, hashes, profiles and accepted mechanisms. Separate general hardware constraints from workload/model-dependent knobs; record exact rejected combinations and valid retry conditions.
2. Introduce a profile registry and visible launcher after the current model's accepted profile is settled. Preserve existing entry points as explicit legacy fallbacks; do not silently repoint Qwen to EXP090.
3. Choose the modern source base as a candidate, audit whether each legacy optimization already exists or needs an equivalent port, and validate one model/profile at a time. Keep all components from the same build together.
4. Qualify each capability-gated optimization on its relevant model/shape/precision matrix. Promote the common bundle only when covered profiles meet their pre-stated correctness/quality/resource/performance gates; otherwise keep that profile's fallback.
5. For new models, read official model/GGUF cards and generation configuration, use safe supported paths and record UNKNOWN cells. Add performance tuning only where a mechanism and a bounded probe justify it; do not rerun a historical sweep merely because the model changed.

This is a design direction, not an implementation or an all-model compatibility/performance claim. Installed Desktop/runtime profiles and defaults are unchanged.
