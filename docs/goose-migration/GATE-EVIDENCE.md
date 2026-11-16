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
| `unicode-gpt2.goose` | `8cd24e06` | 27 | 27/27 | contractions both cases, letters, digit runs, punctuation with a leading space, whitespace runs, leading/trailing spaces, CJK, combining marks, empty segment |
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

## Not blocked after all: `unicode_tolower` is already reachable

I wrote here that qwen2 and llama3 were blocked on a missing FFI for `unicode_tolower`. That was wrong, and it is
corrected rather than deleted because the wrong version was in the repository for one commit.

What the library actually exports:

    nm -D libllama.so | grep tolower
      00000000003d3cc0 T fg218t_tolower          <- a Goose module's own implementation
      0000000000236420 T _Z15unicode_tolowerj    <- the C++ function, reachable by its mangled name

So a bridge can call it today, with no new wrapper:

    unsigned int _Z15unicode_tolowerj(unsigned int cpt);   /* declared in the bridge, called as usual */

The contraction branch then compares the lowered codepoint against 's', 't', 'm', 'd' and the 're'/'ve'/'ll' pairs,
exactly as the native code does.

What remains for qwen2 (133 lines) is translation work, not a missing capability: single digit instead of runs,
letters *and* marks, a prefix that is any non-CRLF non-digit rather than a space, and the `\s*[\r\n]+` branch
tracking `last_end_r_or_n`. Each of those must be translated and then gated, which is why it is a cycle of its own.

The lesson is small and worth keeping: "blocked on a missing FFI" was a claim about the build that one `nm` command
settled. Claiming a blocker without checking costs a cycle as surely as claiming a pass without a gate.

| `unicode-qwen2.goose` | this commit | 28 | 28/28 | case-insensitive contractions, single digits vs runs, letters with combining marks, non-space prefix, the `\s*[\r\n]+` jump, whitespace runs, CJK, empty segment |
| `unicode-llama3.goose` | this commit | 32 | 32/32 | everything Qwen2 covers, plus digit runs split into groups of three (`1234567890`, `12`, `1234`, digits inside text) |

## REJECTED: qwen2 and llama3 passed their gates and still crash the runtime

Both modules passed their differential gates (28/28 and 32/32) and both are rejected anyway.

The assembled runtime aborts on the pinned profile:

    #6  fg161_fail () from libggml-base.so.0
    #7  fg161_gg_function_77.constprop () from libggml-base.so.0
    #5  ggml_abort ()

Rolling the library back to the build before those two ports (Backup/after-afmoe, sha 8ac4e5b7) restores it:
decode 44.38 / 44.60 / 44.54, prefill 2346.9 / 2522.9 / 2521.9, hash c2ff42cc1219. So the break is in one or both of
those commits, and the current installed library is the rolled-back one.

This is the lesson the gates cannot teach on their own: a gate compares a function against its predecessor on a corpus
I chose. It cannot see whether the OUTPUT the function produces is accepted by the code downstream, and here it is
not - the failure surfaces inside a different module (fg161) in a different library (libggml-base), nowhere near the
splitter.

Next step is a bisect, not a guess: install the build with qwen2 but not llama3, run, and see whether it survives.
Neither commit is reverted from git - they are good code that has not been shown safe, and the record must say
exactly that rather than deleting them.

Until the bisect says otherwise, the installed library is Backup/after-afmoe (8ac4e5b7), which measures 44.5 tok/s.

## Boundary: unicode.cpp is done to the extent porting can reach

The file's decisions are now Goose: `unicode_cpt_is_han`, the byte maps in both directions, the newline splitter, and
the Qwen3.5, GPT-2 and AFMOE pre-tokenisers. What remains is 14830 native function lines across the whole runtime, but
inside this file the remainder is of two kinds, and neither is translatable by the method used so far.

**Table-driven normalisation.** `unicode_cpts_normalize_nfc`, `_nfd`, `unicode_canonical_decompose` and
`unicode_canonical_compose` are decided by Unicode tables - decomposition mappings and combining classes - that live
in C++. A Goose module cannot reach them by a call per codepoint without moving the tables into the module as data
first. That is a different construction from everything above: it means passing a table through FFI once, proving the
table transfer itself, and only then porting the loop. It is work worth doing, and it is not "translate 42 lines".

**Framework, not decision.** `unicode_byte_encoding_process` and `unicode_regex_split_stl` drive `std::regex` and
`std::string`. There is no policy in them to move - they are the C++ standard library doing its job, and replacing
that is a different project.

So the honest count for this file: everything that is a decision with pure inputs is ported and gated; nothing else in
it is.

## What is next, and why it is a different shape

`llama-vocab.cpp` (4830), `llama-graph.cpp` (4325), `llama-sampler.cpp` (3790), `llama-model.cpp` (2784) hold the bulk.
Each function there decides something about runtime state - a graph node, a sampler chain, a model tensor - rather than
about its arguments. The gates that worked here (compare against the native function over an enumerable domain) do not
transfer, because the domain is not enumerable and the state is not reproducible from a test harness.

The method that can transfer is narrower and should be stated before starting: pick a function whose decision is a
pure function of a small POD input, port it, gate over that input domain, delete the native body - the same shape as
above. Functions whose decision needs the live runtime are not candidates for this migration, and counting them as
"remaining work" is what makes a migration look endless.

Rule 17 says the scope is the modern 2026 architectures, 2025 selectively, legacy only with a reason. This boundary
applies that rule to the code: the decision surface is ported, the plumbing stays.

## Checked, not assumed: llama-batch has no decision surface to port

Following the boundary above, `llama-batch.cpp` looked like the best next candidate - 495 native lines spread over 40
functions, so mostly small ones. It is not a candidate, and here is the evidence rather than the impression.

`split_simple`, the 33-line batch splitter and the largest policy-shaped function in the file, mutates `used` and
`n_used` before calling `ubatch_add`:

    while (cur_idx < used.size() && used[cur_idx]) { ++cur_idx; }   // reads member state
    used[cur_idx] = true;  ++n_used;                                // writes member state

The 39 others are setters and allocators: `set_token_state`, `set_token_pos`, `set_token_id`, `add_token`, `add_seq`,
`clear`, `llama_batch_init`, `llama_batch_free`. There is no decision in them that a gate could compare - only
fields being assigned.

So the file stays C++, and that is the correct outcome under rule 17 rather than a gap. What remains genuinely
portable in the runtime is the NFC table transfer described above, which is one piece of work with a defined proof,
not a sweep over 15000 lines.

| `unicode-utf8.goose` | this commit | 1112064 | 0 mismatches | every codepoint encoded and compared byte for byte with the C++ encoder, then decoded back and compared; surrogates skipped, as they are not codepoints |
