# EXP-2026-11-18-949: the tokeniser engine enters the canonical unit

Status: composition change accepted. The engine compiles inside the canonical
runtime unit; nothing is wired to a shipping path yet.

## Hypothesis fixed before changes

The canonical composed unit (`tools/build-main-runtime.sh`) carried the
tokenizer *decisions* (`goose-tokenizer.goose`, `vocab-type.goose`,
`special-partition.goose`) but not the engine that turns text into ids and ids
into bytes. `goose/model/bpe-tokenize.goose` and `goose/model/bpe-detokenize.goose`
were therefore dead code in the repository and alive only in an experiment
directory outside it. Adding the engine to the canonical list should typecheck,
generate, and compile - and should not collide with any existing global.

## What was done

`tools/build-main-runtime.sh` MODULES gains five modules, in dependency order:

    goose/misc/unicode-tables.goose      byte/codepoint tables
    goose/misc/unicode-text.goose        uc_cpt_from, decode/encode
    goose/misc/pretokenizer-qwen35.goose pt_qwen35_body
    goose/model/bpe-tokenize.goose       bt_build, bt_bpe, bt_attrs, ...
    goose/model/bpe-detokenize.goose     bt_token_to_piece, bt_detokenize, ...

The dependency set was not guessed: `goose check` reported the missing names one
at a time (`pt_qwen35_body`, `uc_cpt_from`, `uf_flags`) until the set was closed,
and the final composition matches the module list already used by the gated
EXP920 experiment. No module was written; no C++ was touched.

## Measurements

| check | before | after |
| --- | --- | --- |
| modules composed | 130 | **135** |
| `goose check` specializations | 4125 | **4206** |
| `goose check` result | ok | **ok** |
| generated C | 20.6 MB equivalent unit | 20 646 600 bytes |
| `gcc-13 -O1 -fPIC -c` | - | **exit 0** |
| global definitions in the object | - | **1132** |
| duplicate global definitions | - | **0** |

The gate `goose check` alone cannot see a duplicated global, and the object-level
check is what covers it: the generated unit compiles to one object with a single
definition per symbol. Linking the object needs host primitives
(`fg637_fmt2`, `mt_fmt_g6`) that the real build supplies, so a full link was not
attempted and is not claimed.

Engine exports now present in the unit:

    bt_build  bt_bpe  bt_attrs  bt_eog_ids  bt_special_order
    bt_detokenize  bt_is_eog  bt_token_to_piece

## Not claimed

`bt_build` takes the token and merge offsets as `const i64[:]`, which is the
format `gr_string_array_index` produces per array; a caller must run it once for
the token array and once for the merges, then `bt_build` fills the hash tables.
That caller does not exist in tracked source yet. The shipping server still
tokenises in the C++ body, so P7-06, P7-07 and P7-12 stay PARTIAL.

The next slice is the tracked caller: a small function in the carrier that runs
`gr_string_array_index` twice and `bt_build` once at load, plus the piece path
that `bt_token_to_piece` already answers.

## Reproduction

    mapfile -t MODS < <(sed -n '/^MODULES=(/,/^)/p' tools/build-main-runtime.sh \
        | grep -oE 'goose/[a-z0-9/_-]+\.goose')
    python3 tools/compose-goose.py OUT/unit.goose "${MODS[@]}"
    $(python3 -c "import json;print(json.load(open('runtime/compiler/current.json'))['compiler'])") \
        check OUT/unit.goose
    $(python3 -c "import json;print(json.load(open('runtime/compiler/current.json'))['compiler'])") \
        --standalone -o OUT/runtime.c --header OUT/runtime.h OUT/unit.goose
    gcc-13 -O1 -fPIC -c OUT/runtime.c -o OUT/unit.o
    nm --defined-only --extern-only OUT/unit.o | awk '{print $3}' | sort | uniq -d

Logs: `/home/end0/AI/LlamaServer/exp948-arr-str-table-20261118/script-list-check/`
(`gen.log`, `obj.log`). No model was started.
