# EXP-2026-11-17-920: real prompt tokenisation and detokenisation in Goose

Status: in progress.
Base: EXP919, c43fb4b78cc617cdd001e383f80abc849738f9a4.

## Hypothesis fixed before changes

The already accepted reader/model/trunk path can be fed by a Goose tokenizer
that produces the same token ids as the native tokenizer on the same GGUF, and
a Goose detokenizer that reproduces the native piece bytes. No native
tokenizer, sampler or piece-copy call is made from the Goose side; the native
library is used only as the independent reference in the differential gate.

Owned by this experiment:

- UTF-8 decoding with the native recovery policy (one U+FFFD per bad byte),
  codepoint flags (letter/number/mark/whitespace), lowercase lookup and the
  byte-to-codepoint map, all as Goose data generated from the pinned native
  tables;
- the QWEN35 pre-tokenizer matcher (`unicode_regex_split_custom_qwen35`),
  ported branch by branch, operating on codepoints and emitting segment
  lengths, followed by the byte-encoding step;
- the token/merge lookup tables over borrowed GGUF spans (last-wins for
  tokens, first-wins for merges, exactly as the native maps);
- the BPE merge loop with the native rank/left priority order and the
  native stale-bigram rejection;
- detokenisation: attribute-driven special handling, the GPT-2 byte decoder
  for normal pieces, `lstrip`, and the native `clean_spaces`/EOS rules.

Not owned: the GGUF mapping (borrowed from the caller, as in EXP919), the
vocabulary text storage, and the sampler/generation loop (next experiment).

## Fixed contract and gate

Model under test: `Apex-35B-V1.0-Q4_K_M.gguf` — `tokenizer.ggml.model=gpt2`,
`tokenizer.ggml.pre=qwen35`, 248320 tokens, 247587 merges, eos 248046,
add_bos/add_eos false, clean_spaces false.

The gate compares, against `libllama` from the accepted Apex run bundle
(`vocab_only` load, no weights, no GPU, no server):

1. token ids for a corpus covering Russian, English, code, CJK, Hangul,
   emoji, combining marks, contractions, digits, whitespace runs, tabs and
   newlines, special tokens with `parse_special` on and off, invalid UTF-8,
   and long mixed text;
2. detokenised bytes for the same corpus (`remove_special` both ways);
3. `token_to_piece` for every one of the 248320 ids with `special=true` and
   `special=false`;
4. the end-of-generation id set.

No tolerance is used: ids and bytes are compared exactly. The Goose library
must import libc only (`nm -D --undefined-only`).

## Results

The unit builds as a libc-only shared object: `nm -D --undefined-only` shows
glibc symbols and pthread only, no native GGUF/GGML/llama/C++ symbol, so the
tokenizer is not a wrapper around the native one. The native library is used in
the fixture alone, to answer `llama_tokenize`, `llama_detokenize`,
`llama_token_to_piece` and `llama_vocab_is_eog` for comparison.

| check | compared | mismatches |
| --- | ---: | ---: |
| token ids, 41 corpus cases x parse_special on/off | 82 cases / 11173 ids | 0 |
| token_to_piece, every id, special true and false | 496640 pieces | 0 |
| detokenize, every corpus case, remove_special and unparse_special | 164 | 0 |
| end-of-generation set | 5 ids, all 248320 candidates | 0 |

Corpus: empty, ASCII, English with contractions, Russian prose and mixed
Russian numbers, C/Python/JSON code, Chinese + Japanese + Korean, emoji with ZWJ
and skin-tone sequences, combining marks, Arabic, digit runs, leading/trailing
spaces, whitespace runs with tabs and CRLF, newline-only, punctuation runs,
special tokens (`<|im_end|>`, `<|im_start|>`, `<|endoftext|>`, an unknown
`<|...|>` literal), a 70-byte single word, a repeated sentence, invalid 2/3/4
byte UTF-8, an embedded NUL, and a 200-line mixed Russian/CJK/code paragraph
(5690 ids), CRLF with a following space, code with a newline after punctuation,
and contraction-like `a'sd` / `'x` forms, each with `parse_special` true and
false.

Table build: 248320 tokens and 247587 merges inserted with zero duplicates,
matching the native maps' last-wins/first-wins policy.

### Two real defects the gate caught

1. The codepoint flag table must be the DERIVED array native builds
   (`unicode_cpt_flags_array`: range flags plus the whitespace set, the case maps
   and the NFD ranges). The raw range list leaves a space without the whitespace
   bit, so the pre-tokenizer stopped splitting leading spaces.
2. In Goose `(u8_value & 0x3f) << 12` stays an 8-bit expression and truncates to
   zero, so every 3- and 4-byte codepoint decoded to U+FFFD. Each byte is now
   widened to `i64` before masking; the second defect changed 40 of 62 corpus
   cases, the first changed the spacing ones.

### Mutation coverage, and two corpus holes it exposed

Five semantic mutants are rejected: a dropped stale-bigram length check (changes
ids), the removed byte decode in detokenisation (changes piece bytes), a removed
end-of-generation text (changes the eog set), an off-by-one token id lookup
(changes every id), and a dropped newline terminator in the punctuation rule
(changes code text).

Two mutants were NOT detected at first, and each was a real hole in the corpus
rather than in the code:

- disabling the `\s*[\r\n]+` branch and dropping the `[\r\n]*` tail of the
  punctuation rule both need a CRLF with a following space, or punctuation
  followed by a newline; the original corpus had neither, so the corpus gained
  `a\r\n b`, `x = 1;\ny`, and a CRLF C function;
- removing a contraction letter (`'s`) is NOT observable for this vocabulary at
  all, and the corpus cannot fix it: the contraction alternative and the
  `[^\r\n\p{L}\p{N}]?[\p{L}\p{M}]+` alternative produce spans that BPE
  re-merges into the same pieces (`'s` alone, or `'sd` split back into `'s`+`d`).
  The corpus still gained `a'sd a'tx a'rex ...` forms because they document the
  reachable spans, but this decision is recorded as not separately observable
  rather than claimed as covered.

A mutation that hangs (a whitespace branch that stops advancing `pos`) is
recorded as a hang, never as a rejection.

### Boundaries

Mapping, vocabulary text and the caller's buffers are borrowed, as in EXP919.
The attribute passes that depend on the model name (phi-3, modern-bert,
`<mask>`) are not ported: they are keyed on names this model does not have, and
the port refuses to guess. `escape_whitespaces` vocabularies are implemented but
not exercised here (the Apex vocabulary clears the flag), like the FLINT-only
end-of-generation entry. The sampler, the generation loop and the server path
are not part of this experiment.
