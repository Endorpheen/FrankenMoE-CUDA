# EXP-2026-11-19-1393: 73 live comparisons through a server, and the no-bos path - no disagreements

## The server run

A server with `FG_TK_BPE_DIAG`, chat templates on, five requests spanning ASCII, Cyrillic, Japanese, code and a
hand-written special marker. Every `llama_tokenize` the server performs is compared:

```
FG_TK_BPE_COMPARE calls=73 disagreements=0
FG_TK_BPE_DIFF          (none)
```

Seventy-three calls is not a corpus sweep, but it is the first comparison driven by something other than a harness: the
**chat template** builds the strings, so the specials, the role markers and the boundaries come from the model's own
template rather than from text written by hand for the test.

## The library-side no-bos path

The same four probe cases with `add_special = false`:

```
case 0..3: 2, 5, 8, 2 tokens
FG_TK_BPE_COMPARE calls=1 disagreements=0
```

Equal in both flag states. For this vocabulary the flag is a no-op - it declares no BOS - so this establishes that the
path does not diverge, not that a BOS-adding vocabulary would behave the same. That distinction is stated rather than
blurred.

## What is now closed before serving

| item | evidence |
|---|---|
| equality on the shipped `llama_tokenize` | four probe cases, EXP-1391 |
| the special-token case | fixed by the scan in Goose, EXP-1391 |
| the pre-type guard | permits Apex, refuses a patched family, EXP-1392 |
| a template-driven comparison | 73 live calls, 0 disagreements, here |
| the no-bos path | equal, with the caveat that this vocabulary does not add BOS |

## What serving still needs

The conversion itself: the port's ids returned instead of the native ones, behind the guard, with the native path kept
for every family the port does not implement. Then the acceptance triple with the hash - and the hash is the part that
matters, because a tokenizer that is equal on probes and different on a 32K prompt would show up there and nowhere else.

## Shipped path

Unchanged; everything behind the flag. Acceptance 62.52 / 63.13 / 62.96, hash `c2ff42cc1219`.
