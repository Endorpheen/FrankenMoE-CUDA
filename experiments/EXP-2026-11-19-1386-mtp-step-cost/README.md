# EXP-2026-11-19-1386: a draft costs 0.86 of a decode, and the reason is the expert path

## The discriminating measurement

One draft per step, everything else unchanged (context 40960, 512 tokens, `--verbose`):

| drafts per step | decode tok/s | acceptance | mean len | ms per step |
|---|---|---|---|---|
| 0 (control) | 79.86 | - | 1.00 | 12.5 |
| 1 | 75.46 | 0.753 (0.753) | 1.75 | 23.3 |
| 3 | 59.80 | 0.392 (0.630, 0.357, 0.187) | 2.17 | 36.4 |

From those: a step with one draft costs **1.86 decodes** and returns 1.75 tokens. A step with three costs 2.9 decodes and
returns 2.17. Solving the two gives a draft at about **0.86 of a decode** and verification at about 1.0 - so the draft is
not cheap, and the whole loss follows from it.

## Why the draft is not cheap, given it is one layer

`llama_model_qwen35moe::graph_mtp` builds a **single block** (`il = hparams.n_layer()`, one layer) and asserts exactly
that. One layer of a 41-layer model should cost a fortieth of a forward, yet it costs 0.86 of a decode step.

The block is not a small one: the builder asserts `layer.nextn.eh_proj`, `enorm`, `hnorm` **and
`ffn_gate_inp`** - the draft block is itself a 256-expert MoE block. Under `--cpu-moe` its experts come from RAM, the same
path that dominates decode. So the draft step is priced by expert traffic, not by its layer count, and one decode-like
cost is exactly what it measures.

## What that means for speculation on this profile

The economics are: gain `L` tokens per step at a cost of `1 + D` decodes, where `D` is the draft's price in decode units.
With `D = 0.86` the mechanism breaks even at `L = 1.86`; measured `L` is 1.75 at one draft and 2.17 at three (where the
extra drafts each add ~0.86 of cost for ~0.2 of accepted length). So MTP cannot pay for itself while its draft is priced
like a decode.

## The one change that would change those economics

Make the draft block's experts cheap: its experts are one layer's worth, small enough to be **resident** beside the 4992
MiB cache. If layer 41's experts were pinned in VRAM, `D` would fall toward the cost of a dense block and the same
acceptance curve would become a gain rather than a loss.

That is a mechanism-local change - the cache policy for one layer - not a change to the accepted profile, which is what
rule 35 requires.

## Next

1. check whether the expert cache's layer policy includes or excludes the nextn layer today;
2. if excluded, measure pinning it and re-run the same three configurations;
3. accept only if the in-profile numbers improve, with the hash and memory reported.
