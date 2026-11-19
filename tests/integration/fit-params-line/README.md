# fit-params-line

**Requirement:** P10-02 - the maintained tools, and the ones this tree does not have.
**Produced by:** EXP-2026-11-19-1407, moved to the permanent tree on 2026-11-19.
**Tier:** fast - compiles one small C file against the shipped library, no model, no GPU.

## What it proves

`fp_line` produces exactly the line the fit-params tool prints. The tool is not in this build set,
so its rule is re-expressed from its own source and both sides are fed the same numbers:

```
printf("-c %" PRIu32 " -ngl %" PRIi32, cparams.n_ctx, mparams.n_gpu_layers);
when more than one device: " -ts " then the tensor split as uint32 values separated by commas
```

Four contexts (512, 4096, 32768, 262144) against four GPU-layer counts (0, 12, 32, 42): 16 cases.

The comparison carries its own control: the expectation shifted by one must disagree, and the test
fails if it does not. Without that, an agreement means nothing - the same discipline the sampler
doors use.

## What it does not prove

That the runtime calls the port. No host in this tree calls `fp_line`: the printer lives in the
absent tool, and `common/arg.cpp` only carries the flag and its limits. The gate proves the
decision is right, not that it is reached; `docs/goose-migration/P10-02-TOOLS-INVENTORY.md` records
that distinction.

## Run

```
bash tests/integration/fit-params-line/run.sh            # builds/flint-acc/bin
bash tests/integration/fit-params-line/run.sh <lib-dir>  # a release set
```

Expected: `fp_line: calls=20 disagreements=0`, exit 0. Exit 77 without a library.
