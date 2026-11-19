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

## Types and values

```
integer keys: 26/26 equal
key types:    52/52 equal
string values: 13/13 equal
```

Every integer-valued key decodes to the native value, every key reports the native type code, and every string key's
bytes are identical. Together with the 52 keys, 753 tensor names and five header fields, the metadata of this model
is proven equal field by field.

The remaining thirteen keys are covered too, by their own doors:

```
floats: 4/4   bools: 4/4   arrays(type,count): 5/5
```

So every one of the 52 metadata keys is compared by its own type - 26 integers, 13 strings, 4 floats, 4 booleans and
5 arrays - and the whole file's metadata is equal field by field.

Array **contents** are compared too, starting with the largest:

```
vocabulary array: 248320/248320 elements equal
```

Every token string of the model read through `gr_arr_str_span` matches the native reader byte for byte - the same
248 320 tokens that the detokeniser door was proven on, now proven at the source.

What remains inside the arrays is the element content of the other four: the merge list and three small ones.
`gr_array_int` and `gr_arr_str_span` cover them and the same harness extends to them directly.

## Three harness errors before the numbers were right

Each was caught by the numbers looking wrong in a characteristic way, not by review:

1. `gr_kv_type` called with two arguments instead of three - **all 52** returned -1, which is what a uniformly wrong
   signature looks like;
2. `gr_val_span` given two output slots when it requires four - every string returned -1;
3. the span layout assumed `out[1]` was the length; it is the payload **offset**, with the length in `out[2]`. The
   give-away was a five-byte string reported as 105 bytes.

The third one is the instructive one: 105 is not a plausible length, but a harness that only checked `rc == 0` would
have recorded 13 successful string comparisons.

## What this establishes

The ported header parser agrees with the native reader on this model's structure: version, counts, alignment and
where the data begins.

## What it does not establish

That the values attached to those keys are equal - `gr_meta_int` and the typed accessors exist for that and are the
next comparison. Names, indices and the header fields are proven; payloads are not. And, as with the detokenizer, the runtime still parses
GGUF natively: this is a port that has been proven against the real file, not one that has been connected.
