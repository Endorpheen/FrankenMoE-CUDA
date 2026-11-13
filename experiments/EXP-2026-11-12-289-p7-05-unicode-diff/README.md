# EXP289: Unicode differential -- whole range identical, invalid-UTF-8 contract differs

One probe compiled twice: against the untouched flint `libllama` and against the accepted runtime's `stage-run` libraries.
Record layout per code point: code point, encoding length and bytes, `cpt_to_byte`, `tolower`, the raw `unicode_cpt_flags`
struct, `is_han`, `flags_from_utf8`, and `len_utf8` of the first byte.

## What matched

    code-point range: 1 114 112 code points, 62 390 272 bytes
    identical: True, first difference: none

So encoding, byte-token mapping, categories, case mapping, Han detection and UTF-8 length agree for every code point,
surrogates included (both arms raise there, and the raise is compared as a value).

## What differs: the invalid-UTF-8 error contract

Corpus: a byte-sequence file with valid encodings, then truncated, overlong, surrogate and random sequences. The first
invalid case already diverges -- a lone continuation byte `0x80`:

| arm | behaviour |
|---|---|
| native | throws `std::invalid_argument("invalid character")` |
| accepted runtime | `FG215U validation code 701`, then `trap` -- an abort, not an exception |

This is not cosmetic. `src/llama-vocab.cpp:1297-1299` calls `unicode_cpt_from_utf8` inside
`catch (std::invalid_argument &)`: that catch is the byte-fallback path for input that is not valid UTF-8. With a trap
instead of a throw, the catch never runs and the process dies.

## Status

P7-05 stays IN_PROGRESS: the tables, categories, case mapping and conversions are proven identical over the whole range,
but the error contract must be fixed before the item can pass its own criterion ("differential across valid/invalid
UTF8"). The fix is a module change (return a refusal code instead of trapping) plus a bridge that rethrows
`std::invalid_argument`, then this probe re-run to zero differences.

## Reproduce

    g++ -O2 -std=c++17 -I<flint>/src -I<flint>/include -I<flint>/ggml/include \
        -o probe_native unicode_probe.cpp -L<flint>/build-cuda/bin -Wl,-rpath,<flint>/build-cuda/bin -lllama
    g++ ... -o probe_goose unicode_probe.cpp -L<stage-run> -Wl,-rpath,<stage-run> -lllama
    ./probe_native /tmp/uni-native.bin byte_corpus.hex
    ./probe_goose  /tmp/uni-goose.bin  byte_corpus.hex
