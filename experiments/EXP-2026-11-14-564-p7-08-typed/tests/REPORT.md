# EXP565 — typed content: closed branches, escaping, separators

Sandbox working copy of the accepted canon plus the typed-direction port. The
repository is not modified. Companion documents: `REPORT-oracle.md` (the oracle
finding), `REVIEW.md` (risks), `cases.md` (native pairs), `SLICE.md` (delta
against the first draft).

## Base vintage

The first draft was cut from canon `0971716d…`. While the draft was being
reviewed the canon grew by 1983 appended lines (`legacy_*`, `peg_map_gemma4`,
`peg_map_minimax`, `g_esc_inner`/`g_esc_quoted`). The slice is rebased onto the
current head, so the numbers below describe the current tree.

```
base/runtime.goose    c49c095d7e497cb0349e6c4992483dc2ada6f8ffc02964c6b0024cf6954f042e
canon (repo)          c49c095d7e497cb0349e6c4992483dc2ada6f8ffc02964c6b0024cf6954f042e
source/runtime.goose  3be19f7fe73250d3ee79f3caca444d7f75659c00d9f6067c402d855f47e736e3
superseded vintage    0971716dce90a67fba303d661f7c520f5f7a3b073386c49a0c1a156b7efd3eaa  (base/runtime-vintage-0971.goose)
```

The textual diff against the head is 240 lines, of which 206 are additions or
changes; nothing was removed or renamed.

## What the slice contains

| line | name | kind |
|---|---|---|
| 6866 | `concat_parts` | private body of `chat_concat_parts` |
| 6914 | `chat_concat_parts` | export, unchanged signature, now a wrapper |
| 7897 | `contains_media` | private body of `chat_contains_media` |
| 7915 | `chat_contains_media` | export, unchanged signature, now a wrapper |
| 11862 | `utf8_ok_strict` | strict UTF-8 validator, rejects what `dump()` rejects |
| 11916 | `content_parts_caps` | the normalizer over typed parts |
| 11979 | `chat_content_parts_caps_json` | export |
| 11996 | `content_string_caps` | the normalizer over a string `content` |
| 12018 | `chat_content_string_caps_json` | export |

Reuse instead of duplication:

