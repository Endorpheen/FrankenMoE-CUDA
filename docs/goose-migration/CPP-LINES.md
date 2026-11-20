# Physical C++ source line baseline

Baseline on 2026-11-20. The open branch was `main` at commit `25189ba40b1cc7d66917bfa3435be9e10dc9088d`.

| Source scope | Current checkout on disk | Tracked in the baseline commit |
|---|---:|---:|
| C++ and CUDA source and headers with explicit extensions | 5,797,369 | 1,480,954 |
| Shared `.h` headers | 1,487,796 | 221,187 |

The default scan recursively reads regular source files from the current `main` checkout. It includes vendor, generated, archived, ignored and untracked sources. It skips Git metadata, symbolic links and independent nested Git working trees. Each run reads the file contents afresh. The baseline scan read 13,094 files in 3.04 seconds and the repeated scan matched in 1.47 seconds.

The original local snapshot is `build/cpp-lines-baseline.json`, SHA-256 `346dbe60a82e9e9c2461e5480103c1bdc17ba8699b10951696555962aaebeb90`.

Counts are physical lines, including comments, blank lines and a final line without a newline. Shared `.h` files are counted separately because the extension is used by both C and C++.

```bash
python3 tools/cpp-line-count.py
python3 tools/cpp-line-count.py --save build/cpp-lines-baseline.json
python3 tools/cpp-line-count.py --against build/cpp-lines-baseline.json
python3 tools/cpp-line-count.py --git <before-commit> HEAD
```

Run from `main`. The on-disk snapshot records counts and SHA-256 hashes by file. The committed comparison reads Git blobs from the first-parent history of `main` and reports exact renames across extension groups separately. Report the preceding total, net physical lines removed, and resulting total after each C++ deletion. Use the same scope on both sides of a comparison.
