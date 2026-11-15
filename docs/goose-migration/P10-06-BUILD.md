# P10-06 — deterministic Goose build, generated C/CUDA and packaging

Status: **PARTIAL** — the deterministic build and unit packaging are proven; the
CUDA artefact and install/release tails are open.

## Deterministic build

`tools/build-main-runtime.sh` composes every accepted compatible module,
typechecks the unit, generates C, compiles it at -O1 and fails on any duplicate
global symbol. `--repro` builds twice into the same output directory and fails
unless `main.c` and `main.o` are byte-identical.

Measured on 2026-11-15: 84 modules, 3306 specializations, 17 971 868 bytes of C,
4 567 224-byte object, 765 global symbols, 0 duplicates, 99 extern entry points,
`repro: ok`.

The generated C embeds only the output path (in `gs_file0`), so byte-level
comparison requires a fixed `OUT`; the composer and the C generator are
byte-identical for the same input.

## Packaging

`tools/package-main-runtime.sh` builds the unit and writes
`build/goose-main/dist/` with:

- `main.goose`, `main.c`, `main.h`, `main.o`;
- `SHA256SUMS` over the four files;
- `manifest.json` with the compiler version, module count, C/object sizes,
  global and extern symbol counts and per-file hashes.

## Remaining

CUDA generated artefacts and the install/release tails (dist install rules,
signing, release assets beyond the unit package).
