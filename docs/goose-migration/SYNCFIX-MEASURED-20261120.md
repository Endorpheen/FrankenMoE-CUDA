# The sync fix is measured: speed back to the baseline, with the JSON path in

Runs on 2026-11-20, same profile, port 9101, one at a time, model 100% resident.

| set | decode (3 runs) | median | prefill (3 runs) | median | VRAM | hash |
|---|---|---|---|---|---|---|
| baseline `run-candidate` | 61.26 / 62.04 / 61.67 | **61.67** | 2656.3 / 2929.7 / 2923.8 | **2923.8** | 9465 | `c2ff42cc1219` |
| baseline + adapter, old common (`run-json`) | 59.21 / 59.57 / 61.45 | 59.57 | 2538.5 / 2796.5 / 2783.5 | 2783.5 | 9465 | same |
| baseline + adapter, **fixed common** (`run-syncfix`) | 61.29 / 61.53 / 61.68 | **61.53** | 2654.1 / 2915.3 / 2909.2 | **2915.3** | 9465 | same |

So the set that carries the adapter costs **-0.2% decode and -0.3% prefill** against the baseline - inside
run-to-run spread - where the same set with the un-fixed common cost 3-5%. The difference is the one block
that synchronised the context per token from a branch that fires almost never.

And the JSON path is accepted on that set:

```
door json_parse in libllama        present
adapter in libllama-common         present
set self-contained                 ok
swap test, three modes             ok
json swap: goose=1 texts=7 failures=0
```

## What this establishes

- the sampler guard fix is worth **3-5% of decode** in a set that carries the adapter, and costs nothing in
  correctness: the response hash is identical in all three arms;
- the JSON path and the baseline speed are **not in conflict** - the two can be in one set;
- the earlier attribution ("the adapter costs 5%") was wrong in its cause and right in its measurement: the
  cost was real, and it was a synchronisation in the sampler, not the JSON work.

## What is still true and still open

`run-syncfix` is assembled from two builds - the baseline `libllama` and a common built today. It is a
laboratory set, not a release, and the provenance question about the baseline is untouched. What it does
show is that a single build with today's sources **can** hold both, because the only reason this set is
mixed is that the baseline predates the adapter.

## Next

1. one artefact built from today's tree that passes the accepted-component check - which is the
   baseline-provenance question, not a code question;
2. then this measurement repeats on it and the set becomes the baseline.
