# EXP387: sampler chain orchestration is live

## Verdict

**LIVE on the reconstructed tree.**

## Transferred

| op | rule | native site |
|---|---|---|
| 0 | a backend sampler is skipped only while the chain is still in backend mode; that mode then ends | `llama_sampler_chain_apply` |
| 1 | a sampler without an apply hook is skipped | same |
| 2 | accept reaches every sampler | `llama_sampler_chain_accept` |
| 3 | the sample counter advances by one | same |

Order stays native (a range-for over the vector); what moved are the skip decisions and the counter.

## Gate

| check | result |
|---|---|
| 51 draws over chain mode × sampler kind, apply presence, counts 0..20, counters −2..20 | 51 checks / 0 failures |
| mutations | **5/5 caught** |

## Run

| check | result |
|---|---|
| completion | `e68df05b…`, equal |
| probabilities | equal |
| `FG387S_SAMPLER_CHAIN` | **live, 715 calls** (per sampler per token, both walk and accept) |
| `FG386S` / `FG385S` / `FG384S` / `FG345W` / `FG241U` | live |
