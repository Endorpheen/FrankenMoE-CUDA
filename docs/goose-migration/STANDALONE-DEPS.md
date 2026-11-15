# What the standalone runtime actually depends on

Base: this commit. Written because the build script claimed "no libstdc++" while `ldd` showed
libstdc++ in NEEDED, and the difference turned out to be substrate, not ours.

## Measured

```
$ ldd builds/goose-standalone/frankenmoe-v4
  libstdc++.so.6, libm.so.6, libgomp.so.1, libgcc_s.so.1, libc.so.6
```

29 undefined C++ symbols, all of them from the substrate.

## Why libstdc++ is there, and why it is not our debt

`libggml-base.a` contains six C++ objects:

```
ggml.cpp.o  ggml-backend.cpp.o  ggml-backend-meta.cpp.o  ggml-opt.cpp.o
ggml-threading.cpp.o  (+1)
```

`ggml_critical_section_start` in `ggml-threading.cpp.o` references
`std::__throw_system_error(int)`. Linking with `$CC` therefore fails:

```
undefined reference to `std::__throw_system_error(int)'
```

So libstdc++ is a **substrate** dependency. Removing it would mean rewriting ggml's threading,
which is the substrate and is out of scope: rule 17 keeps the substrate, and the goal is a Goose
runtime, not a reimplementation of ggml.

The build script's comment claiming "no libstdc++" was simply wrong and has been replaced with
the measured reason.

## What WAS ours, and is now C

Both of the standalone's own C++ translation units were converted to plain C in this commit:

| file | before | after | what had to change |
|---|---|---|---|
| `graph-bindings` | 305 lines .cpp | 305 lines .c | removed `extern "C" {}`, `nullptr` -> `NULL` |
| `tensor-data` | 129 lines .cpp | 129 lines .c | removed `namespace {}` and `extern "C" {}`, `constexpr` -> `#define`, `sl_i64{...}` -> `(sl_i64){...}` |

Neither used any C++ feature - no classes, no templates, no `std::`. They were `.cpp` files
compiled with a C++ compiler for no reason, and both were on the technical-debt list as adapters.

Verified after conversion: both objects compile with `gcc -std=c11`, and neither imports a C++
symbol (`nm -u | grep _Z` returns 0 for both). The runtime still links with `$CXX` because the
substrate requires it, not because our code does.

## The claim this binary can now support

- Our own code: **100% C**, no C++ sources in the standalone.
- Dependencies: `libm`, `libc`, `libgomp`, `libgcc_s`, `libstdc++`, and the ggml static
  libraries statically linked in.
- Not claimed: zero C++ in the process. That would require rewriting ggml's threading, and
  `libstdc++` would still be a C++ runtime library by definition.

This is the honest boundary: every line we wrote is C, and the remaining C++ is a substrate we
did not write and are not replacing.

## Gates after the change

```
FG_GRAPH_EXEC ADD  backend=cpu result=[11 22 33 44]   PASS
FG_GRAPH_EXEC MUL  backend=cpu result=[10 40 90 160]  PASS
FG_TENSOR_DATA rc=0 tensors=65 copied=115304 layout=0 mismatches=0
```

Same results as before the conversion, so the C rewrite changed no behaviour.
