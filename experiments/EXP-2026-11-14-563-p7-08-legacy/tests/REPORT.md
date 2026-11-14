# EXP-563 acceptance addendum: mutation gate and gap inventory

Date: 2026-11-14. Workspace: `/tmp/opencode/sub-567/`. Repository untouched.
Subject: legacy `use_jinja=false` chat path (`draft.goose`).

## Gate

The gate is `build.sh`: it compiles the native oracle, the Goose draft and the C
host, then `compare.py` diffs the fixtures.

| part | checks | baseline |
|---|---:|---|
| detection (54 names + DOTS1 source) | 55 | 55/55 identical |
| real templates from `tests/test-chat-template.cpp` + `add_ass=0` variant | 34 | 34/34 byte-equal |
| targeted cases (roles, tools, empty chat, BOS, trim) | 20 | 20/20 byte-equal |
| content-parts merge (content present / content empty) | 2 | 2/2 byte-equal |
| **total prompt checks** | **56** | **56/56** |

The earlier review had 51 prompt checks. Cases were added during this run
because the trim, BOS and content-parts mutations were not observable on the
old set:

- `case-19-trim`: `llama3`, one user message `"  padded  "`;
- `case-20-parts-chatml` and the two `parts` records: content + two text parts
  + one `media_marker`, with `content` present and empty;
- `case-18-gigachat`: system/user/assistant/user, exercises the `<s>` BOS.

No mutation was weakened. `legacy_merge` was added to the draft so the
content-parts rule (`common/chat.cpp:3755-3768`) is part of the gate, not only
a note.

## Mutation run

`python3 mutate.py` applies one semantic edit per run to a copy of
`draft.goose`, rebuilds with `goose-2026.3-string --standalone --header`,
relinks the host, and runs the same 56 checks. A mutation is `caught` when any
detection, prompt, or parts check differs.

All 12 mutations compiled, so none is a syntax error. The two halves were run
as separate batches (`python3 mutate.py --half 1`, then `--half 2`); logs are
in `mut/results-half1.md` and `mut/results-half2.md`. First half 6/6, second
half 6/6.

| id | category | mutation | verdict | first failing check |
|---|---|---|---|---|
| M01 | detection order | ChatML branch returns before the PHI_4/SMOLVLM precedence, so `<|im_sep|>` is lost | caught | `detect phi-4 native=PHI_4 goose=0` |
| M02 | detection marker | gemma marker gets a trailing space, so `<start_of_turn>` sources are missed | caught | `detect google/gemma-7b-it native=GEMMA goose=55` |
| M03 | default template choice | builtin-name table disabled; names fall through to substring detection | caught | `detect mismatch at chatml` |
| M04 | role separator | chatml role/content separator newline becomes colon-space | caught | `prompt OpenHermes native_len=194 goose_len=198` |
| M05 | role separator | llama3 header ends with one newline instead of two | caught | `prompt Llama-3 native_len=315 goose_len=311` |
| M06 | role separator | phi3 drops the newline after the role tag | caught | `prompt Phi-3-mini native_len=142 goose_len=138` |
| M07 | system handling | gemma stops folding accumulated system text into the next non-model turn | caught | `prompt gemma native_len=179 goose_len=149` |
| M08 | system handling | llama2 `<<SYS>>` condition inverted (plain gets wrapper, sys variant does not) | caught | `prompt Mistral-v0.2 native_len=88 goose_len=106` |
| M09 | BOS/EOS literal | gigachat system branch drops the `<s>` BOS literal | caught | `prompt GigaChat native_len=286 goose_len=283` |
| M10 | trim | `trim_native` becomes the identity | caught | `case-19-trim native_len=105 goose_len=109` |
| M11 | generation tail | `add_generation_prompt` test inverted for every builder | caught | `prompt OpenHermes native_len=194 goose_len=172` |
| M12 | content-parts merge | separator is added even when the content is still empty | caught | `parts-empty native_len=41 goose_len=42` |

Result: **12/12 caught** (6/6 + 6/6). Per-run details are in `mut/results.md`;
each mutation has its own `mut/MXX/` build and `goose.tsv`.

## Gap inventory: 19 formats without a builder

`legacy_apply` returns `-(id + 1)` for a detected format without a builder;
`meta[0]` still carries the correct id. `host` prints one `gap` line per name
and ends with `built 36 open 18` (DOTS1 has no builtin name, so the name-based
line counts 18 while the format count is 19).

