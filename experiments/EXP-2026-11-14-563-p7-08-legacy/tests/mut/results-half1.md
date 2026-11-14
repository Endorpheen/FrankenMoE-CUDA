| id | category | mutation | verdict | first failure |
|---|---|---|---|---|
| M01 | detection order | ChatML branch returns before PHI_4/SMOLVLM precedence, so <|im_sep|> is lost | caught | `templates: detect phi-4 native=PHI_4 goose=0` |
| M02 | detection marker | gemma marker gains a trailing space, so <start_of_turn> sources are not detected | caught | `templates: detect google/gemma-7b-it native=GEMMA goose=55` |
| M03 | default template choice | builtin-name table disabled, so names fall through to substring detection | caught | `detect mismatch at chatml` |
| M04 | role separator | chatml role/content separator newline becomes a colon-space | caught | `templates: prompt teknium/OpenHermes-2.5-Mistral-7B native_len=194 goose_len=198` |
| M05 | role separator | llama3 header ends with one newline instead of two | caught | `templates: prompt Llama-3 native_len=315 goose_len=311` |
| M06 | role separator | phi3 drops the newline after the role tag | caught | `templates: prompt Phi-3-mini native_len=142 goose_len=138` |
| M07 | system handling | gemma stops folding accumulated system text into the next non-model turn | caught | `templates: prompt google/gemma-7b-it native_len=179 goose_len=149` |
| M08 | system handling | llama2 system wrapper condition inverted: plain llama2 gets <<SYS>>, sys variant does not | caught | `templates: prompt mistralai/Mistral-7B-Instruct-v0.2 (NOTE: Old pre-v1 without a system prompt) native_len=88 goose_len=106` |
| M09 | BOS/EOS literal | gigachat system branch drops the <s> BOS literal | caught | `templates: prompt ai-sage/GigaChat-20B-A3B-instruct native_len=286 goose_len=283` |
