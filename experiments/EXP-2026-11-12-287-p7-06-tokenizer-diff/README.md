# EXP287: tokenizer differential corpus -- P7-06 closure evidence

Criterion: "Apex tokenizer/pretokenizer/special-token policy exact on complete native differential corpus; interface
supports future tokenizer implementations without a separate runtime."

## Method

Two arms, same model, same corpus:

- **native**: `native_tokenize.c` links the untouched flint `libllama` + `libllama-common`, loads the model with
  `vocab_only = true` (no weights, no GPU) and calls `common_tokenize(vocab, text, add_bos = false, parse_special = true)`
  once per corpus line;
- **Goose**: the accepted runtime's `/tokenize` with `add_special = false, parse_special = true` -- the same contract.

This is a genuine differential: the native arm is a different build of the same library, not a transcription.

## Corpus

`corpus.txt`, 2216 lines, deterministic (`seed 287`):

- 17 systematic edges: empty string, single space, two spaces, newline, tab, single ASCII letters and digit, `!`, a CJK
  character, a snowman, the three special-token literals, a 300-character run, 50 spaces, 100 zeros;
- 2000 random strings, length 0-80, drawn from five alphabets: printable ASCII, Latin-1 accents, CJK, Cyrillic, and
  typographic punctuation (`«»`, em dash, curly quote, ellipsis);
- 200 strings mixing alphabets character by character.

Special-token handling is exercised by `parse_special = true` plus the literals in the corpus; the pre-tokenizer is
exercised by whitespace runs, tabs and newlines, and byte fallback by the Cyrillic/CJK/typographic ranges.

## Result

    cases: 2216, mismatches: 0

Every line produced the same token ids as the native library. `diff-summary.json` holds the totals and a sample;
`native-ids.txt` holds the native ids and can be regenerated with the driver in one command.

## The interface clause

The tokenizer remains `llama_vocab` with its pre-tokenizer and decoder enums; the Goose modules replace decisions inside
`llama-vocab.cpp` and add no second runtime. A future tokenizer implementation plugs into the same interface, which is
what the clause asks for.
