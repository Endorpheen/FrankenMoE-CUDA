# Observation: decode 42.42 and prefill 2432.95 tok/s after the graph-reuse batch

Recorded because it is the best measured decode seen on this project and because the source that produced it has
already been superseded. The point of this file is that the numbers do not get lost or get re-attributed to a
binary they did not come from.

## The measurement, verbatim

Three sequential runs, pinned profile, prompt 32768 tokens, 512 predicted:

| run | prefill tok/s | decode tok/s | prompt_n | predicted_n | content sha256 |
|---|---|---|---|---|---|
| 1 | 2243.914799 | 42.027782 | 32768 | 512 | c2ff42cc1219 |
| 2 | 2432.949297 | 42.423594 | 32768 | 512 | c2ff42cc1219 |
| 3 | 2435.858743 | 42.437796 | 32768 | 512 | c2ff42cc1219 |

medians: prefill **2432.95**, decode **42.42**. All three content hashes are the reference `c2ff42cc1219`.

## What produced it

- source state: commit `dd6b5482e4e17c457261eb493762c49a763b9843`, the graph-reuse batch (14
  `llm_graph_input_*::can_reuse` functions moved to `fg_gr_check`);
- argv: the pinned Apex profile that `raw/len5.sh` builds — `-b 4096 -ub 4096 --moe-cache-mib 4992`, context and
  the remaining flags as in the script, threads `-t 8 -tb 16`, `-fa on`, `-ctk q4_0 -ctv q4_0`, `-np 1`;
- request: `raw/request.json` (32768-token prompt, 512 predicted);
- log: `raw/run.log`.

## What must NOT be claimed

1. **This is not a measurement of the current binary.** `runtime/goose-stage/lib/libllama.so` has been rebuilt
   since, several times, including a fix to a real defect in that same batch (see below). The SHA of today's
   library must not be attached to these numbers retroactively; the numbers belong to the source at `dd6b5482`.
2. **No causality is established.** This is a measured result *after* that batch, not a demonstrated effect *of*
   it. Prefill varied 2243.9 to 2435.9 within the set, which is larger than most effects this project has chased;
   attributing the decode number to the reuse decision without a same-hour control would be the exact mistake the
   session has made before with session drift.
3. **Not statistically established.** Three repeats, one configuration, no paired control. Recorded as an
   observation, not as a proven speedup.
4. **The batch was not clean.** A later audit found that the mechanical rewrite had read commented-out
   `res &= self_v_idxs->ne[0] == ...` lines as live rows and activated them, and had flattened buffer-guarded rows
   into unconditional ones. The hash still matched. So this observation was produced by source that was
   behaviourally wrong in five functions — which is itself the most interesting thing in this file: see below.

## Why this observation matters

The run produced the reference answer and the best decode of the session from source that had *changed behaviour*
in five `can_reuse` implementations relative to native. The pinned profile runs one architecture; those five
functions are on other architectures' paths, or guarded by buffer presence that Apex's path satisfies either way.

That is a concrete demonstration of the failure mode this project keeps guarding against: a correct reference
hash is not evidence that a port is correct. It is evidence that one path is correct.

## Artifacts

```
raw/l5-batch14_512-1.json   b68a83257395c553...
raw/l5-batch14_512-2.json   5de7751f9d702fbe...
raw/l5-batch14_512-3.json   9e3dc4a4a6b1e7c6...
raw/request.json            971b9e1c0093fd0d...
raw/run.log                 11797b3074195dff...
raw/len5.sh                 (the launcher; reproduces the argv)
```

Full sha256 values are in `MANIFEST.txt`. Igor keeps an independent archive in
`LlamaServer/runtime-observations`.

## Follow-up, not now

Finding the mechanism is deferred deliberately. When it is pursued, it needs a paired same-hour control at the same
generation length, and it needs the fixed source - not `dd6b5482` - so the two are not confused.
