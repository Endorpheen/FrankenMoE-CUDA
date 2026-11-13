# EXP290: Unicode decoder returns a refusal instead of trapping -- P7-05 fix

EXP289 found the defect: `fg215u` trapped on invalid UTF-8 (`validation code 701`) where native throws
`std::invalid_argument("invalid character")`, and `llama-vocab.cpp:1297-1299` relies on that exception to drive byte
fallback.

## The fix

1. **Module** (`source/cpt_refuse.goose`): the five validation sites now *return* a negative code (`-701` .. `-706`)
   instead of calling `fail`. The argument contract (`offset` inside the buffer) still traps, because that is a
   programming error rather than invalid input.
2. **Call site** (`unicode-patched.cpp`): `unicode_cpt_from_utf8` throws `std::invalid_argument("invalid character")` when
   the module returns a negative code -- exactly the exception native throws, so existing `catch` blocks keep working.
3. The bridge keeps the same ABI (`fg215u_cpt_from_utf8`, `fg215u_enabled`), so nothing else changes.

## Verification

- differential probe (EXP289 instrument) re-run against a staged `libllama` built with the fix:
  `cmp /tmp/uni-native.bin /tmp/uni-fixed.bin` -> identical, 67 679 364 bytes. That covers 1 114 112 code points plus the
  byte corpus of valid, truncated, overlong, surrogate and random sequences, with the throw behaviour now compared as the
  same value in both arms;
- accepted profile with the fixed library: `e68df05b...`, 128 tokens, clean shutdown.

A first build failed with `fatal error: cpt-generated.c: No such file or directory`: the bridge includes the generated
file relative to its own directory, and the generator had written it under `source/`. The file now sits next to
`bridge.c`, which is also where the reconstruction build expects it.
