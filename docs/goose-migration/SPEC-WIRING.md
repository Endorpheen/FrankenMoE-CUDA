# Wiring the speculative decisions into the host: what works and what does not

State on 2026-11-18. The doors exist and are exported; the host call does not reach them yet.

## What is ready

`goose/speculative/spec-doors.goose` exports four doors by name and the chain build composes them
into the same unit:

```
int64_t fg_sp_compatible(...)
int64_t fg_sp_prefix(...)
int64_t fg_sp_ngram_accept(...)
int64_t fg_sp_n_max_call(...)
```

`nm -D --defined-only libllama.so` lists all four, and the host has a cross-check in
`common_speculative_n_max` that builds the arrays, calls the door and compares with the native value
(applying the native one either way, so a wrong port can only be counted).

## What does not work: the symbol does not resolve

Four attempts, each failing in a different way:

| attempt | result |
|---|---|
| `__attribute__((weak))` reference | links, and stays **null** at run time: the caller is in a static library, the definition in a shared one, and the linker may resolve a weak undefined symbol to null |
| strong reference | link error: `libllama-common.so: undefined reference to fg_sp_n_max_call`, because `llama-common` must not depend on `libllama` - it is the other way round |
| `dlsym(RTLD_DEFAULT, ...)` | returns null: the dependency is loaded locally, so it is not in the global scope |
| `dlopen("libllama.so.0", RTLD_NOLOAD)` then `dlsym` | no output from the hook at all in the live run, so this path did not reach the door either |

The server builds and serves requests with the hook in place - it is inert, not broken - but the
counter never prints, which means the door is never called.

## The options, named

1. **Move the door into `libllama-common`**: the speculative decisions have no dependency on
   `libllama`, so a second small Goose unit compiled into `llama-common` removes the boundary
   entirely. This is the cleanest and it is what the other host-side ports do.
2. **Make `llama-common` link `libllama`**: creates a cycle in the current layout and should be
   rejected.
3. **Resolve by the library's own handle**: keep `dlopen` but pass the real soname with the version
   suffix, and check the return of `dlopen` before drawing conclusions. The last attempt did not
   print, so it needs one more diagnostic before it can be called failed.

Option 1 is the next step. Nothing here weakens what is proven: the decisions are gated in their own
units; this is only about which library carries them.
