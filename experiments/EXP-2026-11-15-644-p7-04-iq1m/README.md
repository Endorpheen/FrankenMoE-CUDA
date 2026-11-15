# EXP-2026-11-15-642 — iq1_m split search (P7-04)

Base commit: `1de2effc`. Experiment root: this directory.
Native oracle: `../../tree/llama.cpp` (CPU-only build of the same commit).

## Hypothesis

`qz2_row_iq1_m` evaluates the exhaustive split search from prefix sums. The
algebra is exact, but the float accumulation order differs from
`quantize_row_iq1_m_impl`, and whenever two of the four level combinations tie
(the normal case when a 16-value group ends up in a single part) the port
selects a different `k`. `k` decides whether the levels are read back through
`x_p` or `x_m`, so the whole block differs.

## Result

ACCEPTED. One function, one mechanism, two files:

* `goose/model/quant-rows2.goose` — `qz2_row_iq1_m` (prefix block removed,
  native accumulation restored);
* `goose/model/quant-weighted.goose` — the same change in `qz_row_iq1_m`
  (consistency; weighted path not gated here).

| state | gate |
|---|---|
| before (canon + EXP641 qgrid) | 12 of 24 cases differ |
| after | **24/24, 0 mismatches** |

Mutations: 12 one-site edits, two halves of six, Goose object at `-O0`:
**6/6 + 6/6**, and the gate is green again after restore.

## Files

```
source/quant-rows2.goose             canon 1de2effc + EXP641 qgrid patch
patches/iq1m-split.diff              the fix against source (used by the runner)
patches/canon-iq1m-split.diff        the fix in canon paths, on top of EXP641
patches/canon-full-on-1de2effc.diff  EXP641 + this fix, against canon 1de2effc
patches/exp641-qgrid.diff            the accepted EXP641 patch (reference)
tests/fixture.cpp                    the gate: 12 patterns x 2 lengths
tests/make-tables.py                 extracts the native init tables
tests/mutate.py                      twelve mutations, module state materialiser
tests/build.sh                       before | fixed | mut <id>
tests/run-mutations.sh               the mutation run
tests/hashes.sh                      SHA-256 of every artefact
results/                             gate output, traces, mutations, hashes
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

tests/build.sh before && ./results/bin/before   # 12 of 24 differ
tests/build.sh fixed  && ./results/bin/fixed    # 0 of 24 differ
bash tests/run-mutations.sh                     # 6/6 + 6/6
bash tests/hashes.sh
```

## Boundary notes

* **Base state.** At `1de2effc` the canon files do not contain the EXP641 qgrid
  change (it lives only in `experiments/EXP-2026-11-15-641-.../patches`), so
  `source/quant-rows2.goose` is canon + that accepted patch. The full patch
  `canon-full-on-1de2effc.diff` applies both in one step.
* `quant-weighted.goose` is patched for consistency and is not gated (no
  imatrix corpus).
* fp16 storage, the `1.1125` factor and the `max_scale/15` super-scale are
  unchanged; mutations B5/B6 show the gate notices if they move.
* No model, no server, no GPU.
