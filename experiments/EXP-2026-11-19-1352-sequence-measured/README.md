# EXP-2026-11-19-1352: the sequence form measured once, equal - and why it is one case, not a sample

## The measurement

The comparison now sits at the public `llama_detokenize`, where the buffer belongs to the caller and is not advanced -
the two earlier attempts failed precisely because they compared a pointer that had walked the string
(EXP-2026-11-19-1320, -1321).

First run, with the door given a buffer sized from the native result:

```
FG_TK_SEQ_DIFF  n_tokens=32768 native=184528 goose=-65544
```

That was my harness: the door had 64 KiB and the sequence needs 184 528 bytes, so it reported a short buffer and the
comparison measured the buffer rather than the decision. After sizing:

```
FG_TK_SEQ_PROGRESS calls=1 disagreements=0
```

A 32 768-token sequence detokenised identically - the largest single case in the report, and **one** case.

## Why there is only one

`llama_detokenize` is a library API, and the server does not call it: `/detokenize` answers 200 for five different
token lists, and the counter does not move once. So the sequence form is exercised by whatever inside the load path
uses it, and by nothing the endpoints do.

That makes this door the least covered in the report and the honest label is: **equal on one 32 768-token sequence,
not sampled**. It is not "unmeasured" any more, and it is not "proven" either.

## What would turn it into a sample

A harness calling `llama_detokenize` directly over generated token lists of varied lengths, with the flags varied -
the same shape as every other harness in this report. The door takes 15 parameters and the flags matter (remove_special,
unparse_special, add_space_prefix, add_bos/add_eos), so the harness needs those combinations enumerated rather than
sampled at random.

## State

| piece | state |
|---|---|
| piece cache, buffer form, end of generation | served by the port |
| sequence form | equal on one large case; **not converted**, and templating it needs the harness above |
