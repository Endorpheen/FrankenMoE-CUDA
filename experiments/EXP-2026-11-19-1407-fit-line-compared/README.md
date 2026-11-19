# EXP-2026-11-19-1407: the fit-params line compared with the native format - 16 cases, all equal

## Where the native reference came from

The `llama-fit-params` target is not in this build set and its source is not under `vendor/flint`, so the previous card
could not compare against it. The tool's source does exist in a staging tree
(`builds/rt-goose/src/tools/fit-params/fit-params.cpp`), and it prints the line with:

```c
printf("-c %" PRIu32 " -ngl %" PRIi32, cparams.n_ctx, mparams.n_gpu_layers);
```

That is the whole line for the simple case, so the comparison writes the same format out and feeds both sides the same
numbers - the discipline used for the sampler doors, where the native rule is re-expressed from its source rather than
called.

## The comparison

Four contexts from 512 to 262 144 against four GPU-layer counts from 0 to 42:

```
fp_line: calls=16 disagreements=0
```

Sixteen of sixteen identical, byte for byte including the newline.

## What is not compared

- the `-ts` branch, which the native tool emits only when more than one device is present - this machine has one;
- the `-ot`/`--override-tensor` and `--override-kv` parts, which need override patterns;
- the backend list.

Those are stated as gaps; the card does not claim the whole line is equal.

## State for P10-02

| sub-item | state |
|---|---|
| fit-params line composed into the shipped unit | yes, both doors exported |
| probed on a value known by hand | yes, `-c 4096 -ngl 41` |
| compared with the native format | **16/16 equal**, simple case |
| used by a call site | no - no call site exists in this tree yet |
| `-ts`, overrides, backends | not compared |
