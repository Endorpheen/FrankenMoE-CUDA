# EXP220: cumulative llama-vocab.cpp and unicode.cpp — second chain repair

EXP219 fixed the `unicode.cpp` collision. The same mistake had a second site, found by auditing the
object files instead of trusting the witness list.

## The loss

`llama-vocab.cpp` is another single translation unit with more than one route in it. EXP202 put the
BPE tokenizer decisions there (22 references) and also a `unicode_len_utf8` route in `unicode.cpp`.
EXP213 then copied the pristine `llama-vocab.cpp` and added only its `try_copy` route.

Evidence, not inference:

    exp213 run : FG202T_TOKENIZER, FG213P_PIECE_COPY=321941     both present
    exp217 run : FG213P_PIECE_COPY=321941, no FG202T            tokenizer gone
    exp219 run : same                                            still gone

`nm -u runtime/exp213/build/llama-vocab.o` shows only `fg213p_`, yet the EXP213 *runtime* contained
15 `fg202t_*` symbols. So the object currently on disk is not the one that produced the passing run —
the cumulative build was overwritten or never saved, and nothing recorded it. That is why the loss
survived two stages unnoticed: the object looked authoritative and the witness check was run without
a baseline.

## The repair

Both files rebuilt cumulatively from the pristine source:

- `unicode.cpp` — EXP202 `utf8_len`, EXP215 `cpt_from_utf8`, EXP216 `cpt_to_utf8`,
  EXP217 `cpts_from_utf8`, EXP218 `tolower`
- `llama-vocab.cpp` — EXP202 tokenizer decisions (22 references) + EXP213 `try_copy`

All five witnesses in one run:

    FG202T_TOKENIZER   FG213P_PIECE_COPY calls=321941   FG215U calls=256
    FG216E calls=268   FG217C calls=255553

`--baseline exp219`: no missing, `new FG202T`, verdict OK.

## Result — ACCEPTED

    2567.91 / 26.78, tokens and all 128 probabilities exact, sha e68df05b

| pair | prefill | decode |
|---|---:|---:|
| 1 | +0.33% | +0.49% |
| 2 | −0.46% | +4.05% |
| **mean** | **−0.07%** | **+2.24%** |

Prefill is flat. Decode's mean is +2.24% but it rests entirely on pair 2, and the within-arm spread
is the honest caveat: A `[26.34, 25.30]` against B `[26.47, 26.33]`. A's spread is 1.04 tok/s, larger
than the effect. Read as **no regression detected**, not as a 2% win.

## Process note

Two stages were needed because I audited files, not builds. The rule that would have caught both
immediately: **check the object files for the expected route symbols before linking a new stage**,
not only the witness lines afterwards. `nm -u` on each `*/build/*.o` is a two-second check.
