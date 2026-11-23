# benchmarks/experiment-history.jsonl

One JSON object per line, appended, never rewritten. It is the single source of truth for
every performance number in this repository.

## Why it exists

In one session the same figures were re-derived three times, and one of them was wrong in a
way nobody could have caught by reading it back:

```
"35 tok/s at 4084 MiB VRAM"   - measured while NO server was running. 3801 MiB of that
                               figure is other processes' baseline on this GPU.
```

That was quoted as a configuration for a while. A journal written by the harness itself, with
VRAM recorded per phase, cannot drift like that.

## How to write to it

Never by hand. The harness appends:

```
python3 tools/bench-log.py --tag mc3584-r2 --cache-mib 3584 \
    --response /tmp/ab-mc3584-2.json --vram-during 6784 --ram-kb 21839496 \
    --warmup 1 --status ok
```

`--response` reads timings and the content hash out of a saved `/completion` body, so the
numbers come from the server rather than from a transcript. Anything not passed stays `null`.

## How to read it

```
python3 tools/bench-log.py --show                 # every row
python3 tools/bench-log.py --compare              # best decode per cache size
python3 tools/bench-log.py --compare --include-historical
```

`--compare` **excludes historical and reconstructed rows by default.** A reference run at 512
predicted tokens is not comparable with a measurement at 128, and mixing them is what produced
contradictory numbers before. When included they stay visibly separate.

## Fields

| group | fields |
|---|---|
| identity | `id`, `tag`, `utc`, `git_commit`, `source`, `completeness` |
| runtime | `libraries` (sha256 of each key .so), `goose_compiler` |
| workload | `model`, `quant`, `ctx`, `prompt_n`, `predicted_n` |
| configuration | `argv`, `cache_mib`, `ngl`, `cpu_moe`, `flash_attn` |
| results | `prefill_tok_s`, `decode_tok_s`, `content_sha256` |
| memory | `vram_mib_before_load`, `vram_mib_after_load`, `vram_mib_during`, `vram_mib_peak`, `ram_kb` |
| cache | `cache_allocated_mib`, `cache_hits`, `cache_misses`, `cache_evictions` |
| outcome | `status`, `error`, `warmup_requests`, `repeat_index`, `concurrent_load` |

## Rules the tool enforces

1. Append only. A measurement is never overwritten; a wrong one is superseded by a later row
   and both remain visible.
2. Missing means `null`. No field is filled with an estimate.
3. VRAM is per phase. A pre-load figure cannot be quoted as steady state, because that is
   exactly the error the 4084 MiB number came from.
4. Failures are rows too. Both OOM runs from the cache sweep are in the journal with their
   `cudaMalloc failed` text.
5. Library hashes are read from the files the loader takes, so a session that silently loads a
   different build cannot appear to have measured the same runtime.
6. `source` is `measured`, `historical:<where>` or `reconstructed:<where>`, and `completeness`
   says what is missing. A reconstructed row is never presented as measured.

## Historical hybrid baseline

`runtime/goose-stage/BEST-CONFIG.json` records the older hybrid profile.
The launcher below requires its original machine-local library set:

```
bash runtime/goose-stage/run-apex.sh          # original hybrid launcher; see its versioned flags
```

The script refuses to start if either `libggml-cpu` hash differs from `63ad4984` or the port is
busy, so a session cannot silently measure a different runtime.

Measured at 4352 MiB, 512 tokens: **decode 39.86-40.29, prefill 2429.9, VRAM 7552 MiB, RAM 20.8
GiB**, content sha `c2ff42cc` - the same 512-token answer as the reference.

Two neighbours are kept deliberately, not as failures:
- **3584 MiB** - economical: 33.81 decode at 6784 MiB, i.e. 768 MiB cheaper for 17.9% less decode.
- **4096 MiB** - 39.40 decode at 7298 MiB.

4608 MiB is recorded as a **local dip** (38.65), not an improvement. That dip is the reason
"more cache is better" was wrong twice in this session.

## What is in it now

- 18 measured rows from the 2026-11-15 session, heading from 855.71 / 18.21 (first end-to-end
  run) to 2339.20 / 37.72 (4608 MiB cache).
- 2 OOM rows at 5120 and 7168 MiB.
- 4 historical rows from `docs/goose-migration/apex-optimization.json`, each marked
  `partial` because they carry prefill, decode and VRAM only - no per-phase memory, no RAM, no
  response hash.

The first trustworthy conclusion out of the journal is that cache size and decode do **not**
follow a clean curve on this build: 3584 gives 36.23, 4096 gives 36.12, 4608 gives 37.72, while
the historical 3584 run gave 41.94. A monotone memory-to-speed relation would not look like
that, which is consistent with the other finding of the session: the quantised-dot and expert
routes have no live call sites, so more resident experts do not get used the way they would in
the reference build.
