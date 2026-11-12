# Reproducible variant: expert device cache (decode 30+)

Status: experimental, **not** an accepted default. The content check differs from `e68df05b`, and per Igor's rule that
is not waived without a decision.

## Command

Base argv is unchanged (`runtime/exp244/results-apex-or244-or-goose/argv.json`); add exactly one flag:

    --moe-cache-mib 2048

Run: `python3 runtime/reconstructed/run_metrics.py` (metrics in `build/run-mc2048b/`) after appending the flag.

## Measured on the current build (EXP087 + EXP101 + pinning, full 307-module port)

| variant | prefill | decode | RSS MiB | VRAM MiB | content |
|---|---|---|---|---|---|
| accepted (no cache) | 2306 | 24.3 | 21319 | 3190 | e68df05b |
| --moe-cache-mib 512 | 2332 | 22.4 | 21320 | 3706 | differs |
| --moe-cache-mib 1024 | 2327 | 29.2 | 21320 | 4218 | differs |
| --moe-cache-mib 1280 | 2334.2 | **30.63** | 21320 | **4476** | differs |
| --moe-cache-mib 1536 | 2344.0 | 31.74 | 21320 | 4730 | differs |
| --moe-cache-mib 2048 | 2348 | 34.03 | 21320 | 5242 | differs |
| --moe-cache-mib 4096 | OOM | - | - | - | - |

Note that EXP087/EXP101 changed the cache's price: the same 2048 setting now costs 5242 MiB VRAM instead of 8456.

