# goose-determinism

**Requirement:** P10-06 - reproducibility of the generated C, listed as its own item.
**Produced by:** the P10-06 state note of 2026-11-19; this is its executable half.
**Tier:** integration - composes four modules and generates C twice, no model, no GPU, seconds.

## What it proves

Two things, both byte-level:

1. the composer produces the **same** composed file twice from the same module list;
2. the compiler produces the **same** C and the **same** header twice from the same composed file.

Result on the four Qwen modules: compositions identical; `unit.c` identical (259 567 bytes, sha256
`62f2c2bc…`); `unit.h` identical (4 830 bytes, sha256 `ae24e6c1…`).

## The first version of this test was wrong, and said so

It composed into two different directories and compared the generated C. Two lines differed, and
both were the **source path** the compiler embeds (`…/build/a/unit.goose` against
`…/build/b/unit.goose`). The compiler was deterministic; the test was not comparing like with like.

That is why the corrected version generates both outputs from **one** composed file: a check that
fails for a reason that says nothing about the property under test is worse than no check, because
it teaches the reader to ignore the failure.

## What it does not prove

That the whole cumulative unit is deterministic - this uses a representative four-module set,
because generating the full 160-module unit twice is minutes of work for the same property. It also
does not compare across machines or compiler versions; it compares two runs of one pinned compiler,
which is what P10-06 asks for.

## Run

```
bash tests/integration/goose-determinism/run.sh
```

Exit 0 when both comparisons hold; 77 without a compiler.
