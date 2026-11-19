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

## The `-ts` branch, and the expectation error it caught

The branch can be exercised without a second GPU: the values are `uint32_t` casts of the split floats, and the tool
trims trailing zero splits before printing. Three split sets were added, and the first run reported one difference:

```
DIFF ts k=2 native=[-c 4096 -ngl 41 -ts 1,0] goose=[-c 4096 -ngl 41]
```

The difference was in the **harness**: the native expectation did not implement the trim (`while (nd > 1 && split[nd-1] ==
0) nd--;`), while `fp_line` already did. With the trim written into the expectation:

```
fp_line: calls=19 disagreements=0
```

Nineteen of nineteen, including the uint32 quirk - a split of 0.6 prints as `0`, because the tool casts the float rather
than printing its decimals. The port reproduces that too, which is the point: matching the tool means matching its quirks.

## What is still not compared


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
