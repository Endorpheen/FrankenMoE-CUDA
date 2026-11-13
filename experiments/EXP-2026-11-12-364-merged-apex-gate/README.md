# EXP364: the merged candidate on the accepted Apex profile

The merged candidate (all eleven ported bridges, matched-era build) run on the accepted profile: Apex Q4_K_M, 262144 context, `-ngl all`,
`--cpu-moe`, `-fa on`, `q4_0` K/V, 4096 batch, 8 threads, the preserved 32768-token request with 128 generated tokens.

## Correctness: identical

Five runs of the candidate and three of native, all producing

    sha256(content) = e68df05b4829865dcf8cd2424febfaded15b055f

which is the accepted baseline recorded before any of this merge work. 128 tokens, same text, every time.

## Performance: the candidate is slower

| run | arm | prompt tok/s | decode tok/s |
|---|---|---:|---:|
| 1 | candidate | 1704 | 24.50 |
| 1 | native | 1790 | 27.53 |
| 2 | candidate | 1738 | 26.76 |
| 2 | native | 1773 | 27.44 |
| 3 | candidate | 1717 | 25.65 |
| 3 | native | 1770 | 28.11 |
| 4 | candidate | 1772 | 24.95 |
| **median** | **candidate** | **1717** | **25.65** |
| **median** | **native** | **1773** | **27.53** |

Decode: **-6.8%**. Prefill: -3.2%, but the ranges overlap, so prefill differences are within noise.

By the project rule that a regression above 2% blocks promotion, this build cannot be promoted as-is.

## Where it is not coming from

| variant | prompt tok/s | decode tok/s |
|---|---|---|
| all merged gates off | 1768 | 26.07 |
| `GGML_GOOSE_BATCH_REJECT=0` | 1800 / 1716 | 26.61 / 25.13 |
| our build with flint's `llama-kv-cache.cpp` | 1653 | 26.08 |

None of these recovers decode to native's level, and the batch-reject runs disagree between repeats, so the gap is not attributable to a single port or
to the older kv-cache file. Compiler flags were compared and are identical (`-O3 -DNDEBUG -fPIC` and the same defines).

What is left is the reconstructed source set as a whole: our `libllama` is built from `runtime/reconstructed/src`, which is an older snapshot of the
upstream sources with bridges added. A decode gap of this size with identical flags and identical content is consistent with missing upstream
optimisations that landed after the snapshot.

## Status

Correctness on the accepted profile: **passes**, first time for the merged candidate.
Performance: **fails the 2% rule**, cause narrowed to "the reconstructed source set", not to any individual bridge.

Next step is a source-level comparison of the hot decode paths between our snapshot and current flint, not another measurement of the same build.
