# EXP-2026-11-17-907: audit the accepted runtime source closure

Status: ACCEPTED for accounting/tooling only. Base: 3989367d4da7607d17e1e656eb332bd9aec46c84.

Hypothesis recorded before changes: historical fixed-path accounting does not
describe the accepted independent runtime and may execute a model while claiming
to be static. A read-only audit of the actual link closure can expose remaining
native code, source provenance gaps and ABI boundaries without rebuilding or
changing the baseline. No speed improvement is predicted; Amdahl is inapplicable
to an accounting-only change.

## Result and scope

Added `tools/audit-runtime-closure.py`, a recorded-command/Ninja/object-symbol
inventory, and the source/ownership/ABI map with a compact machine-readable
snapshot under `docs/goose-migration/`. The accepted set has 9 ELF components,
602 unique object inputs and 350 C++ translation units, alongside 107 C and
145 CUDA translation units. Counts do not indicate functional coverage.
The audit verifies every selected ELF against its link output, follows static
archives, honors the CUDA link cwd, and labels two recipe-only bridge source
mappings whose nested compiler commands were not captured. No repository-local
compiler input in the audited closure is untracked.

Changed `tools/migration-status.sh` so model loading requires `--probe-load`;
the default is read-only. Missing historical files no longer produce malformed
function counts. Historical evidence is labeled and EXP906 is named separately.

## Validation and memory impact

The full audit passed. The focused unittest gate passed 4/4, covering working
directory resolution, static-archive dependencies, generated-source provenance,
and default/explicit model-probe behavior with fake inputs. Logs and detailed
symbol inventories are kept under ignored `Backup/exp907-runtime-map/`.
The runtime binary set is byte-identical to EXP906. No model was loaded, no
compiler/build ran, and no runtime code changed. Decode, prefill, RAM and VRAM
were not remeasured; their latest accepted values remain EXP906's results.

The Goose manifest audit found 4199 matching tracked entries, 72 exact-hash
untracked files and three changed files. These are source-accounting findings,
not a license to accept unknown bodies or weaken a gate. Repair is a separate
bounded change. The draft v0.1.1 remains unpublished while source gates fail.

No C++ was replaced or removed in this experiment. Function symbols, source
ownership, observed execution and standalone replacement remain distinct.
