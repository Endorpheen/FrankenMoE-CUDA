# P2-18: the candidate CPU link map, object by object

Criterion: "Candidate link map contains only Goose-emitted CPU logic plus audited primitives; frozen original remains outside candidate
for rollback."

The link command is the recorded `link` step in `variants/cpu-backend/build/integration-commands.json`; it names 33 objects. Each was
classified by a mechanical marker: an object is **Goose-emitted** when it carries `gg_function_N` symbols, which only the Goose compiler
produces.

| class | count | objects |
|---|---|---|
| Goose-emitted | 13 | `goose-controller` (2), `goose-q4` (1), `goose-q6` (4), `activation` (2), `goose-tiled` (13), `goose-tiled` from the perf variant (34), `goose-element` (23), `controller` (2), `goose-cpu` (42), `executor` (1), `stateful` (17), `control` (6), `backend-goose` (2) -- numbers are `gg_function_` symbol counts |
| integration bridges | 6 | `core.o`, `backend-cpp.o`, `backend-traits.o`, `bridge.o` x2, `cpu-bridge.o` -- hand-written C++ that projects native structures into the modules; `core.o` carries 152 hook references and no generated code |
| audited native primitives | 14 | `repack.cpp` (base and arch/x86), `hbm.cpp`, `quants.c` (base and arch/x86), `amx.cpp`, `mmq.cpp`, `native-tiled.o`, `tiled-kernel.cpp`, `vec.cpp`, `sgemm.cpp`, `preserved-native-0.o`, `native-unary-ops.o`, `native-binary-ops.o` |

The native column is the same set of SIMD and architecture-specific kernels recorded as outside the Goose compiler's current scope in
P1-11, plus one deliberately preserved original object.

## Rollback

The frozen original is present and outside the candidate:

    variants/cpu-backend/run/libggml-cpu.so.0.27.0.pre-p502   1 917 080 bytes

That file is the library as it stood before this session's P5-02 change, so a rollback is a copy, not a rebuild.

## Verdict

No object in the candidate is unaccounted for: every one is Goose-emitted, a bridge, or a named native primitive with its reason. The
frozen original exists separately. The criterion is met on the evidence above, and the marker used to separate generated from
hand-written code is mechanical rather than asserted.
