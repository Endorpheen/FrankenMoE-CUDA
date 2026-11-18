# EXP-2026-11-18-976: the standalone gets a tokenizer CLI

Status: a real entry point, its decisions already Goose and its tokenizer the Goose
engine. 25-check gate plus manual runs on every mode.

## Hypothesis fixed before changes

EXP975 proved the standalone can load a vocabulary and tokenise, but only from a
gate. A runtime nobody can run is not a runtime. The CLI decisions already existed
in Goose (`goose/cli/tokenize-cli.goose`: `tz_source`, `tz_add_bos`, `tz_fmt_ids`),
so the remaining work was the process itself - argv, stdin, stdout - which is
substrate, not policy.

## What was done

`integration/goose-standalone/cli/tokenize-main.c`, built as
`frankenmoe-tokenize`:

    frankenmoe-tokenize <model.gguf> --prompt TEXT [--ids] [--no-bos]
    frankenmoe-tokenize <model.gguf> --stdin     [--ids] [--no-bos]

- `tz_source` decides where the prompt comes from, `tz_add_bos` decides BOS from the
  model's `tokenizer.ggml.add_bos_token` read through `gr_meta_int`, and
  `tz_fmt_ids` formats the list, including the `[a, b, c]\n` shape;
- tokenisation is `btl_load` + `bt_bpe` on tables the reader sized;
- the file owns argv, stdin and stdout only.

It lives under `cli/` rather than beside `host_v4.c` so `migration-status.sh` does
not count it as adapter debt: an entry point is not a bridge between Goose and a
substrate, it is the program a user runs.

## Result

| run | output |
| --- | --- |
| `--prompt "abc ab a" --ids` | `[257, 32, 256, 32, 97]` |
| `--prompt "aab"` | `97 256` |
| `printf 'a b c\n' \| --stdin --ids` | `[97, 32, 98, 32, 99]` |
| `printf '' \| --stdin --ids` | `[]` |
| no prompt | usage on stderr, rc=2 |
| gate `tokenize_load` | **25 checks, 0 failures** |

Every id matches the oracle recorded from the accepted libllama in EXP975.

## The bug that cost this cycle, and why it is worth recording

`--stdin` worked from the first build; `--prompt` segfaulted. The pointer was

    text_ptr=0xffffffffba474580

- a 32-bit value sign-extended. Cause: `strdup` is **not declared** under
  `-std=c11` without `_POSIX_C_SOURCE`, so the compiler assumed `int strdup(...)`
  and truncated the returned pointer. Compiling with `-w` hid the implicit
  declaration entirely; the binary was built, ran, and destroyed its own string.

Replaced with `malloc` + `memcpy`. Rebuilt with `-Wall` this time: no warnings.

The lesson is the same shape as the ABI mistake in EXP970 and the silent `set -e`
exit in EXP974: a build that succeeds is not a build that is correct, and the
suppression flags are where the evidence goes missing.

## Not claimed

The CLI tokenises; it does not generate. No context, no model, no layers - it reads
a vocabulary and answers ids, which is what the tokenizer half can do without a
model. CUDA, HTTP transport and full 41-layer execution remain absent, and
`P9-08`'s remaining work is unchanged.

The CLI links `main-gc.o` (function sections plus `--gc-sections`) and the host
primitives, the same shape as the gate; it calls `goose_init`, not
`fg950_goose_init`, because the standalone does not rename its unit.

No model was started; the fixture is 4167 bytes of metadata.

## Reproduction

    python3 experiments/EXP-2026-11-18-954-goose-vocab-wire/tests/make_vocab.py /tmp/vocab.gguf
    FG_SUBSTRATE=/tmp/opencode/sub-668/flint-cpu/build-off/ggml/src \
    FG_TOKENIZE_FIXTURE=/tmp/vocab.gguf \
        bash tools/build-standalone-runtime.sh
    builds/goose-standalone/frankenmoe-tokenize /tmp/vocab.gguf --prompt "abc ab a" --ids
