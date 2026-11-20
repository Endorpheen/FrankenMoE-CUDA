# Apex performance baseline

## Current canonical baseline: EXP-2026-11-19-1298

Registered 2026-11-19. Any performance comparison is made against this entry, on identical conditions.

```
profile        Apex-35B-V1.0-Q4_K_M, RTX 4070 12 GiB
request        32768-token input, 512-token output, seed 123, one sequence (np 1)
context        262144
batch/ubatch   4096 / 4096
ngl            all, -fit off, --cpu-moe, --moe-cache-mib 4992
quantisation   -ctk q4_0 -ctv q4_0, flash attention on
threads        -t 8 -tb 16, --cpu-mask 0f0f, --cpu-mask-batch ffff, strict
server         llama-server from builds/flint-acc/bin, port 9101
hash           c2ff42cc1219 (response content and 512-token sequence)
```

### Measurements, three sequential runs of three requests

```
run 1   decode 62.29 62.73 62.89   prefill 2678.4 2953.3 2948.0
run 2   decode 62.53 62.77 62.99   prefill 2702.5 2948.7 2962.5
run 3   decode 62.56 63.45 62.48   prefill 2662.4 2968.7 2904.4
median of the nine: decode 62.73   prefill 2948.0
```

The registration run quoted in the experiment card gave decode 62.79 / prefill 2952.3 / VRAM 9511 MiB /
RSS 20837548 kB; the three-run medians above are the number to compare against, because they are the same
quantity measured nine times rather than three times once.

### Control for the baseline's own key feature

Same build, same conditions, `GGML_MMAP_EXPERT_PIN=0`:

```
decode 42.63 43.91 44.15   prefill 1945 2190 2175   VRAM 9475 MiB   RSS 20837332 kB
```

Everything in the new baseline depends on the resident mmap expert ranges being registered. A build that
loses that registration silently returns to this level, which is why
`tools/check-accepted-components.py` runs as a pre-flight of every benchmark.

### Published release

```
tag      v0.9.0-exp1298   (annotated, on e5a725033a0d65ea7d3d6da5446e573ee0318151)
release  https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.9.0-exp1298   (Latest)
asset    frankenmoe-apex-exp1298-linux-x86_64.tar.gz
sha256   834a5808aaafd8109d81cfbd593968425263d5eca601f2cb05b8d23a3c751177
```

Downloaded after publication to a separate directory, checksum verified, extracted (12 of 12 files verified) and run:
decode 62.56 / 62.86 / 62.38, prefill 2701.2 / 2956.0 / 2954.1, hash `c2ff42cc1219`, accepted-component pre-flight
5 of 5. Earlier releases including EXP906 are untouched.

### Later checkpoints of the same baseline

| checkpoint | difference | numbers |
|---|---|---|
| `builds/checkpoint-20261119-EXP1298` | the published release | decode 62.73, prefill 2948.0 |
| `builds/checkpoint-20261119-EXP1332` | same baseline, unit grown by the detokenizer and GGUF closures (11847 symbols) | decode 62.72, prefill 2944.0, hash unchanged |
| `builds/checkpoint-20261119-EXP1346` | same baseline, and the loader now takes its metadata from the port (11853 symbols) | decode 62.69, prefill 2945.4, hash unchanged |
| `builds/checkpoint-20261119-EXP1355` | same baseline, and all four detokeniser decisions are served by the port | decode 63.14, prefill 2944.6, hash unchanged |
| `builds/checkpoint-20261119-EXP1373` | same baseline, the short-buffer contract fixed, the grammar window widened, and the server's synthetic door in the compiled copy | decode 62.76, prefill 2951.0, hash unchanged |
| `builds/checkpoint-20261119-EXP1396` | same baseline, and the tokenizer is now served by the port on both sides - encode and decode - behind a pre-type guard | decode 63.02, prefill 2952.6, hash unchanged; text-prompt reference `2d38a6fd6a03` |
| `builds/checkpoint-20261119-EXP1401` | same baseline, the encoder swept over the whole vocabulary, and the GGUF check widened to floats and booleans | decode 62.96, prefill 2946.8, hash unchanged |
| `builds/checkpoint-20261119-EXP1405` | same baseline, the stack-overflow defect in the short-buffer diagnostic fixed, and both comparisons shown able to fail | decode 62.93, prefill 2937.6, hash unchanged |
| `builds/checkpoint-20261119-EXP1412` | same baseline, plus the fit-params line and the bench-report formatting composed into the unit and protected by the gate | decode 62.85, prefill 2945.6, hash unchanged |

The baseline number is EXP1298's. EXP1332 is the artefact that carries more of the port, and its measurements say the
growth cost nothing measurable.

### Provenance

| item | value |
|---|---|
| git revision | see `builds/checkpoint-20261119-EXP1298/REVISION` |
| checkpoint | `builds/checkpoint-20261119-EXP1298/` with `SHA256SUMS` |
| headline experiment | `experiments/EXP-2026-11-19-1298-host-registration/` |
| launch command | `tools/apex-test.sh <tag> 512 4096 4096 4992` (records argv, checksums, deps per run) |

## Excluded 32K sampler series

The raw sampler series is stored in `results/p610-32k-1197/` at
`2d8f1699b50ccb1685bc92566dd27455da9243f8`. The commit subject calls
this series EXP-1196. The EXP-1197 grammar-cost card at `8a0aedc6`
describes a separate experiment.

