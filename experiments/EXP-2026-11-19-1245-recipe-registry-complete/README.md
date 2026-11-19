# EXP-2026-11-19-1245: the MoE recipe registry is complete, names and suffixes

The sweep now checks three things against the native table - the name of every row, the number of non-null
suffixes, and every suffix string (with the nullptr slot reported as -1):

```
FG_ARCH_SWEEP recipes=36 disagreements=0
```

36 comparisons: nine names plus 27 suffix slots. The suffix-count checks run in the same pass but are not
counted in that total, which is a flaw in the counter rather than in the check - noted so the figure is not
read as "36 = everything".

## What the rows are, and which one is not like the others

```
qwen3moe  qwen2moe  qwen35moe   -> ffn_gate_exps, ffn_up_exps, ffn_down_exps
gemma4                          -> ffn_gate_up_exps, ffn_down_exps, nullptr
gpt-oss   lfm2moe  deepseek4  bailingmoe3  qwen4exp -> the standard three
```

`gemma4` fuses gate and up into one tensor and leaves its third slot empty, so it is the only row whose
`ar_exps` is 2 and whose slot 2 must answer -1 rather than an empty string. That distinction - nullptr versus
empty - is exactly what a hand port gets wrong silently, and it is now checked live.

## Where P9-01 stands

The recipe registry: **complete and verified live** - nine rows, their names, their suffix tables, index for
index, on a real model load.

The 163-name `LLM_ARCH_NAMES` table of llama.cpp is a different artefact. It stays with P7-12 and is not ported;
conflating the two is the error this card's predecessor made and corrected.

## Residual, stated

`ar_match` (the suffix matcher that consumes these rows) is not part of this sweep. It has its own gate from the
original port; adding it here would need a layer-name generator, which is a bigger check than the table itself.
