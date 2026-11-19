# EXP-2026-11-19-1335: the ported GGUF reader now has a call path from the runtime

## What was missing

The GGUF port was proven completely against the native reader on the real model file - header, 52 typed keys,
744 231 array elements, 753 tensor names and descriptors, tensor sizes - and reached by **nothing**. That is the
state rule 33 calls debt rather than progress (EXP-2026-11-19-1324).

## The connection

`llama_model_loader` now calls `gr_parse` after the native reader has parsed the same file, and compares the two
views. Behind `FG_GGUF_DIAG`, so the shipped path pays one `getenv` read and nothing else.

A server started with the flag:

```
FG_GGUF_PARSE nk=52 nt=753 align=32 off=10990944 disagreements=0
```

Key count, tensor count, alignment and data offset all agree with what the native loader reported for the same file,
during a real start.

## Acceptance with the hook compiled in and the flag off

```
decode  62.49 / 62.87 / 62.74
prefill 2697.6 / 2948.3 / 2958.5
hash    c2ff42cc1219
```

## What this is and is not

Is: the first call path from the shipped runtime to the GGUF port, which is what the coverage report was asking for.

Is not: a replacement. The loader still takes its numbers from the native reader, and the port's values are compared
rather than used. Replacing the source of those numbers is a separate change with its own acceptance, and this card
deliberately stops before it - the same order the detokenizer took, and for the same reason: prove the call path
before making it load-bearing.

## Two insertion errors, both mine

The first attempt failed before writing anything - a `std::string format(` marker that this file does not contain -
and the second assumed the anchor line was unique when it appears twice. Both were caught because the build ran after
each edit: the first left the tree untouched, the second was fixed by indexing the first occurrence explicitly.
