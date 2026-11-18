# EXP-2026-11-18-957: the tokeniser path has a switch, and the switch is exact

Status: both arms of the switch produce identical output. This is the rollback
proof, not a behaviour change.

## Hypothesis fixed before changes

EXP954-956 routed the QWEN35 vocabulary build, tokenisation and piece decoding
through Goose, but there was no way to go back: the change had no switch and no
tested rollback, which rule 10 requires (on by default, the flag only disables)
and rule 15 needs before any C++ can be deleted. If the native bodies are still
correct, `GGML_GOOSE_TOKENIZER=0` must reproduce them exactly - the same pieces for
every id and the same ids for the same text - and must leave the witness at zero.

## What was done

- `gates.c` gains `goose_tok_enabled()`, following the existing `goose_on()`
  pattern: on unless the variable is literally `0`.
- The five vocabulary helpers, the tokenize hook and the piece hook all consult
  it, so one variable returns the whole path to C++.
- The adapter also skips building the BPE tables when the flag is off, so the
  rollback costs nothing beyond the header window.
- The flag is named `goose_tok_enabled`, not `fg954t_enabled`: the build's
  module-discovery heuristic matches `fgNNNx` and would have searched for a Goose
  module of that name. The first build failed exactly that way, which is how the
  constraint was found rather than assumed.

## Result

Both arms run on the same 260-token qwen35 vocabulary in `vocab_only` mode.

| check | flag on (default) | flag off |
| --- | --- | --- |
| build exit code | 0 | 0 |
| `libllama.so.0` bytes | 5 989 112 | 5 989 112 |
| Goose answers during load | 531 | **0** |
| Goose answers during piece dumps | 519 | **0** |
| piece dump, special=true/false | 1311 + 1304 bytes | 1311 + 1304 bytes |
| `cmp` on vs off | - | **identical** |
| flag-off dump vs accepted library | - | **identical** |
| `abc` -> ids | 1 (`abc`) | 1 (`abc`) |
| gate checks | 2 / 0 failures | 2 / 0 failures |

The tokenize gate shows the same shape: flag on gives 531 answers during load and
1 during tokenize with 13/0 checks; flag off gives 0 and 0 with the same single id
and only the two witness checks failing, which is what those checks are for.

So the two arms are interchangeable in output and distinguishable in who decides.
That is precisely the property needed before the C++ bodies can be removed.

## Not claimed

Nothing is deleted, and the flag does not change behaviour while it stays at its
default. The C++ bodies remain the fallback for SPM, WPM, UGM, RWKV and every BPE
pre-tokenizer other than QWEN35. P7-06/P7-07/P7-12 stay PARTIAL.

No model was loaded: 4 KB of metadata, `vocab_only`, no weights, no graph, no GPU.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp957-goose-flag-20261118
    python3 runtime/reconstructed/build.py --build-dir "$OUT/build2" \
        --runtime-libs /home/end0/AI/LlamaServer/apex-independent-fast-20261116/run
    "$OUT/gate" "$OUT/vocab.gguf" "$OUT/on.bin"
    GGML_GOOSE_TOKENIZER=0 "$OUT/gate" "$OUT/vocab.gguf" "$OUT/off.bin"
    cmp "$OUT/on.bin" "$OUT/off.bin"
