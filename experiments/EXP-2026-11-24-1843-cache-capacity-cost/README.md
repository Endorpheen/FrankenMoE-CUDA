# EXP-2026-11-24-1843 — What one layer of cache capacity actually costs: +19 % traffic, −9 % decode

Status: **MEASUREMENT.** Baseline `37ac0f1d` unchanged (prefill 3122.4, decode 78.225).

## Why this was measured

The reopened prefetch arena would take 432-498 MiB, that is one of the 13 resident
layers, and the earlier estimate treated the cost as "7.7 % less capacity, so about
7-8 % less re-use". That inference was never measured. The direct way to measure it
is to shrink the cache by exactly one layer and read the engine's own counters: same
binary, same profile, `--expert-cache-mib 5824` instead of 6290.

## The measurement

Two arms, three requests each (warm-up plus two measured), counters read as deltas
between `/props` before and after:

| counter | 6290 MiB (baseline) | 5824 MiB (−466 MiB) | change |
| --- | ---: | ---: | ---: |
| expert fills | 117 644 | 137 994 | **+17.3 %** |
| expert evictions | 114 209 | 134 773 | **+18.0 %** |
| uploaded expert bytes | 224.60 GB | 267.45 GB | **+19.1 %** (+42.8 GB) |
| expert hits | 2 944 729 | 2 924 379 | −0.7 % |
| prefill tok/s | 3122.4 | 3097.9 / 3099.5 | −0.8 % |
| decode tok/s | 78.225 | 71.016 / 70.629 | **−9.2 %** |
| text | `f8664331bd30` | `f8664331bd30` | identical |

`runs/cap-084158` holds the requests, responses, VRAM and props.

## What it means for the arena

- The cost of a **static** reserve is not 7.7 % of capacity but **+19 % of expert
  traffic** and **−9.2 % of decode speed**. Over the acceptance request that is about
  14 GB more transfer, roughly 0.6 s at the measured 22.5 GB/s link.
- The potential gain it would buy is the 2.232 s of copies that currently do not
  overlap compute (0.237 s of overlap in the EXP-1838 window). So a *dynamic* reserve
  could still be net positive, but only if it is borrowed only while a prefetch is
  actually pending and returned immediately, and only if the prefetched planes are
  the ones the next layer will ask for.
- That last condition is the open question. In a prefill the next layer's routes do
  not exist until its own router runs, so the only early source is the k=8 route
  prediction, and enabling it in prefill was measured at +22 % traffic with no gain
  (EXP-1840) — consistent with predictions that mostly do not match the routes the
  layer later demands.

## Consequence and next step

A partial prefetch of *missing* experts with a *dynamic* reserve is the only form of
this idea that can pay, and its first requirement is a prediction-accuracy number:
of the planes staged by the prediction, how many are still resident when the layer
asks for them, and how many are evicted unused. That counter is the next measurement;
without it the reserve policy cannot be tuned, only guessed.

## Artefacts

- `runs/cap-084158` — the 5824 MiB arm: three requests, props before/after, VRAM.
- Reference counters for 6290 MiB come from `runs/pfpre-082715/props.json`.
