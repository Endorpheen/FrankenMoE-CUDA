# EXP-2026-11-19-1315: the detokenizer is composed; the first cross-check counted my own table, not the port

## What was built

The detokenizer module and its dependency closure now compose into the unit the shipped library is built from, so
eight `bt_*` doors exist in the artefact where none did (EXP-2026-11-19-1314). This card is the attempt to give them
a call path.

## The cross-check, and the numbers it produced

A hook in `llama_vocab::impl::token_to_piece(token, buf, ...)` - the overload the server calls for every emitted
token - built a token table once from the vocab's own piece cache and compared the port's bytes against the native
piece behind `FG_TK_DETOK_DIAG`:

```
FG_TK_DETOK_TOTAL calls=1082 disagreements=566
FG_TK_DETOK_DIFF token=220 native=1 goose=16 native_hex=20 goose_hex=[UNK_BYTE_0x20 ]
```

## Why that number is not a finding about the port

The printed bytes say it. Token 220 is a single space; the port answered with sixteen bytes of
`[UNK_BYTE_0x20 ]` - its marker for a byte it could not decode as UTF-8. A one-byte piece cannot fail to decode
unless the bytes handed to it are not that piece. The next example shows the same shape for a CJK token: the port
emits an unknown-byte marker for the **same** three bytes it then prints correctly.

So the offsets and lengths I built from `cache_token_to_piece` do not describe the blob the door expects, and the
count measures my table, not the port's logic. The instrumentation was reverted rather than left in: a diagnostic
that produces 566 numbers about itself is worse than no diagnostic.

## The rule this is the third instance of

Printing the two sides before interpreting the count is what caught it, and it took one rebuild. Had the summary
been written from `disagreements=566` alone, the card would have recorded a port defect that does not exist.

## What the next attempt must do first

Prove the table before counting anything: for a handful of known tokens (`the`, a space, a CJK piece, a control
token) assert that the blob slice equals the native piece byte for byte. Only then is a disagreement attributable to
the port. The door's own module already carries a verifier over arrays it built itself; what is missing here is the
translation layer between the vocab's cache and the door's view of the token text.
