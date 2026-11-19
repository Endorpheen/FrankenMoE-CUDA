# EXP-2026-11-19-1387: why a draft costs a decode - the head's experts are evicted

## What the code says

- `llama_moe_cache` covers **every** layer including the nextn one: `g.slru.init(model.layers.size(), n_expert, g.n_slots)`
  and `layers[il]` is addressed by the same index the MTP graph uses (`il = hparams.n_layer()`, i.e. 41).
- `llama_model::pin_mmap_experts` registers every tensor with `ne[2] == n_expert`, so the head's experts are host-pinned
  like the rest - they are resident in RAM and streamed through the same 4992 MiB device cache.

So nothing is missing from the path. What makes a draft expensive is the **policy**: the head's experts are touched only
during drafts, so they are cold in the SLRU and are re-uploaded on each draft step, exactly like a cold expert in a
decode. The draft's price is therefore expert traffic, which is what the 0.86-of-a-decode measurement said.

## How much a fix would cost

From the model's metadata (256 experts, `n_embd 2048`, expert feed-forward 512, `n_embd` projections, q4_K weights), one
layer's expert set is on the order of **450 MB** - roughly 9 % of the 4992 MiB cache. Guaranteeing that slice to the nextn
layer would remove the upload from every draft step without touching the target profile.

## The measurement that decides it

If the upload is the cost, then pinning the head's experts must move the draft's price (`D` in EXP-1386's terms) well
below 0.86, and the same acceptance curve (63 % / 36 % / 19 %) must then produce a **gain** instead of a loss - at
`D ~ 0.1` the model predicts roughly `2.17 / 1.1 = 1.97x`.

That is a large, checkable prediction, and it is the reason to spend the change rather than abandon MTP.

## What must not move

The accepted profile's numbers. Any implementation is measured on the same build with the mechanism off (62.73 / 2951)
and on (MTP at 40960, and MTP at the accepted context if the memory allows), and the response hash is compared for the
non-MTP path.

## Order of work

1. read how the SLRU assigns slots and whether a per-layer quota already exists;
2. add a **quota for the nextn layer's experts**, since they are small and their reuse pattern is the opposite of the
   trunk's;
3. measure the draft's price again with one draft per step - the same discriminating run as EXP-1386;
4. only if the price falls, run the three MTP configurations and compare against the control.
