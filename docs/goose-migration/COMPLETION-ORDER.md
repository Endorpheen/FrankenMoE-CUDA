# The completion order — mandatory, not advisory

Recorded 2026-11-15. This is the order of work for finishing FrankenMoE-CUDA. It is not
reorderable and no stage is dropped because it is difficult. Stages 3 and 4 do not begin
before Igor's own manual check of stage 2.

**Реальный Goose end-to-end → ручная проверка → завершение переноса → полный аудит →
окончательное удаление ненужного C++.**

## Stage 1 — first real end-to-end run (DONE, EXP-2026-11-15-708)

Goal: run the NEW integrated FrankenMoE runtime with Goose, not the old reference binary and
not a standalone synthetic test.

| requirement | state |
|---|---|
| Apex-35B-V1.0-Q4_K_M, real GGUF weights | done |
| saved EXP244 argv/request plus `--moe-cache-mib 3584` | done |
| `-c 262144`, 32768 prompt, 128 predicted, prior CPU/GPU/KV/batch/thread flags | done |
| real execution of the new Goose code, witnessed | partial — see below |
| real output tokens, prefill/decode, RAM/VRAM, correctness, module list | done except VRAM |
| compared against the saved reference 2228.76 / 41.62 | done |

Measured: `prefill 855.71 tok/s`, `decode 18.21 tok/s`, content sha256
`9d90a010...1ba7560` — **identical to the reference output**. This is a functional
acceptance of the new engine, NOT a 100% migration claim and NOT a performance match.

### What stage 1 left open, and it is a mandatory item

`FG148_QUANT` executed **482 400 times** in the reference. In our run: **zero**. Our
`libggml-cpu.so.0` carries 16 `fg148_*` symbols including `fg148_row_q4_0`, but an
`objdump` scan finds **no caller at all**. So the quantised dot-product route is not wired in
this build: the Q4_K/Q4_0 work runs on the plain C++ path.

That is exactly the kind of hidden C++ fallback this plan forbids. It is not an optimisation
to be skipped — it is a Goose route that exists in the accepted runtime and is dead here.

Temporary C/C++ dependencies remain permitted only as explicitly accounted intermediate
dependencies, and this one is accounted: it is the top open blocker of stage 3.

## Stage 2 — Igor's own check

Provide ONE ready terminal command that starts the new integrated runtime through
llama-server, on the port the earlier logs used (8081), loading Apex with the agreed flags,
so Igor can open the interface, send requests, read answers and watch generation speed.

Requirements: it must start the NEW runtime with the Goose libraries, must not touch a
running server, other processes or reference artefacts, and must not invent paths or
parameters.

No mass deletion of C++ and no claim of final migration before his feedback.

## Stage 3 — maximum migration onto Goose

After his confirmation: complete every remaining component — CPU, CUDA, GGML, KV, loader,
tokenizer, server, API, scheduler, memory and the rest of the required surface. Use the
already written and proven Goose code; do not duplicate it and do not restart the port.
Version, ABI and build incompatibilities are to be RESOLVED, not declared permanent
exclusions. Verify correctness, safety, stability and performance. No hidden C++ fallback
masquerading as Goose.

## Stage 4 — full audit before deleting C++

Audit every remaining C/C++/CUDA file and every real execution path. Determine what is still
used, what Goose already replaces, and which dependencies are technically unavoidable.

Principle: keep the absolute minimum of C/C++ — only genuinely necessary low-level primitives
and external ABI dependencies that cannot be reasonably replaced. Every exception needs a
concrete technical justification; "it is simpler to keep it" is not one.

After full acceptance, a saved rollback state and a check for hidden dependencies: remove all
replaced and unnecessary C++ from the repository in ONE coordinated final operation, leaving
only explicitly documented critical exceptions. Nothing is deleted early. Reference results,
tests and migration evidence are preserved.

## Reporting rules

- Do not present a partial success as final readiness.
- Do not widen the exception list without technical evidence.
- Do not stop other agents.
- Do not re-run the old reference binary to re-measure a known number.
