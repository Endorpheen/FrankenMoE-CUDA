# EXP-2026-11-19-1273: the CSV encoder and unescaper as a pair

```
csv pair: cases=4000 fails=0 (fields with a newline: 961)
```

## Why this card compares the two ports with each other

`mt_count_cols` and `mt_unescape_field` serve the analysis tooling, and that tooling's consumer is **not in the
tree** - so unlike every other metrics decision there is no native function to compare against, and inventing a
transcription would be the mistake this session has recorded eight times.

What can be checked honestly is the **invariant between the pair**, and it has a subtle shape:

- a field without a newline must round-trip **exactly** through encode→unescape;
- a field **with** a newline must come back with it replaced by a space, because that is what the encoder writes
  (verified against the real writer in EXP-2026-11-19-1267).

961 of the 4000 cases contain a newline, so the lossy arm is exercised rather than assumed. A round-trip that
returned the newline would mean the unescaper disagrees with the writer the sink actually uses - which is exactly
the defect a "looks right" inspection would miss.

Each encoded field is also required to count as **one** column, which ties `mt_count_cols` to the encoder's
quoting rules: a field whose quotes the counter mishandled would split into two.

## Statement of limitation

This is an invariant check, not equality with native code. It catches asymmetries between the two helpers and
disagreement with the writer they serve; it cannot say anything about a consumer that does not exist here. The
card says so rather than presenting the green number as reference evidence.
