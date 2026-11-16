# Gate evidence: what was proved, when, and against what

A differential gate only proves something **before** the native body is deleted. Afterwards the "native" function is
itself a call into the module, so comparing the two compares the module with itself — a tautology that still reports
PASS. That is why every line below records the commit at which the gate ran, while the native code still existed.

| module | commit | cases | result | what the cases covered |
|---|---|---|---|---|
| `qwen35-split.goose` | `ef24268e` | 30 | 30/30 | contractions both cases, word after a symbol, digits beside letters, `\r\n` inside and at the end of whitespace runs, runs reaching the segment end, CJK, combining marks |
| `unicode-han.goose` | `1c5d797f` | 1114112 | 0 mismatches | every codepoint in `0..0x10FFFF`; 89008 Han characters found; every range boundary |
| `unicode-newlines.goose` | `d681de39` | 12 | 12/12 | a lone newline, runs of newlines, a run at the segment end, `\r\n` inside a run, leading and trailing newlines, a segment with no newline |
| `unicode-byte-map.goose` | `eb8bec99` | 256 | 0 mismatches | every byte; range edges `0x21`, `0x7E`, `0xA1`, `0xAC`, `0xAE`, `0xFF`, and every gap byte |
| `unicode-afmoe.goose` | `31cb32e3` | 18 | 18/18 | every remainder (1, 2, 0), runs of one to seven digits, digits inside text, text with no digits, leading zeros, digits inside spaces |

## What the numbers mean and do not mean

They mean the module answered identically to the C++ that stood in its place, over a domain that covers every branch
the code has. For Han and the byte map the domain is exhaustive: there is nothing left to sample.

They do not mean the runtime behaves as before in every architecture. `qwen35`, `afmoe`, `newlines`, `byte-map` and
the Han predicate are only reached through tokenisation of text; the pinned request carries pre-tokenised token ids,
so a regression there would not appear in the 44 tok/s measurement. The reference hash `c2ff42cc1219` staying equal
proves the rest of the runtime is unchanged; it does not exercise these paths.

That gap is inherent to these ports and must be stated with each of them, not hidden behind "gates pass".

## Two defects the gates found, and one they found in themselves

- packet fields read with the 32-bit accessor, which truncates pointers — caught by the module misbehaving, twice
  (qwen35 and afmoe);
- `n_off` read at byte 8 instead of 24 in the first qwen35 packet layout;
- the newlines gate itself called a C++ `std::string` function through a C pointer and segfaulted, so that gate is
  C++ and builds a real `std::string` and `std::vector`.

## Reproducing the pipes after replacement

`HAN_GATE` still reports PASS and `AF_GATE` now reports "symbols not found", because the native functions are gone or
replaced. Neither is evidence any more. Re-run them only against a build from before the corresponding commit:

    git show <commit>^:runtime/reconstructed/src/unicode.cpp    # the native body
