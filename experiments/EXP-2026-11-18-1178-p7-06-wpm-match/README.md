# EXP-2026-11-18-1178: P7-06 WPM match search

The WPM session searches every word for the longest piece that the vocabulary recognises:
at position `i` it starts at `min(n, i + max_token_len + 1)` and walks down to `i + 1`. The
`+1` is preserved as it stands, because it decides which pieces are considered for a word
longer than `max_token_len`, and "tidying" it would change tokenization silently.

`src/wpm_match.goose` returns the chosen length, or minus one when nothing matched. The
mask is indexed by absolute position, mirroring the native loop's index.

```
wpm match: cases=7 fails=0
```

Seven cases: longest wins, shorter when the longest is absent, the bound by `n`, a mask entry
exactly at `i + maxlen + 1` (which is why the `+1` matters), a non-zero start offset, an empty
mask, and `max_token_len = 0` where only a single byte can match.

Two of my rows were wrong first, both times because I read the mask as a length rather than
as an absolute position. The reference transcription disagreed and the run said so.