Both requests are byte-identical (SHA-256
`2fac7e8358c977ce081156612f770cdb622ff25cfc39999484c18fc784fc8a8e`).
They extend `word ` repeated 32,000 times and evaluate 32,001 input tokens.
The request overrides launcher settings with seed 42, temperature 0.8,
top-p 0.90, typical-p 0.90, repeat penalty 1.10 and EOS masking. It requests
512 output tokens, disables prompt caching, and requests no token IDs or
probability records. The launcher keeps context 262,144, batch and ubatch
4,096, one sequence, FlashAttention with Q4_0 KV, mmap and a 4,992 MiB
expert cache, with CPU MoE plus CUDA. The harness names a mutable stage
executable and library directory. The recorded series lacks matching
artifact hashes and a loaded library map.

The headline 84.21 decode and 3654.43 prefill are the second OFF request.
Against its first request, prompt evaluation fell from 10,411.325 to
8,756.758 ms and decode from 6,271.904 to 6,068.077 ms. Both responses
report `cache_n=0`. The log shows 510 graph reuses after request one and
1,019 after request two, cumulative. Process and data warm-up can explain
part of the repeat gain, but this record cannot assign the gain to one
cache or kernel. GPU samples are whole-card occupancy after the requests.
RSS is the process snapshot at that same point. Neither is an isolated
allocation peak.

All four responses have the same content SHA-256
`e97af530063a5d15944aeda2c3f76b6aa74311ce7376799925dfdcba505a60d6`.
The ON trace shows typical-p retaining 15 candidates on its first call
and one at call 100, with top-p also retaining one there. The same collapse
in output appears with the tested Goose sampler switches OFF. This rules
out the tested switch bundle as a necessary condition. A repeated
continuation, the shared filtering/sampling path and forced length are
consistent with a low-diversity loop. The first trigger cannot be assigned
to a prompt, an individual stage, or a logit/detokenization defect from
these artifacts alone. They also do not show the per-token expert routes.
The speed is excluded from all accepted references.

### Verified content in EXP-1476

The three archived release responses in
`rel-exp1476-20261119-212613` contain the same substantive capacity-risk
assessment (2,255 characters), the same 512 output IDs (252 distinct), and
matching saved probability arrays. They report medians of 62.8579 decode
and 2940.9133 prefill on the 32,768-ID request and different sampler
settings. The executable and libraries still match the archived manifest.

| Control item | SHA-256 |
|---|---|
| request body | `484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f` |
| response content | `c2ff42cc1219ea4dfe6d69f02e008d0f90c0fc56260736a0aafb82cec362924f` |
| output IDs, compact JSON | `fdf27fb67f12a5c0dcfa04ad59daf47100749df70cc096ed6cda8f9285e3d54c` |

The archived Path B 512-token series also preserves both hashes and its
probability arrays across all paired responses. Its faster median decode
is historical evidence for that artifact. It does not establish throughput
on the independent Goose runtime. The resident host registration
experiment EXP-1298 and the in-place expert-bank and graph-reuse evidence
are independent reasons to build those mechanisms into the new runtime.
The rejected sampler series cannot quantify their individual contribution.

## Historical baseline: EXP-2026-11-16-906 (kept, not superseded in the record)

```
decode 55.5176   prefill 2625.2066   (median of six responses, A-B-B-A block)
artifact: /home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/stage-run
```

Its own control, without host registration, decoded at 42.59 / prefilled at 2019.28. EXP1298 is +13.1 % decode
and +12.5 % prefill against EXP906, and EXP906 remains in the project history with its artefacts untouched.

## Rule for future records

A new confirmed record becomes the current baseline; every previous baseline is preserved in this file and
its artefacts are never deleted, overwritten or declared invalid. If a comparison is made under conditions
that differ from the table above, the difference is stated with the numbers rather than left implicit.

## How a speed claim is accepted (rule 35)

Any figure is reported together with the profile it was measured under. The accepted profile is the one in
`tools/apex-test.sh`: context 262144, `-b 4096 -ub 4096`, `-ngl all -fit off --cpu-moe`, `--moe-cache-mib 4992`,
KV `q4_0`, `--cpu-mask 0f0f --cpu-strict 1`, `-np 1 --cache-ram 0 --ctx-checkpoints 32 --load-mode mmap`, and the
fixed 32K-input / 512-output request.

A number produced with a different context, cache, batch or request length is an **exploration**, filed separately and
never compared with the record. The record stands at decode 62.79 (canonical median 62.73) and prefill 2948, and it is
only exceeded inside that profile, in paired runs, with the response hash check and memory figures reported.

## Probability parity, measured as a digest

`tools/apex-probs-pair.sh` runs one prompt twice on the same build: greedy, top-five probabilities reported per position, once
as shipped and once with `GGML_GOOSE_SAMPLER_TOP_P=0 GGML_GOOSE_SAMPLER_PENALTIES=0`.

```
goose_on:   digest=d384807f4f2014e7 positions=24 content_len=115
doors_off:  digest=d384807f4f2014e7 positions=24 content_len=115
identical=True
```

The reported distribution is the same distribution, which is what a probability claim has to mean. Text parity is held
separately by the record hash `c2ff42cc1219`, because text can differ legitimately at a top-p boundary while the distribution
does not.
