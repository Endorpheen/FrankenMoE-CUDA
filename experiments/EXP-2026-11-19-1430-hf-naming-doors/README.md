# EXP-2026-11-19-1430: the Hugging Face cache naming decisions composed and probed

## What was there

`goose/server/hf-cache-naming.goose` holds the naming rules for the HF cache - repository validity, commit and token
validation, the folder form and its inverse, and the file-finalisation branch. The host file exists
(`vendor/flint/common/hf-cache.cpp`). The module was in no build set, so it was a file, not a capability.

No externs were declared; that was checked before composing, after EXP-1418 showed what a missing provider does.

## The probe, first time right

```
repo_valid("org/name") = 1
repo_valid("org")      = 0
repo_valid("")         = 0
repo_valid("a/b/c")    = 0
commit_valid(40 hex)   = 1
commit_valid(39 hex)   = 0
repo_folder("org/name") = 17 -> [models--org--name]
```

The folder is the form the cache actually uses on disk (`models--org--name`), and the validity rules answer as the rules
say: one separator, no empties, a commit is exactly forty hex characters.

This probe passed on the first run - unlike the last three, where the expectation was the wrong part. The difference is
that its expectations came from the **domain** (how the HF cache names things) rather than from my reading of the door.

## Acceptance, and why only the hash is quoted

```
decode 51.38 / 52.25 / 59.47   prefill 2243.1 / 2379.0 / 2885.8   hash c2ff42cc1219
```

The hash is unchanged, which is what this card can claim: the module is additive and nothing calls it. The timings are
**not** quoted as a result - the GPU is shared with a foreign service (EXP-1427), so these numbers describe the sharing, and
reporting them as a build characteristic would be the mistake that card was written to prevent.

## State for the HF item of P7-12

| item | state |
|---|---|
| naming doors composed into the unit | yes, gated |
| the host that would call them | `vendor/flint/common/hf-cache.cpp` exists |
| wiring | not done: no call site yet |
