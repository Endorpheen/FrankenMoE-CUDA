# EXP-2026-11-18-1218: an exported function taking slices segfaults; the same logic works alone

## Reproduction, ten lines

```c
void * h = dlopen(".../libllama.so.0", RTLD_LAZY);
((void (*)(int, char **)) dlsym(h, "goose_init"))(0, 0);
typedef int64_t (*fn_t)(const void*, int64_t, const void*, int64_t, int64_t, void*, int64_t);
fn_t fn = (fn_t) dlsym(h, "fg_sp_prefix");
int64_t draft[4] = {5,6,7,8}, target[4] = {5,6,9,9}, out[2] = {-1,-1};
fn(draft, 4, target, 4, 4, out, 2);     // segfault
```

```
goose_init=0x76eedb5ad140
before
exit=139   (segmentation fault)
```

The generated wrapper is exactly the shape of the call:

```c
int64_t fg_sp_prefix(sl_i64 a0, sl_i64 a1, int64_t a2, sl_i64 a3) {
    if (!gs_program_initialized) gs_panic("goose_init must be called before exported functions");
    return fg_sp_prefix_g(a0.data, a0.len, a1.data, a1.len, a2, a3.data, a3.len);
}
```

and `goose_init` was called first, so the panic path is not it.

## What does work

The **scalar** door from the same library, in the same program shape:

```
fn=0x78b49e9afc10
before
after: r=1        (fg_sp_compatible with identical vocabularies)
```

So exports work in general; it is the **slice** parameters that crash.

## The same logic works when compiled alone

`exp/EXP-2026-11-18-1166-p6-11-spec-compat` gates `g_spec_prefix` (the same function) on seven cases
through its own wrapper and passes. So the algorithm is fine; the difference is the composed unit and
the exported slice ABI.

## Consequences

- no live measurement of the prefix decision is possible until this is fixed, which is why the counter
  never appeared: the call does not return because it crashes, and the server survives because the
  failure is contained;
- the decision itself stays gated synthetically (7 cases) and the rejection decision on 5006.

## What is asked of the compiler agent

Whether an exported function with slice parameters is supported at all in a composed unit, and if so
what the host must do beyond `goose_init`. This is the concrete blocker and it is reproducible with the
ten lines above.
