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
| --moe-cache-mib 2048 | 2348 | 34.03 | 21320 | 5242 | differs |
| --moe-cache-mib 4096 | OOM | - | - | - | - |

Note that EXP087/EXP101 changed the cache's price: the same 2048 setting now costs 5242 MiB VRAM instead of 8456.

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
