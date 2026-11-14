| id | category | mutation | verdict | first failure |
|---|---|---|---|---|
| M10 | trim | trim_native becomes the identity, so padded content keeps its spaces | caught | `cases: prompt case-19-trim native_len=105 goose_len=109` |
| M11 | generation tail | add_generation_prompt test inverted for every builder | caught | `templates: prompt teknium/OpenHermes-2.5-Mistral-7B native_len=194 goose_len=172` |
| M13 | new format: chatglm4 prefix | chatglm4 loses the <sop> separator inside its fixed prefix | caught | `cases: prompt case-22-chatglm4 native_len=48 goose_len=46` |
| M14 | new format: exaone-moe separator | exaone-moe drops the newline after the user role tag | caught | `cases: prompt case-26-exaone-moe native_len=117 goose_len=116` |
| M15 | new format: bailing2 synthetic system | bailing2 no longer inserts the fixed system turn when none is present | caught | `cases: prompt case-34-bailing2 native_len=140 goose_len=88` |
| M16 | new format: hunyuan-dense tail | hunyuan-dense assistant turn uses the wrong closing marker | caught | `cases: prompt case-35-hunyuan-dense native_len=126 goose_len=126` |
| M17 | new format: gpt-oss assistant close | gpt-oss closes the assistant turn with <|end|> instead of <|return|> | caught | `cases: prompt case-39-gpt-oss native_len=123 goose_len=120` |
| M18 | new format: minicpm user wrapper | minicpm stops emitting the <AI> marker after a user turn | caught | `cases: prompt case-23-minicpm native_len=15 goose_len=11` |
| M12 | content-parts merge | merge adds the separator even when the content is still empty | caught | `parts: mismatch parts-empty native=['0', '41'] goose=['0', '42']` |
