# EXP-2026-11-19-1327: the ported GGUF parser reads the real model header exactly as the native reader does

The GGUF reader's doors are composed into the shipped library but no C code calls them yet (EXP-2026-11-19-1326).
This card gives them their first comparison against the native reader, on the real 20 GB model file rather than on
fabricated arrays.

## The comparison

`gr_parse` takes the file's first 64 MiB window and reports version, key count, tensor count, alignment and data
offset in its state array. The native side is `gguf_init_from_file` from the same build, asked the same questions.

```
port   rc=0 version=3 nk=52 nt=753 align=32 data_off=10990944
native          version=3 nk=52 nt=753 align=32 data_off=10990944
```

All five values agree on a file with 52 metadata keys and 753 tensors.

## Two harness errors, both caught before the result was read

1. the file was written with C++ brace initialisers in a `.c` file, so it did not compile;
2. the success check compared the **return value** against 919, while 919 is the marker the parser writes into
   `state[0]` and the return is a byte count. The first run therefore printed `PARSE REFUSED` while every value
   already matched - which is exactly the shape of error that would have become a false finding if the numbers had
   not been printed alongside.

Both are the same lesson as the rest of this session, applied one step earlier: print what each side produced before
interpreting the verdict.

## Every key and every tensor, by name

The lookup doors are compared for the whole file: for each index the native reader knows, its key or tensor name is
looked up through the port, which must return the same index.

```
lookup: kv=52/52 tensor=753/753 fails=0
absent key: both report -1
```

All 52 metadata keys and all 753 tensor names resolve to the index the native reader gives them, and a key that does
not exist is reported absent by both sides - the negative case matters as much as the positive one, because a lookup
that returns zero for everything would pass a positive-only check.

## What this establishes

The ported header parser agrees with the native reader on this model's structure: version, counts, alignment and
where the data begins.

## What it does not establish

That the values attached to those keys are equal - `gr_meta_int` and the typed accessors exist for that and are the
next comparison. Names, indices and the header fields are proven; payloads are not. And, as with the detokenizer, the runtime still parses
GGUF natively: this is a port that has been proven against the real file, not one that has been connected.