- the join rule is the accepted `concat_parts` (canon body, unchanged);
- escaping is the accepted `g_esc_quoted` / `g_esc_inner` from the canon's
  legacy path, i.e. the module's nlohmann-compatible writer (`\" \\ \b \f \n
  \r \t`, `\u00XX` lowercase, raw bytes above 0x7e);
- only strict UTF-8 validation is new, because `nlohmann::dump()` throws
  `type_error.316` and the entry must report failure instead of writing bytes
  JSON cannot carry.

Why the two canon functions were split into body + wrapper: Goose refuses a
call to an `export fn` from another function (`error: … could not resolve one
concrete function signature`; a four-line file reproduces it). The exported
names and signatures stay as they were.

Export count: base 51, source 53. Nothing was removed. New exports:
`chat_content_parts_caps_json`, `chat_content_string_caps_json`.

## Gate

```
FG565_TYPED pairs=24 values=34 failures=0
build mode=final opt=-O1 fixture=cached gen_ms=218413 fixture_ms=3 link_ms=166 run_ms=92 total_ms=218675
```

`pairs` counts comparisons; `values` adds the two-sided checks inside the
render, UTF-8 and separator pairs. Build is 3m39s with `-O1`; `-O2` was measured
at 5m15s and is not used.

| # | input | direction / caps | oracle |
|---|---|---|---|
| P1 | two texts | string | `common_chat_msgs_to_json_oaicompat(true).dump()` |
| P2 | two texts | typed | same, `false` |
| P3 | single text | typed | same, `false` |
| P4 | text + media | string | same, `true` |
| P5 | media + text | string | same, `true` |
| P6 | media only | string | same, `true` |
| P7 | unknown type between texts | string | same, `true` |
| P8 | text ending in `\n` | string | same, `true` |
| P9 | text + media | typed caps | same, `false` |
| P10 | unknown type between texts | typed | same, `false` |
| P11 | typed parts, typed-only template | render | `apply()` |
| P12 | flat string, typed-only template | render | `apply()` (pair 10 of cases.md) |
| P13 | no content, no parts | both caps | helper dump: `""` |
| P14 | no content, no parts | typed-only | `apply()`: one empty text part |
| P15 | text + media | typed-only | `apply()`: join then wrap |
| P16 | text + media + text | typed-only | `apply()`: join then wrap |
| P17 | Cyrillic | string | helper dump, raw UTF-8 |
| P18 | emoji | typed | helper dump, raw UTF-8 |
| P19 | `0x01`, `0x0b` | string | helper dump, `\u0001` / `\u000b` |
| P20 | overlong `c0 80`, truncated `d0`, surrogate `ed a0 80` | both | dump throws, port returns -1 |
| P21 | separators | string | helper dump vs the module writer |

All comparisons are byte-for-byte on the full message array; the render pairs
compare the full prompt string.

### Closed branches

1. `n == 0`: P13 pins `""` under both caps; P14 pins the `only_typed` wrap of
   that empty string into one empty text part, through a real template render.
   Mutations 14 and 15 make both observable.
2. Join + `only_typed`: P15 and P16 reach it with a media message under
   typed-only caps, where `to_json_oaicompat` joins first and the normalizer then
   wraps. Mutation 16 pins the wrapper.
3. Non-ASCII: the native oracle emits raw UTF-8 (probe bytes: `d0 9f d1 80 …` for
   Привет, `f0 9f 99 82` for 🙂) and `\u00XX` for control bytes, with `0x7f` raw.
   The draft's `esc` wrote `\xNN`, which is not JSON. The slice now uses
   `g_esc_quoted`, and P17–P19 show byte equality. Invalid UTF-8: `dump()`
   throws `type_error.316`; the port returns -1, checked for the overlong, the
   truncated and the surrogate form (P20).
4. Separators: the oracle is compact. P21 asserts three facts at once: the new
   compact entry equals the native dump byte-for-byte; the module's own
   `chat_message_json` (spaced, `", "` / `": "`) does **not** equal it; and the
   spaced text parses to the same value. Compact is therefore correct for a
   byte-level oracle contract, spaced is a formatting choice of a different
   surface.

## Mutations

21 mutations, two halves, each rebuilt from scratch with `-O0`; non-zero exit or
a lost gate line counts as caught.

```
mutations 11/11 caught (range 0:11) missed=[]
mutations 10/10 caught (range 11:21) missed=[]
baseline after restore: rc=0 FG565_TYPED pairs=24 values=34 failures=0
```

| # | mutation | result |
|---|---|---|
| 0 | array `type` key misspelled | caught, 5 |
| 1 | array `text` key misspelled | caught, 5 |
| 2 | array separator is `;` | caught, 3 |
| 3 | array drops its last part | caught, 4 |
| 4 | every array part reported as `text` | caught, 1 |
| 5 | array text field filled from the type | caught, 5 |
| 6 | join separator is `;` | caught, 3 |
| 7 | join never clears the media flag | caught, 3 |
| 8 | media no longer forces the string shape | caught, 3 |
| 9 | `only_string` ignores the typed cap | caught, 4 |
| 10 | `only_typed` ignores the string cap | caught, 2 |
| 11 | string branch always joins | caught, 5 |
| 12 | string normalizer loses its closing brace | caught, 1 |
| 13 | string normalizer ignores the string cap | caught, 1 |
| 14 | no parts emits `[]` instead of `""` | caught, 1 |
| 15 | `only_typed` empty part becomes `[]` | caught, 1 |
| 16 | join + `only_typed` wrapper loses `}` | caught, 2 |
| 17 | control bytes use uppercase hex | caught, 1 |
| 18 | high bytes escaped instead of raw | caught, 2 |
| 19 | overlong two-byte lead accepted | caught, 1 |
| 20 | surrogate sequence accepted | caught, 1 |

Mutations 17–20 edit the canon's `g_emit_hex4` / `g_esc_inner` and the slice's
validator, i.e. exactly the rules P17–P20 pin.

## Remaining

1. The canon still carries two escapers: `esc` (old, writes `\xNN` above 0x7e,
   used by `chat_content_json`, `chat_message_full_json`, `chat_tools_json`) and
   `g_esc_quoted` (nlohmann-compatible, used by the legacy path and by this
   slice). For non-ASCII content the two surfaces produce different bytes. The
   slice does not change the accepted functions; unifying them is the next
   cleanup and should get its own gate.
2. Legacy `llama_chat_apply_template` flattening is now ported in the canon
   (`legacy_apply`) but is not part of this gate; it adds `\n` before every part,
   including a media marker, and must be gated separately.
3. The autoparser / grammars, streaming diffs and model parsers stay native.
4. The join+escape writes in place: the escaped text starts at offset 0 while the
   joined text sits at `out.len / 2`, so a caller needs at least twice the
   worst-case escaped size. The gate uses 64 KiB buffers for a few bytes of
   input; a production caller must size `out` accordingly.
5. The fixture links prebuilt objects from `build-cuda` (2026-11-12 vintage) for
   `chat.cpp` and the jinja runtime; goldens must be regenerated if the tree
   moves.

## Reproduce

```
cd /tmp/opencode/sub-565
bash build.sh final            # 3m39s, FG565_TYPED pairs=24 ... failures=0
python3 mutate.py 0 11         # 11/11 caught
python3 mutate.py 11 21        # 10/10 caught
sha256sum base/runtime.goose source/runtime.goose
```

`tests/escape_probe.cpp` reproduces the native escaping bytes quoted above
(link line in `SLICE.md`).

## Export list (53)

All 51 base exports are present unchanged; the last two are new.

| line | export |
|---|---|
| 399 | `chat_probe_used` |
| 1831 | `builtin_names` |
| 1838 | `builtin_call` |
| 5692 | `eval_expr` |
| 6521 | `exec_program` |
| 6548 | `chat_message_json` |
| 6586 | `chat_normalize` |
| 6684 | `chat_probe_content_used` |
| 6708 | `chat_probe_fresh` |
| 6714 | `chat_probe_mask` |
| 6718 | `chat_probe_content_mask` |
| 6771 | `chat_probe_tool_name_mask` |
| 6790 | `chat_probe_tool_calls_mask` |
| 6804 | `chat_probe_tool_arg_mask` |
| 6838 | `chat_probe_second_call_mask` |
| 6914 | `chat_concat_parts` |
| 6927 | `chat_content_json` |
| 7014 | `chat_message_full_json` |
| 7083 | `chat_tools_json` |
| 7118 | `chat_tool_parameters_json` |
| 7410 | `chat_tools_parse` |
| 7870 | `chat_tools_error` |
| 7915 | `chat_contains_media` |
| 7927 | `chat_format_single` |
| 7987 | `chat_input_parts` |
| 8002 | `chat_strip_affix` |
| 8056 | `chat_bases` |
| 8065 | `chat_parse_content_only` |
| 8145 | `peg_rest` |
| 8149 | `peg_end_node` |
| 8153 | `peg_eps_node` |
| 8171 | `peg_literal` |
| 8184 | `peg_child_one` |
| 8221 | `peg_tag` |
| 8236 | `peg_atomic` |
| 8374 | `peg_seq` |
| 8396 | `peg_choice` |
| 8407 | `peg_repeat` |
| 8594 | `peg_dump_basic` |
| 8619 | `peg_any_node` |
| 8623 | `peg_not_node` |
| 8694 | `peg_optspace` |
| 8753 | `peg_prefix` |
| 8797 | `peg_tag_safe` |
| 8863 | `peg_permute` |
| 9789 | `peg_map_msg` |
| 10230 | `peg_map_gemma4` |
| 10525 | `peg_map_minimax` |
| 11802 | `legacy_merge` |
| 11818 | `legacy_detect` |
| 11822 | `legacy_apply` |
| 11979 | `chat_content_parts_caps_json` |
| 12018 | `chat_content_string_caps_json` |
