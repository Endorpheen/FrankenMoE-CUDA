# EXP-2026-11-19-1370: which exported doors have a call site - 49 doors, 38 called, 11 explained

## The method, and the two wrong versions of it

Counting a door as "used" because its name appears in `strings libllama.so.0` is vacuous: the dynamic symbol table
contains every exported name. Counting it as used because another binary references it is wrong in the other
direction: nearly every door is called from C++ that compiles **into** libllama itself.

The method that works is a source-level search per symbol over the host trees (`vendor/flint`,
`runtime/reconstructed`, `integration`), excluding the generated sampler-chain shim, plus a second pass for doors
resolved by name with `dlsym`.

## The result

```
doors 49: with a host call site 38, without 11
```

The eleven, classified rather than listed:

| door(s) | why there is no host call site |
|---|---|
| `bt_build`, `bt_attrs`, `bt_special_order`, `bt_eog_ids` | called from Goose itself (`btl_load`) and from the standalone tokenizer CLI - the vocabulary side, not host C++ |
| `fg_dry_rep_limit_entry`, `fg_typical_entry` | DRY step 1 and the typical entry, reached through the sampler doors and called directly by the gates |
| `fg_sampler_dist_call`, `fg_sampler_chain_call`, `fg_sampler_chain` | the sampler unit's own entry points; the production path enters through the numbered doors the same shim exports |
| `fg_sp_compatible`, `fg_sp_ngram_accept` | **exported and resolved nowhere** - neither host C++, nor `dlsym`, nor another Goose module |

## What the audit found

Six doors are resolved by name in production (`fg_sp_n_max_call`, `fg_sp_prefix`, `fg_sp_reject_accept`,
`fg_sp_synth_accept`, `fg_tk_bpe_before`, `fg_tk_norm`); thirty-two by direct call. And **two are exported with no
call site at all**: `fg_sp_compatible` and `fg_sp_ngram_accept`.

That is not a defect by itself - rule 28 forbids deleting on unproven unreachability - but it is the first candidate
list for the next dead-code pass, and it is recorded here instead of being left for someone to rediscover.

## What this does not answer

Whether the 38 produce the right answer. That is the per-door evidence in `VERIFIED.md`, and the two are different
questions: a door can be called and wrong, or right and unused.
