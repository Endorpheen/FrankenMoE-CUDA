# EXP-2026-11-19-1406: the fit-params line is composed into the unit and answers a known case

## What P10-02 asked for and what this does

The item lists three utilities; the first is the fit-params line. The Goose implementation existed as a module
(`goose/cli/fit-params-line.goose`) with two exported doors, and **neither was in the shipped library** - the build set's
module list did not include it, so it was a file, not a capability.

Adding one line to `tools/build-sampler-chain.sh` composes it, and both doors appear in the artefact:

```
nm -D libllama.so.0 | grep -c 'T fp_line|T fp_ts_count'   -> 2
```

## The probe, because linking is not answering

The loop's own rule for a new export is a probe on a value known by hand. `fp_line(4096, 41, one device, no override)`
must print `-c 4096 -ngl 41` and a newline:

```
n=16 line=[-c 4096 -ngl 41
]
```

Sixteen characters including the newline, which is what the door's contract says it writes.

## What this does and does not establish

Establishes: the module compiles inside the cumulative unit, its doors are reachable in the artefact, and one case
produces the expected text.

Does not establish: equality with the native formatter. That needs the native line for the same numbers, which the
`llama-fit-params` tool would print - and that target is not part of this build set (`cmake --target help` has no fit
tool). The next step is to obtain the native line another way, most plausibly by running the server with `-fit on` and
reading the line it logs, then feeding those numbers to `fp_line`.

## Shipped path

The module is additive: no call site uses it yet. Acceptance afterwards: decode 62.64 / 63.18 / 63.25, prefill
2696.4 / 2946.3 / 2947.5, hash `c2ff42cc1219`, accepted components ok.
