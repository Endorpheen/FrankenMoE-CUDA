# EXP192: runtime identity and module-liveness verification

A single script that answers two questions about the qualified runtime, both of which
have already gone wrong once in this migration.

## 1. Is the running library the one the manifests describe?

After the EXP180 switch fix the manifests still recorded the pre-fix hashes, so a reader
would have paired old binaries with new claims. The script resolves the active
`libggml-cuda.so.0` symlink, hashes the real file and compares it against the stage
manifest's `sha256`, and also flags stale copies left in the run directory by an
interrupted rebuild — one such file was still present and was removed.

## 2. Did every module in the chain actually execute?

A module can stay linked and still stop being reached: an eligibility predicate can flip,
a selector can change, a rejection switch can be mis-set. The witness line is the only
evidence a module ran, so absence is a failure here rather than an assumption.

The script knows eighteen expected witnesses across the active chain, and knows which
witnesses must be absent: EXP180 and EXP186 were rejected and are opt-in, so seeing their
lines means a rejected candidate is running.

## Result on the current runtime

    library  OK  libggml-cuda-mmvq4k-fa8dc98a39ae.so.0.27.0  sha256=232058c901df36a7
    library  OK  no stale copies in run
    witness  OK  all 18 expected modules executed
    witness  OK  no rejected module executed

## Checked against the failure it is meant to catch

A verifier that always passes is worthless, so both branches were exercised:

- feeding a log with no witness lines reports every module as "did not execute" and exits
  1;
- pointing the manifest at a wrong hash reports `library drift: active 232058c9... !=
  manifest 0000...` and exits 1.

The first version crashed on a log path outside the repository instead of reporting the
missing witnesses; that was fixed and is why the check is worth running rather than
assuming.

## Usage

    python3 verify_runtime.py [<apex server.log>]

With no argument it uses the newest `*-goose/server.log` under the active stage. Exit 0
means the runtime matches its manifest and every expected module executed.
