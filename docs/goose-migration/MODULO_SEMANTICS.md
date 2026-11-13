# Division and modulo semantics: Goose is Euclidean, C truncates

Follow-up required by EXP246, where a direct transcription of `wrap_around` produced 30 wrong indices in
10234 checks because `(coord + size) % size` means different things in the two languages.

## The difference

| expression | C | Goose |
|---|---:|---:|
| `-5 % 10` | `-5` | `5` |

C's `%` keeps the sign of the dividend; Goose's returns a non-negative result. `/` diverges the same way for
negative operands (`-7/2` is `-3` in C, `-4` in Goose). Any expression containing either operator on a value
that can be negative must be rewritten explicitly, as `cmod` in EXP246 does:

    fn cmod(a, m) { if a < 0 { return -((-a) % m); } a % m }

## Sweep of the eight shipped routes

Searched every `*.goose` under the shipped stages (EXP237–244) for `/` and `%` in non-comment lines:

| stage | sites | verdict |
|---|---:|---|
| EXP237 split_equal | 0 | clean |
| EXP238 split_seq | 0 | clean |
| EXP239 classify | 0 | clean |
| EXP240 seqset | 1 | **safe: guarded** |
| EXP241 ubatch classify | 0 | clean |
| EXP242 out_ids / seq_idx | 0 | clean |
| EXP243 compat positions | 0 | clean |
| EXP244 output reorder | 0 | clean |

The single site is EXP240 `source/ss4.goose`:

    ensure(sid>=0 as i64 && sid<n_max, 2103);   // line 41
    let w:i64 = sid/(64 as i64);                 // line 42
    let bit:i64 = (1 as i64) << (sid&63 as i64); // line 43

The guard on line 41 makes `sid` non-negative, so `/` agrees between the languages at this point. Line 43 uses
`&63` (bitwise AND), which has no sign dependence for a non-negative operand. **Verified, not assumed** — the
guard is one line above the division.

## Why the earlier stages were not affected

All eight routes index arrays with values that are guarded or structurally non-negative. EXP246 was the first
port whose native code deliberately relies on C's signed modulo to produce a *negative* intermediate before the
`+ size` correction. That is why eight stages passed before the trap surfaced.

## Standing rule

Before committing any new `*.goose` module, check every `/` and `%`:

1. is the operand provably non-negative at that point — guarded, or a loop counter?
2. if not, replace with an explicit `cmod`-style helper rather than trusting the operator;
3. and when a native line looks like a plain `x % y`, ask what the native code does with a negative `x` —
   `pad.cu` needed the C rule exactly.
