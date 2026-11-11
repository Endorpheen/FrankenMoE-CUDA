# EXP286: CUDA snake and softcap in Goose

`snake_kernel` (`snake.cu`) and `softcap_f32` (`softcap.cu`), both named in P3-04's
remaining list.

- snake: `y = x + sin^2(a[c]*x) * inv_b[c]`, `c = idx / T`, F32/F16/BF16 data
  with F32 compute.
- softcap: `y = tanh(scale*x) * softcap`.

## Verdict

**OFF_PATH** — bit-exact against both native kernels in all supported types, but
the fixed 32K/128 Apex profile does not execute SNAKE or SOFTCAP. No performance
claim, shipped DSO unchanged.

## Scope transferred

All arithmetic and indexing is Goose. Externs are typed loads/stores and the
scalar `sinf`/`tanhf`, compiled in the same translation unit with the same
fast-math flags as the native kernels.

The native snake derives `c` with `fastdiv`; the fastdiv result is defined to be
the exact integer quotient, so Goose uses the division directly and keeps the
same value. This substitution is stated rather than implied.

Host: block 256, grid `ceil(n/256)` for both kernels, with pointer/extent/mode
checks.

## Gate

| check | result |
|---|---|
| snake: 4 T lengths (1,3,64,1000) × 3 channel counts × 3 dtypes | 36 outcomes per combination, all bit-identical |
| softcap: 5 sizes (1,255,256,257,10000) × 2 scales × 2 caps | all bit-identical |
| total values compared | 84 728 |
| failures / launch failures | 0 / 0 |
| host | 24 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| snake drops one factor of `s` | 13 882 |
| snake subtracts instead of adding | 13 882 |
| snake channel index off by one | 13 882 |
| softcap drops the cap factor | 43 050 |
| host grid uses 128 instead of 256 | 4 |

The channel mutation reads `a[C]`/`inv_b[C]`, so the fixture allocates `C+1`
entries; that keeps the mutation observable as a value difference instead of an
out-of-bounds read with undefined behaviour.

## Own defects found before the gate

Two oracle truncations in a row: the first softcap copy included the opening of
`softcap_f32_cuda` (the kernel ends at line 13, not 15), and the first snake copy
started at line 10, below the template line. Both produced cascading "identifier
undefined" errors inside the generated module. The link then needed a local
`ggml_abort` hook because the reference headers keep `GGML_ASSERT` active and the
fixture does not link `ggml-base`. The pattern is now familiar: when a copied
oracle misbehaves, inspect the copy's braces and its first line before the module.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
