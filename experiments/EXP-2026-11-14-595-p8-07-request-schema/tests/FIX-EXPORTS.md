# FIX-EXPORTS — public API for P8-07 S1 (`server-schema.goose`)

Defect: the accepted port had 1402 lines and no `export fn`. Every function was
private (`sc_*`), and the gate only worked because `build/gate_schema.goose`
appends the driver to the same module and calls the private bodies. A runtime
cannot call the module: `goose --standalone --header` failed with
"requires at least one export fn".

Fix: 23 `export fn` one-line forwards were added at the end of
`source/server-schema.goose` (now 1436 lines). No decision changed: every
wrapper forwards to the existing `sc_*` body with the same arguments, so the
gate output is byte-identical to the accepted one.

## Added exports

| export | kind |
|---|---|
| `schema_find(name)` | forward to `sc_find` |
| `schema_tfind(name)` | forward to `sc_tfind` |
| `schema_reset(I, F, ANT)` | forward to `sc_reset` |
| `schema_base_std(BI, BF, B, BANT)` | forward to `sc_base_std` |
| `schema_base_set_i(BI, BF, row, v)` | forward to `sc_base_set_i` |
| `schema_base_set_f(BI, BF, row, v)` | forward to `sc_base_set_f` |
| `schema_base_cfg(B, verbosity, np)` | forward to `sc_base_cfg` |
| `schema_base_set_ant(B, BANT, txt)` | forward to `sc_base_set_ant` |
| `schema_defaults(I, F, BI, BF, B, ANT, BANT, SAM, KEYN)` | forward to `sc_defaults` |
| `schema_apply(..., row, key, k, iv, fv, txt)` | forward to `sc_apply` |
| `schema_tline(row, np, out, at)` | forward to `sc_tline` |
| `schema_vline(row, I, F, SAM, ANT, out, at)` | forward to `sc_vline` |
| `schema_tts_defaults(I, F, BI)` | forward to `sc_tts_defaults` |
| `schema_tts_apply(I, F, MSG, row, k, iv, fv)` | forward to `sc_tts_apply` |
| `schema_tts_tline(row, out, at)` | forward to `sc_tts_tline` |
| `schema_tts_vline(row, I, F, out, at)` | forward to `sc_tts_vline` |
| `schema_row_count()` | constant, 63 |
| `schema_tts_row_count()` | constant, 6 |
| `schema_rc_ok()` / `schema_rc_range()` / `schema_rc_type()` / `schema_rc_message()` | constants 0/1/2/3 |
| `schema_msg_len(I)` | `I[79]` |

The minimum requested surface is covered: `schema_apply` (apply/validate one
completion entry), `schema_vline` (read one completion row back),
`schema_tts_apply`, `schema_tts_vline`, `schema_defaults`, `schema_base_std`,
`schema_reset`, `schema_find`. The remaining wrappers expose the base overrides,
the row counts, the status codes and the message length so an integrator does
not need the magic indices.

## Gate after the change

The generated driver now calls the public wrappers (`schema_apply`,
`schema_defaults`, `schema_vline`, ...) instead of the private bodies;
`sc_puts`/`sc_copy` remain only to build input buffers and the error line,
because they are text helpers, not schema decisions.

```
bash tests/build.sh
gate: 181 native lines, 181 Goose lines, diff = 0
```

Mutations re-run after the change, unchanged table:

```
python3 tests/mutate.py 0 6    -> mutations 6/6 caught (range 0:6)
python3 tests/mutate.py 6 12   -> mutations 6/6 caught (range 6:12)
```

## Standalone check

```
goose --standalone -o build/schema_api.c --header build/schema_api.h \
      source/server-schema.goose
rc=0
wrote build/schema_api.c (340861 bytes)
wrote build/schema_api.h (3362 bytes)
```

`build/schema_api.h` declares all 23 exports
(`grep -c "^int64_t schema_\|^void schema_"` = 23), for example:

```
int64_t schema_find(sl_u8 gs_export_arg0_2);
void    schema_reset(sl_i64 gs_export_arg0_4, sl_f32 gs_export_arg1, sl_u8 gs_export_arg2);
void    schema_defaults(sl_i64 ..., sl_u8 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8);
int64_t schema_apply(sl_i64 ..., int64_t gs_export_arg10, sl_u8 gs_export_arg11, ...);
```

Note for integration: a Goose `export fn` must have one concrete signature.
Passing a string literal and a mutable slice as the same argument gives two
specialisations and the compiler rejects the export ("could not resolve one
concrete function signature"). The gate therefore passes every value text as a
slice of one local buffer, and the exported API takes `const u8[:]` values; a
caller must do the same.

## SHA-256

| artifact | SHA-256 |
|---|---|
| `source/server-schema.goose` (new) | `40a367df5a1d661e1e6a577c3b57150a3dc08354721c94b832034e330ef531af` |
| `source/server-schema.goose` (accepted, before) | `d3f91eccf6a7e230cabe4ef428e34a5c407f66f8dbba789b47b334749dc6b98a` |
| `build/schema_api.h` (generated, not committed) | `1a0f2039ef8fd5ac364af5cb6b4aef956a8bac171552024d549dbedd97addfe6` |
| `build/gate_schema.goose` | `86118f6a44678257271503298fa8c0dd139d9c16493442226df354f44ea45018` |
| `build/native_schema.out` | `8618056b5bb80eda995da85ea439a1a69d153630b414209641785f70f2026c4d` |
| `build/goose_schema.out` | `8618056b5bb80eda995da85ea439a1a69d153630b414209641785f70f2026c4d` |
| `build/diff.txt` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty) |
| `tests/gen_schema.py` | `3aebea983f96f5eb5a37d4fe690aad59babf30de245a3c18ffcb1a52587834e0` |
| `tests/mutate.py` | `f1a8dae92ffe9a821b7722a3ff81ce02700f9e1af673faa4da75c8afa974d3df` |

`build/schema_api.c` (340861 bytes) and the header are compiler output; they are
reproducible from the Goose source and are not part of the port.