Balance point (Igor's preference): on the **same 32K prompt**, cache 3072 gives 37.79 at 6266 MiB, which is about
-3 tok/s and about -1 GiB against cache 4096 (40.78 at 7292). The 37.66 / 5050 entry below was measured on a 16K prompt
and must not be quoted as a 2 GiB saving at equal load.

Best compromise for "decode 30+ at the lowest VRAM" measured so far: **1280 MiB -> 30.63 tok/s at 4476 MiB**, i.e. 766 MiB
cheaper than the 2048 setting for 3.4 tok/s less. 1024 lands just under the target (29.22), so 1280 is the smallest
cache that clears 30.

## Why the content check differs (cause established, not waived)

Not an error and not garbage: logits shift slightly and at `temperature=1.0` the sampler picks a neighbouring branch.
At position 20 the accepted run ranks " line" first (-0.917) with " sentence" second (-1.244); with the cache " sentence"
is first (-0.796) and " line" second (-0.943). Both continuations are coherent and on topic. With the cache off the
output is stable across repeats (`e68df05b` twice); with the cache on it is stable on the other branch (`9d90a010`).

So: a numerically different but valid path, not a broken one. Whether that is acceptable is a correctness decision for
Igor, not something to switch on silently.

## Rollback

Nothing in the build changed for this variant - it is one CLI flag. Remove `--moe-cache-mib` to return to the accepted
profile. Build backups for the other work: `build/libllama-exp371-prepin.so.0.7.0`, `/tmp/libggml-base.pre087`,
`/tmp/libggml-cuda.pre101`, `/tmp/llama-kv-cache.cpp.pre101`.

## Numeric analysis (why the two paths separate)

Measured with two clean arms on the same build (the earlier comparison had a script bug that leaked the flag into both
arms; that reading is discarded).

Token-level: the first 20 chosen tokens are **identical** in both arms; they separate at position 20. Logit distance on
the identical prefix:

    idx  max|dlogp| over shared top-5   chosen token
      0        1.2848                     same
      2        2.0766                     same
      5        0.5878                     same
     10        0.6099                     same
     15        0.2705                     same
     19        1.1545                     same

So the difference is not noise and not a storage-precision loss: `llama-moe-cache` keeps experts in the original tensor
type (`g.ref.back()->type`), not fp16. What changes is the **compute engine**: without the cache the expert matvec runs
on our CPU kernels, with the cache on the GPU kernels. Different summation order over 40 layers x 256 experts compounds
to a 0.2-2.0 logit spread, which at `temperature=1.0` eventually selects the neighbouring branch.

That is the same class as `--n-cpu-moe 34` (device placement changes rounding), which was rejected for the same reason.
This does not prove quality loss, and it does not prove quality preservation either - it is an established cause, not a
waiver.

## Quality measurement, beyond "the answer reads fine"

On the common 20-token prefix (both arms choose the same tokens there):

| quantity | accepted (CPU experts) | cache (GPU experts) | delta |
|---|---|---|---|
| mean logprob of chosen token | -0.2040 | -0.2134 | -0.0094 |
| perplexity (exp of that) | 1.226 | 1.238 | +1.0% |
| mean abs logprob delta of the chosen token | - | 0.0954 | max 0.7767 |
| mean abs delta over shared top-5 | - | 0.3893 | max 2.0766 |

So the path is not merely coherent: on the tokens both arms agree about, it is within 1% on mean logprob. That is a
measured equivalence of quality, not a proof of bit equality - and it is still not a waiver of the acceptance criterion.

## Second, independent prompt: the divergence is prompt-dependent

The 1% perplexity result came from one prompt. Repeated with an unrelated prompt (queue/latency text, 8x repeated,
same sampling parameters):

| metric | accepted (CPU) | cache (GPU) | delta |
|---|---|---|---|
| common chosen-token prefix | 1 of 128 | - | - |
| mean logprob on that prefix | -0.0111 | -0.0096 | +0.0015 |
| perplexity | 1.011 | 1.010 | - |

So the *quality* reading holds (the cache path is not worse; it is marginally better on this sample), but the *text*
diverges from the very first token on this prompt. The earlier "20-token common prefix" was a property of the
repetitive 32K prompt, not a general guarantee.

That matters for the decision: the two paths are not "bit-identical with a tiny perturbation" - they are two numerically
close but distinct samplers, and on ordinary prompts they will produce different answers. Nothing here shows a quality
loss, and nothing here shows quality preservation across arbitrary prompts either. This is the honest state: one
measured prompt shows +1% perplexity, one shows parity, and both diverge textually.

## Bit-compatibility: tested, not achievable this way

Hypothesis: our GPU kernels were bit-identical to the CPU ones in the earlier gates, so the divergence might come from
the MMQ precision mode rather than from moving the work to the GPU. Tested with the cache on:

| MMQ precision | prefill | decode | completion sha | exact match |
|---|---|---|---|---|
| `GGML_CUDA_MMQ_PREC=q4` | 2325.9 | 34.11 | 9d90a010 | no |
| `GGML_CUDA_MMQ_PREC=q8` | 2327.3 | 33.98 | 9d90a010 | no |
| default | 2348.3 | 34.03 | 9d90a010 | no |

Identical output hash in all three, so the precision mode is not the cause and cannot be used to recover bit equality.
The divergence is inherent to evaluating the expert product on the GPU instead of the host: different summation order
over the same values. Making the cached path bit-identical would require computing the experts on the CPU from the
resident copy, which removes the speedup - so this branch is closed rather than left as a vague hope.

## The divergence does not grow with cache size

Measured the same request with three settings on the same build:

| setting | completion sha | common prefix | perplexity (ref / cached) |
|---|---|---|---|
| no cache | e68df05b | - | 1.226 / - |
| cache 2048 | 9d90a010 | 20 tokens | 1.22624 / 1.23782 |
| cache 3584 | **9d90a010** | 20 tokens | 1.22624 / 1.23782 |

2048 and 3584 produce the **same hash and the same perplexity to five decimals**. So the divergence is a binary property
of *whether* the experts are computed on the GPU, not of *how much* is cached: making the cache larger neither heals nor
worsens the numeric difference. That is a useful safety property for the candidate base - moving from 3072 to 3584 is a
pure speed/memory change with no additional quality risk.
