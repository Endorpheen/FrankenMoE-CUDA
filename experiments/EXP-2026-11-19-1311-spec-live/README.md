# EXP-2026-11-19-1311: the speculative prefix door agrees on real drafts from the real model

The speculative doors were verified against arrays built by a harness. This card reaches them during real generation.

## What had to be got right first, and what I misread

The first attempt enabled `--spec-type ngram-simple` with an ordinary prompt and saw **no** prefix calls. The server
log carried a line I misread:

```
FG_SEQ_RM type=2 (0=no, so speculative is disabled)
```

`type=2` is `COMMON_CONTEXT_SEQ_RM_TYPE_FULL`, the enum member that means the context **can** remove full sequences -
speculation is supported, and the parenthetical in the message is generic. The real reason there were no calls is
that nothing was drafted: a 41-token prompt with no repetition gives an n-gram drafter nothing to propose.

## What produced drafts

The same lifecycle with `--spec-type ngram-map-k,ngram-simple` and a prompt built from a repeating block
(`A B C D` several times), 128 predicted tokens, temperature 0.2:

```
FG_PREFIX_ENTER     draft=48 result=49 fn=0x7d7deb7b6710
FG_PREFIX_ENTER     draft=13 result=14 fn=0x7d7deb7b6710
FG_PREFIX_RETURNED  goose=48 i=48 k=48
FG_PREFIX_RETURNED  goose=13 i=13 k=13
FG_SP_PREFIX_TOTAL  calls=2 disagreements=0
```

The pointer is non-null, so the door was resolved through `dlsym(RTLD_NOLOAD)` and called with the struct-by-value
ABI it requires - the same ABI that crashed when it was first written with flattened pointer/length pairs. The port's
answer equals the native count on both calls, and the door's own total reports **2 calls, 0 disagreements**.

## What this establishes

The prefix comparison runs on real drafts produced by the shipped server, returns the native count, and the accepted
prefix is the native one - the native result is what returns in any case, so a disagreement could only have been
counted, and none was.

## What it does not establish

Two calls is a witness, not a sample. The doors for synthesis and rejection acceptance are still only exercised
synthetically, and the baseline profile sets `--spec-type none`, so nothing here changes the accepted numbers.

## Housekeeping

The server ran in its own session on port 9101, was terminated by its process group, and the port was checked
afterwards. The unrelated `igorek-llama-server` on 8022 was not touched.
