# EXP-2026-11-15-636 — IQ quantizers off the grid (P7-04)

Base commit: `4930d479`. Experiment root: this directory.
Native oracle tree: `../../tree/llama.cpp` (CPU-only build of the same commit;
`upstream/llama.cpp` is unchanged between `b18249e0` and `4930d479`).

## Hypothesis

The P7-04 IQ row quantizers read the grid through the same host primitive the
IQ *dequantizers* use. That primitive returns the static grid literals of
`ggml-common.h`, while the native quantizers read the tables
`ggml_quantize_init()` builds. The two tables hold different encodings of the
same points, so the port decodes grid entries with the wrong scale and its
quantized blocks differ from native as soon as a 2-bit group leaves the grid;
`iq2_s` aborts outright in its final store.

## Result

ACCEPTED as a fix for the grid view. One mechanism, two files, fourteen
functions:

* `goose/model/quant-rows2.goose` — 6 functions, 13 changed lines;
* `goose/model/quant-weighted.goose` — 8 functions, 15 changed lines.

Gate (`tests/fixture.cpp`, corpus of ten patterns × two lengths × two types,
122,245 measured off-grid lookups):

| state | iq2_s | iq3_xxs | both |
|---|---|---|---|
| canon (`4930d479`) | segfault | 2 mismatches, then `fail(1221)` | crash |
| canon + patch | 0/20 | 0/20 | **0/40** |

Mutations: twelve one-site edits, two halves of six, Goose object at `-O0`:
**half A 6/6, half B 6/6**.

## Files

```
source/quant-rows2.goose        canon, byte identical to 4930d479
source/quant-weighted.goose     canon, byte identical to 4930d479
patches/quant-rows2-iq-qgrid.diff    the fix for rows2 only
patches/quant-weighted-iq-qgrid.diff the fix for weighted only
patches/canon-iq-qgrid.diff          both, in canon paths (the patch to apply)
tests/fixture.cpp               the gate: two grid views, off-grid corpus
tests/make-tables.py            extracts the native init tables into a TU
tests/mutate.py                 twelve mutations, materialises module states
tests/build.sh                  canon | fixed | mut <id>
tests/run-mutations.sh          the mutation run
results/                        logs, generated C, gate output, mutations.txt
```

## Reproduce

```bash
# native oracle (once)
cmake -S ../../tree/llama.cpp -B ../../tree/llama.cpp/build \
      -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DGGML_CUDA=OFF \
      -DGGML_NATIVE=ON -DLLAMA_BUILD_COMMON=OFF -DLLAMA_BUILD_TESTS=OFF \
      -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF \
      -DLLAMA_BUILD_TOOLS=OFF -DLLAMA_BUILD_APP=OFF
cmake --build ../../tree/llama.cpp/build -j 32 --target ggml

tests/build.sh canon && ./results/bin/canon      # crashes
tests/build.sh fixed && ./results/bin/fixed      # 0 mismatches
bash tests/run-mutations.sh                      # 6/6 + 6/6
```

## Boundaries

* `iq1_m` is **not** fixed here: it carries a second, independent defect that
  survives the grid fix (10 of 20 corpus cases still differ). See REPORT.md §6.
* weighted (imatrix) entry points are patched for consistency but are not gated
  in this experiment; the fixture has no imatrix corpus.
* fp16/scale conversion and all host primitives are unchanged.
* no model, no server, no GPU.
