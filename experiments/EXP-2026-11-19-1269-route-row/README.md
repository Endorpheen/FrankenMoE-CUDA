# EXP-2026-11-19-1269: the route row, and a broken library I caused and fixed

## The differential

```
route row: cases=3000 fails=0
```

3000 rows written by the real `CsvRouteTraceSink` through a `FILE*` and by the ported `mt_route_row_encode` into
a buffer, compared byte for byte, with the weight drawn to reach every arm: ordinary, 0, 1e-7, 123456 and NaN.
The NaN arm is the one worth having: the native prints the literal `nan` rather than `0` so that "unknown" cannot
be read as "the router gave this expert no mass", and a hand-written `%.6g` would print `nan` too - by accident,
not by decision. The port's `mt_weight_kind` is checked against the arm the native actually took.

## The defect this exposed, and it was mine

Adding `metrics-traces.goose` to the chain build made the **library unloadable**:

```
libllama.so.0: undefined symbol: mt_fmt_g6
```

The module declares `extern "mt_fmt_g6"` for the host to provide, and **nothing in the tree provides it**. It had
been sitting in the accumulated unit - composed, but never linked into anything that resolves externs, so the
defect was invisible until a differential tried to load the library.

The fix is the provider itself, in the shim: `snprintf("%.6g")`, which is exactly the conversion the native sink
performs. After it:

```
nm -u libllama.so.0 | grep -c mt_fmt   -> 0
route row: cases=3000 fails=0
apex: sha c2ff42cc1219, decode 45.43-46.27
```

## What the sequence says

A module can be composed, typechecked, counted in the symbol total and still be unlinkable, because an `extern`
is a promise nobody checked. The differential is what forced the library to load; the acceptance run afterwards is
what confirmed the shipped path still works and still produces the control hash.

The two remaining metrics families (trace column lists, summary keys) are the same shape and can follow.
