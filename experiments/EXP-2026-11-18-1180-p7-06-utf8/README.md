# EXP-2026-11-18-1180: P7-06 UTF-8 accounting in the byte fallback

Two rules from the score-based tokenizer path: the byte count of a codepoint, and the bytes
of its encoding when a piece is unknown and falls back to byte tokens.

```
utf8: fails=0
```

The gate compares against a real UTF-8 encoder plus a dense sweep across 0x7FF..0x8010, not
against a re-typed expression.

## The gate caught my port twice

The first version returned only the lead or continuation **marker** and dropped the payload
bits, so every multi-byte encoding differed:

```
BYTE mismatch cp=233 i=0 got=192 want=195
```

The native line is `bytes_[b | ((c >> ((s - i - 1) * 6)) & 0x3F)]`, and the payload term is
what I had lost - seventeen mismatches over eleven codepoints, all in the same direction.

The second version was worse before it was better: writing the OR without a bitwise operator
produced a long arithmetic expression that happened to be unreadable and only accidentally
close. It is now `base + payload` with the reason stated in the source - the two terms occupy
disjoint bits, so the sum *is* the OR - and that reason is what makes the simplification
trustworthy rather than lucky.

This is the second consecutive slice where the gate caught my code rather than my data, and
both times the case that caught it was the non-obvious one (no escaping, multi-byte).
