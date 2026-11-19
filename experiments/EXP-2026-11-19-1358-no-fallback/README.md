# EXP-2026-11-19-1358: the detokeniser has no native path left on its successful route

## What was removed

Both converted forms used to call the native function whenever the door reported a short buffer. With the door's
negative now equal to the native's (EXP-2026-11-19-1357), that condition went away: the door's answer is served
whichever sign it has, and the native call remains only for the case where the door cannot answer at all - the shared
table not ready, or `bt_detokenize` unresolvable.

## The measurement, and the false alarm inside it

First run after the change printed:

```
FG_TK_SEQ_DIFF n_tokens=129 native=1015 goose=-1015
```

A disagreement that was not one: my diagnostic compared the door's short-buffer answer against a native call that had
a 64 KiB buffer and therefore succeeded. The comparison was corrected to give the native the same constraint as the
door, and the line disappeared - the third instrument error of this kind in the report, and the same diagnosis:
compare both sides under identical conditions or the verdict describes the harness.

The short-buffer case itself:

```
len_max=8    door -1015, native -1015
len_max=64   door -1015, native -1015
```

## Acceptance

```
decode   63.10 / 63.18 / 62.67
prefill  2950.9 / 2941.5 / 2696.8
hash     c2ff42cc1219
```

Accepted components: all present.

## Where the detokeniser stands

| piece | from the port |
|---|---|
| piece cache (248 320) | yes |
| buffer form, including a short buffer | yes |
| end-of-generation decision | yes |
| sequence form, including a short buffer | yes |

The only native call left on this path is the unresolvable-door guard, which cannot be exercised while the library is
linked against itself.
