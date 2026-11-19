# EXP-2026-11-18-1179: P7-06 tokenizer text normalization

`llama_vocab::normalize` is the step every tokenizer family runs before splitting, and it has
four interacting flags: escaped space, prepended space, merged spaces and a suffix space. The
subtle part is that with `merge_spaces` the emitted space belongs to the **transition into a
non-space**, not to the space byte itself, and a run of spaces produces exactly one.

```
normalize: cases=11 fails=0
```

## The gate caught a real defect

The first run failed on the `no escape` case:

```
goose (6): E2 96 81 61 20 62
ref   (4): 20 61 20 62
```

My port had hardcoded the three-byte escaped space in the prepend and suffix paths, so with
`escape_space` off it still emitted `U+2581` where the native code emits a plain space. Both
paths now depend on the flag. This is the first time in these slices that the gate caught my
code rather than my test data, and it was found because the case existed - a suite that only
tested the default flags would have passed.

Cases: eleven, covering each flag alone and in combination, a whitespace-only input (which
must emit nothing when merging), an empty input with and without merging, and empty with
suffix.

The reference is the native algorithm transcribed line by line; the comparison is on the
exact bytes, printed in hex on failure, because a whitespace difference is invisible otherwise.
