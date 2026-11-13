# EXP265: MoE cache admission/grouping on the live constructor path

Completes the decision layer of the MoE cache on the real path. EXP-264 put the copy decisions in;
this puts the admission and grouping decision of the constructor in as well.

## What is replaced

The group-building loop of `llama-moe-cache.cpp`'s constructor, behind `fg262a_enabled()` (default on;
`GGML_GOOSE_MOE_CACHE_ADMIT=0` restores the native loop):

1. the native loop still collects the admitted layers exactly as before (experts present, all host weights, known GPU
   device) into flat staging vectors;
2. layout identities are assigned **exactly** -- each admitted layer is compared with
   `llama_moe_cache_same_layout` against the layouts seen so far and gets that index -- so the Goose decision receives
   identities, never hashes, and cannot merge two layouts the native comparison would keep apart;
3. `fg262a_run` returns the group of every layer plus the group headers (device, layout id, bytes, member count, count);
4. the constructor rebuilds `groups` from that output, in the same first-appearance order as the native loop.

If the plan refuses (more groups than layers, which cannot happen with a bound of `n_layers`), the constructor throws
instead of silently continuing with a partial cache.

## Gates

Object compile with the flint command from `compile_commands.json`: ok, adding only `fg262a_enabled`, `fg262a_run`
(the EXP-263 symbols are already in this file from EXP-264).

Live A/B on one relinked DSO under `--moe-cache-mib 3072`, with the copy switch held on in both arms:

| arm | content sha256 | tokens | probabilities | admission witness | copy witness |
|---|---|---|---|---|---|
| `GGML_GOOSE_MOE_CACHE_ADMIT=1` | `9d90a010...` | equal | equal | `FG262A_MOE_CACHE_ADMIT plans=1` | `runs=15855 pairs=46221 stats=5200` |
| `GGML_GOOSE_MOE_CACHE_ADMIT=0` | `9d90a010...` | equal | equal | `plans=0` | identical |

So the admission decision executes exactly once, at construction, and produces the same grouping the native loop
produces -- otherwise the cache contents, and therefore the completion, would differ.

## Scope

The cache profile's content differs from the accepted no-cache profile (`e68df05b`) because the cache changes the
numerics; that is the open GPU-cache question, unrelated to this substitution. This module is live in the 3072 MiB
cache configuration.

## What remains in P1-10's lifetime layer

Tensor/bank construction, view setup, and the copy call sequence (one call per run). Those are ggml calls plus a
loop; the decisions around them are now Goose.
