# Legacy chat-template golden pairs

Every prompt below is copied from the native oracle (`probe --cases-tsv`), not
invented. The Goose draft reproduces all of them byte for byte:

```
native:  33/33 templates + 1 no-ass variant, 20/20 targeted cases, 2/2 content-parts, 55/55 detections
draft:   56/56 prompt bytes equal, 0 mismatches
```

The shared conversation for cases 1-6 is:

| # | role | content |
|---|---|---|
| 0 | system | `You are a helpful assistant.` |
| 1 | user | `Hello` |
| 2 | assistant | `Hi there` |
| 3 | user | `Who are you?` |

`add_generation_prompt = true` unless stated otherwise. In the prompts `\n`
means one 0x0A byte; no real newline is intended.

---

## Pair 1 — `chatml` (format id 0)

Template string: `chatml` (builtin name). Detected: `CHATML`. Length 194.

```
<|im_start|>system\nYou are a helpful assistant.<|im_end|>\n<|im_start|>user\nHello<|im_end|>\n<|im_start|>assistant\nHi there<|im_end|>\n<|im_start|>user\nWho are you?<|im_end|>\n<|im_start|>assistant\n
```

## Pair 2 — `llama3` (format id 25)

Template string: `llama3` (builtin name). Detected: `LLAMA_3`. Length 315.

```
<|start_header_id|>system<|end_header_id|>\n\nYou are a helpful assistant.<|eot_id|><|start_header_id|>user<|end_header_id|>\n\nHello<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\nHi there<|eot_id|><|start_header_id|>user<|end_header_id|>\n\nWho are you?<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n
```

## Pair 3 — `gemma` (format id 15)

Template string: `gemma` (builtin name). Detected: `GEMMA`. Length 179.

The system message has no turn of its own: it is trimmed and prepended to the
first user turn. `assistant` becomes `model`.

```
<start_of_turn>user\nYou are a helpful assistant.\n\nHello<end_of_turn>\n<start_of_turn>model\nHi there<end_of_turn>\n<start_of_turn>user\nWho are you?<end_of_turn>\n<start_of_turn>model\n
```

## Pair 4 — `llama2-sys` (format id 2)

Template string: `llama2-sys` (builtin name). Detected: `LLAMA_2_SYS`. Length 106.

```
[INST] <<SYS>>\nYou are a helpful assistant.\n<</SYS>>\n\nHello [/INST]Hi there</s>[INST] Who are you? [/INST]
```

## Pair 5 — `mistral-v1` (format id 5)

Template string: `mistral-v1` (builtin name). Detected: `MISTRAL_V1`. Length 92.

Note the leading space before `[INST]` and before `Hi there`.

```
 [INST] You are a helpful assistant.\n\nHello [/INST] Hi there</s> [INST] Who are you? [/INST]
```

## Pair 6 — `deepseek3` (format id 22)

Template string: `deepseek3` (builtin name). Detected: `DEEPSEEK_3`. Length 140.

```
You are a helpful assistant.\n\n<｜User｜>Hello<｜Assistant｜>Hi there<｜end▁of▁sentence｜><｜User｜>Who are you?<｜Assistant｜>
```

## Pair 7 — `chatglm3` (format id 26)

Messages:

| # | role | content |
|---|---|---|
| 0 | system | `Be brief.` |
| 1 | user | `Hello` |

Template string: `chatglm3` (builtin name). Detected: `CHATGLM_3`. Length 59.

The fixed `[gMASK]sop` prefix and the single space after each role line matter.

```
[gMASK]sop<|system|>\n Be brief.<|user|>\n Hello<|assistant|>\n
```

## Pair 8 — `pangu-embedded` (format id 53)

Messages:

| # | role | content |
|---|---|---|
| 0 | user | `Hello` |
| 1 | assistant | `Hi there` |

Template string: `pangu-embedded` (builtin name). Detected: `PANGU_EMBED`. Length 115.

No system message is present, so the builder inserts an empty synthetic system
block before the first message.

```
[unused9]系统：[unused10][unused9]用户：Hello[unused10][unused9]助手：Hi there[unused10][unused9]助手：
```

---

## Repeat the oracle

```
cd /tmp/opencode/sub-567
./probe --cases-tsv                  # 20 targeted cases
./probe --names-tsv                  # 54 names + DOTS1 source
./probe --tsv                        # 33 real templates from the upstream test
./host                               # the Goose draft over the same fixtures
```

The comparison script that produced the counts is `compare.py`.
