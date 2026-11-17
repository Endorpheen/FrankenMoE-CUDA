# EXP-2026-11-17-939: metadata values formatted in Goose

Status: ACCEPTED.

## Hypothesis

The load log formats every key/value pair, and that formatting was C++
(`gguf_kv_to_str` + `gguf_data_to_str`). It is a small but real set of decisions:
which wire type maps to which text form, how a bool prints, how a float prints
(six decimals, like `std::to_string`), how an array is joined and how strings are
quoted. If Goose owns them, the last metadata accessors on the load path stop
being native.

## What Goose owns now

`source/gguf-format.goose`:

- `gr_kv_to_str` — the whole value for one entry, with the caller's `max_len`
  cap and the same early stop the native version has;
- `gf_scalar` — the ten scalar types;
- `gf_f32_from_bits` — IEEE-754 binary32 decoded **in Goose arithmetic**
  (sign, biased exponent, mantissa, `2^k` by repeated multiply/divide), so the
  formatter needs no float binding at all;
- `gf_put_f64` — `printf("%f")` emulation: scale by 1e6, round, print, six
  decimals;
- `gf_put_quoted`, `gf_str_at`, `gf_elem_size` — string quoting, element spans,
  element stride.

## Gate

The gate formats all 52 entries of the Apex file and compares them with the
accepted library's own load log, after the same 40-character truncation the
loader applies.

    goose entries 52, mismatches 0

Wired into the shipping candidate, the whole load log is identical:

    accepted entries 52, candidate entries 52, mismatches 0

and the metadata accessor counts drop:

| call | accepted | candidate before | candidate now |
| --- | ---: | ---: | ---: |
| `gguf_get_kv_type` | 252 | 99 | **47** |
| `gguf_get_arr_type` | 17 | 5 | **0** |
| `gguf_get_arr_n` | 17 | 5 | **0** |

The remaining 47 are the loader's own type checks, not the log.

Behaviour unchanged: pieces, tokenize and sampler gates stay byte-identical.

## Reproduce

    python3 tools/compose-goose.py unit.goose experiments/EXP-2026-11-17-937-goose-gguf-arrays/source/reader.goose goose/model/gguf-format.goose
    goose --standalone -o unit.c --header unit.h unit.goose
    gcc-13 -O1 -I. -o gate tests/gate.c unit.c <forwards> <graph-bindings> <libggml-base>

## Found while gating

Two defects, both caught only because the comparison was byte-exact against the
native log rather than against my own expectation:

- the string branch refused a value longer than the output buffer, so a long chat
  template printed nothing at all. It now writes a prefix, which is what the
  `max_len` cap means;
- my first gate truncated at `max_len` instead of `max_len - 3` followed by
  `...`, and the resulting 8 "mismatches" were the gate's fault, not the
  formatter's. Worth stating: a gate that reports a mismatch must first prove it
  is measuring the right thing.