| format | id | gap code | what is missing | why it is still open |
|---|---|---:|---|---|
| `deepseek-ocr` | 23 | -24 | builder is "append content only" | no source in the upstream fixture list; trivial, needs a probe fixture |
| `chatglm4` | 27 | -28 | `[gMASK]<sop>` prefix, `<|role|>\n` body, `<|assistant|>\n` tail | upstream test marks ChatGLM4 with no template source |
| `minicpm` | 29 | -30 | `<用户>`/`<AI>` markers with trim | needs a UTF-8 Chinese source fixture; upstream test has no template |
| `exaone3` | 30 | -31 | `[|role|]` turn with trim | no source in the fixture set |
| `exaone4` | 31 | -32 | exaone3 plus a `tool` role branch | no source in the fixture set |
| `exaone-moe` | 32 | -33 | `<|role|>\n` wrappers with a `tool` branch | no source in the fixture set |
| `rwkv-world` | 33 | -34 | turn builder plus "last message gets `Assistant:`" state | no source; needs a stateful case, not a plain template |
| `megrez` | 38 | -39 | `<|role_start|>role<|role_end|>content<|turn_end|>` | upstream test marks Megrez with no template source |
| `bailing` | 40 | -41 | uppercase roles, `user`→`HUMAN` | upstream test marks Ling-lite with no template source |
| `bailing-think` | 41 | -42 | bailing plus `<think>` generation tail | no source; depends on the bailing builder |
| `bailing2` | 42 | -43 | synthetic `<role>SYSTEM</role>detailed thinking off<|role_end|>` plus role_end | no source; two state rules (system presence, role_end) |
| `smolvlm` | 44 | -45 | `<|im_start|>` BOS then User/Assistant turns | no source in the fixture set |
| `dots1` | 45 | -46 | `<|system|>`, `<|userprompt|>`, `<|response|>` wrappers with end markers | no builtin name at all (`LLM_CHAT_TEMPLATES` omits it); needs a source fixture |
| `hunyuan-moe` | 46 | -47 | `<|extra_0|>`/`<|extra_4|>` markers, assistant branch uses `<|eos|>` | no source in the fixture set |
| `gpt-oss` | 47 | -48 | `<|start|>role<|message|>content<|return|>/<|end|>` | no source; channel/role logic is Jinja-specific, the legacy builder is fixed |
| `hunyuan-dense` | 48 | -49 | index-0 system special case, `<｜hy_*｜>` markers | no source; needs an index-0 case |
| `hunyuan-vl` | 49 | -50 | BOS literal plus first-message system special case | no source; same state shape as hunyuan-dense |
| `seed_oss` | 51 | -52 | `<seed:bos>role\ncontent<seed:eos>`, assistant trimmed | upstream test marks Seed-OSS with no template source |
| `grok-2` | 52 | -53 | `System:`/`Human:`/`Assistant:` with separators and trim | no source; needs three role branches |

Classification of the reasons:

- **Plain wrapper, missing only a fixture** (9): deepseek-ocr, chatglm4,
  minicpm, exaone3, exaone4, exaone-moe, megrez, dots1, seed_oss. The builder is
  mechanical; the blocker is a golden pair, not the code.
- **Stateful rules** (5): rwkv-world, bailing2, hunyuan-dense, hunyuan-vl,
  smolvlm. They depend on message index, not only on role/content.
- **Role mapping** (4): bailing, bailing-think, hunyuan-moe, grok-2.
- **Complex branch set** (1): gpt-oss.
- **No Jinja dependency** in any of the 19: each is a self-contained builder in
  `src/llama-chat.cpp`, so the gaps can be closed without the Jinja stack.

## Reproduction

reproduce: bash build.sh && python3 compare.py

From a clean copy (no binaries, no generated `.c`/`.h`/`.o`, no `.tsv`):

```
cd /tmp/opencode/sub-567
bash build.sh && python3 compare.py
```

Expected output:

```
detection: 55/55 identical
templates: ok=34 gaps=0 bad=0
cases: ok=20 gaps=0 bad=0
parts: ok=2 gaps=0 bad=0
```

The gate sources are `probe.cpp` (native raw legacy oracle), `probe2.cpp`
(full `common_chat_params` oracle), `host.cpp` (Goose draft host), `templates.h`
(33 real template sources) and `draft.goose`. `build.sh` compiles `draft.c`
with `gcc`, then links `host.cpp` with `g++` and `draft.o`; binaries and
generated files are disposable, the sources are not.

Current artifact hashes:

```
2f8a009f0288d24fe6c0c586de961e77932a515ae301e91294fb523a85d88ca0  draft.goose
38ceafa6191030e30abe3d933a18f2285186c94eadcc0bc9177813e926b9d5fa  probe.cpp
1303f118a19477cffb72bdf165c401faef6b89db75dc87e8ed3180305b5570f9  host.cpp
```

Mutation replay (optional, rebuilds one mutation at a time):

```
python3 mutate.py --half 1
python3 mutate.py --half 2
```
