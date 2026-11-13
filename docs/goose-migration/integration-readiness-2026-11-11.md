# Integration readiness for the small Apex path (2026-11-11)

Owner priority as of this checkpoint: close the small end-to-end Apex path (32K prompt / 128
generated) step by step, with integration and live witnesses. Do not widen the general
runtime and do not accumulate OFF_PATH modules. This note is the hand-off between the gated
work of EXP289–EXP346 and the integration step it was never meant to replace.

## What is already live in the active candidate

`runtime/active.json` points at EXP244. Its route list (`where-the-work-is.md`) is still the
accurate picture: ten KV/recurrent routes, five Unicode/tokenizer routes, the loader, graph
decisions, CPU quant batch, CPU MoE executor and CUDA up to MMQ dispatch. The KV/recurrent
live routes are `fg227n`, `fg228e`, `fg229i`, `fg232r`, `fg233c`.

## What EXP321–EXP346 added, and what each needs to go live

All of it is gated, OFF_PATH and — by the acceptance policy — worth nothing until it is called
by the active candidate. Every row below has `source/host-qualified.c` ready to link.

| experiment | native function | live witness it would add | blocking prerequisite |
|---|---|---|---|
| EXP321–EXP323, EXP345 | `llama_kv_cache::state_write` range collection, `state_write_meta`, `state_write_data` | `fg321w` | a save request against the active DSO |
| EXP324–EXP325 | `state_read_meta`, `state_read_data` | `fg324r` | a restore request against the active DSO |
| EXP344 | `state_read_sinfo`, both `state_clear` overloads | `fg344s` | same restore path |
| EXP330, EXP332 | `prepare`, `apply_ubatch` | `fg330p` | every completion, no extra endpoint |
| EXP327–EXP329 | `seq_add`, `seq_div`, `update` | `fg327a` | a context switch or K-shift request |
| EXP326 | lazy VMM buffer policy | `fg326v` | allocation at load time |
| EXP333–EXP334 | `set_input_k_shift`, rotation copy, `get_prev_tokens` | `fg333k` | a batch that needs any of them |
| EXP335–EXP343 | hybrid / hybrid-idx policy and k-pool layout | `fg335h` | a hybrid model profile, not Apex |

The last row is the one to leave alone: Apex is not a hybrid-indexed profile, so those routes
would be dormant even when integrated. They stay gated and must not be counted.

## The one item that cannot go live

`set_input_pos_bucket` still cannot be ported: its arithmetic mixes float and double and Goose
has only `f32`. That is a compiler capability question, not a porting task, and it is the only
scoped KV input left uncovered.

## Integration plan for APEX.KV (needs rule-1 approval at step 2)

1. Build a new DSO stage from EXP244 with the EXP321–EXP334 modules linked in place of their
   native bodies. This is a build, not a model run; production and the compiler are untouched.
2. Run one Apex request on the 32K/128 profile with the witness counters enabled. Approval is
   required by rule 1; one run, no parallel tasks.
3. Accept only if the text, the token stream and every saved completion probability match the
   reference, and the new route counters are non-zero. A zero counter keeps the item PARTIAL.
4. If step 3 fails, keep the diagnostic and revert the stage: the previous DSO stays active.

## Why this file exists

The gate work kept producing commits with gates and no live effect. That is exactly what the
acceptance policy refuses to count, and it is why the next step is a build-and-run, not another
module.
