# Compiler switch: goose-2026.3-string (runtime 1e714e6146bc)

Owner directive: this repository uses the `goose-2026.3-string` binary from now
on. The previous pin is kept as the recorded rollback.

## Installed binary

| field | value |
|---|---|
| path | `/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose` |
| binary sha256 | `664d4de42463cc10cb9c4a61dac59ce6023faa9c9da2838fddc0f4fb8294b3ab` |
| release archive | `goose-2026.3-string-linux-x86_64.tar.gz`, sha256 `5c6fdf169408f687a49f3f9ce3ebc0804ece16da0320bbbcb893721f5422558b` (matches the published `.sha256`) |
| `goose version` | `goose runtime 1e714e6146bc` |
| source commit | `e0501dc7dee4522428a46f813cebbce0ad62736e` |
| rollback | `runtime/compiler/current.json` keeps the previous path and hash |

The standard library ships next to the binary; `import std;` resolves without
`--stdlib` or a working-directory symlink.

Build caches were cleared (`rm -rf ~/.cache/goose/build`) as the release notes
require when moving from the previous version.

## What changed for our harness

The old harness parsed `gg_function_N` prototypes out of the generated C. The
new compiler names program symbols after the source and splits the runtime:

- functions are exported for C with `export fn` and declared in a generated
  header (`--header out.h`); slices appear as `sl_u8 { uint8_t *data; int64_t len; }`,
  the same `{data, len}` ABI as before;
- `goose_init(argc, argv)` must run before exported functions are called;
- by default the C references a separate runtime object, so fixtures use
  `--standalone`, which embeds the runtime in the translation unit and links with
  a plain C host;
- the compiler now warns about redundant `as i64` casts and rejects implicit
  argument conversions that the old binary accepted (see EXP446 below).

## Compatibility sweep

Every module written in this migration session was compiled with the new binary:

| experiment | compiles | note |
|---|---|---|
| 445 startup policy | yes | |
| 446 pre-routing | yes, after a fix | `ld_u8(src, 2 as i64)` was an implicit i64→u64 argument; now `as u64` |
| 448 shutdown policy | yes | |
| 449 route table | yes | `import std` resolves from the binary's directory |
| 450 request errors | yes | |
| 451 required fields | yes | |
| 452 multipart | yes | |
| 453 stream policy | yes | |
| 454 stream manager | yes | |
| 455 SSE framing | yes | |
| 456 GCP/props | yes | |
| 457 GGUF offsets | yes | |
| 458 file I/O | yes | |
| 459 shards | yes | migrated to the new export/header/standalone pattern |

Modules without a `main` (`-o out.c` requires one) are built with
`--standalone ... --header` instead; that is the intended C-host path.

## Migration state of the gates

All fourteen experiments of this session are migrated to `export fn` +
`--header` + a shared `--emit-runtime` object, and every gate and mutation set
was re-run on the new binary. The recorded numbers reproduce exactly:

| experiment | gate | mutations |
|---|---|---|
| 445 startup | 69 132 cases / 553 056 values / 0 failures | 6/6 |
| 446 pre-routing | 352 665 / 2 115 990 / 0 | 7/7 |
| 448 shutdown | 340 / 940 / 0 | 8/8 |
| 449 routes | 429 / 429 / 0 | 6/6 |
| 450 request errors | 64 / 64 / 0 | 6/6 |
| 451 required fields | 172 049 / 172 049 / 0 | 7/7 |
| 452 multipart | 20 030 / 132 335 / 0 | 6/6 |
| 453 stream policy | 1 464 / 5 064 / 0 | 8/8 |
| 454 stream manager | 33 823 / 71 135 / 0 | 7/7 |
| 455 SSE framing | 1 652 / 2 552 / 0 | 7/7 |
| 456 GCP and props | 111 / 170 / 0 | 10/10 |
| 457 GGUF offsets | 2 109 / 2 109 / 0 | 8/8 |
| 458 file I/O | 8 275 / 19 629 / 0 | 7/7 |
| 459 shards | 234 / 406 / 0 | 8/8 |

Migration notes that will matter for the next module:

- the generated C no longer contains `gg_function_N`; the harness adapters call
  the exported names through a small `tests/ffi-adapter.h` that also supplies the
  `fg*_st_*` extern stubs with C linkage (C++ fixtures need `extern "C"`);
- an exported function needs a concrete signature: `string`/`u8[:]` become
  `sl_u8 { uint8_t *data; int64_t len }`, `i64[:]` becomes `sl_i64`;
- `main` must be gone, otherwise `goose_init` runs it with zero arguments and the
  fixture crashes before the first check (that happened in five experiments and is
  why the strip step now brace-matches instead of using a regex);
- two program objects both define `goose_init`; the second is renamed with
  `objcopy --redefine-sym` and called explicitly (`goose_init_props`) so each
  module's initialiser runs once over the shared runtime;
- 449's table generator now emits `export fn` and no `main`, so its
  generator-level mutations still take effect;
- the compiler rejects implicit argument conversions; `ld_u8(src, 2 as i64)`
  became `2 as u64` in 446.

All gated experiments now have a `build_fixture.sh` that emits into `source/`,
compiles the shared runtime once per experiment, and links the fixture. The
committed `*-gen.c` and `*-gen.h` are produced by the pinned binary recorded in
`runtime/compiler/current.json`.

## Template for migrated experiments

    # module: export fn with concrete signatures, no main
    goose --standalone -o source/<name>-gen.c --header source/<name>-gen.h source/<name>.goose
    gcc  -O2 -std=c11   -c source/<name>-gen.c -o build/gen.o
    g++  -O2 -std=c++17 -c tests/<name>_fixture.cpp -o build/fixture.o
    g++  build/fixture.o build/gen.o -o build/fixture

The fixture includes the generated header, calls `goose_init(0, nullptr)` first,
and passes slices as `sl_u8`. Each migrated experiment writes `provenance.json`
with the compiler hash, `goose version`, source hash and generated-C hash.
