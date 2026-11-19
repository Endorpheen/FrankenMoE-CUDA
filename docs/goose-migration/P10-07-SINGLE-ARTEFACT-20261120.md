# The JSON path now works in one artefact, but that artefact is not the baseline

Built 2026-11-20 as `/home/end0/AI/LlamaServer/frankenmoe-cand-json3`, after the two runtime defects were
found by probing: the module declares its runtime and defines none of it, and the door refuses to answer
before `goose_init`.

## What it proves

```
bash tools/verify-json-build.sh .../frankenmoe-cand-json3/run
door json_parse in libllama        present
adapter in libllama-common         present
set self-contained                 ok
swap test, three modes             ok
json swap: goose=1 texts=7 failures=0
```

One artefact, both halves, seven texts through the Goose parser in strict mode, no fallback. P10-07's
technical barrier is gone: the path works end to end in a single build.

## What it does not prove, and why it is not the baseline

```
bash tools/verify-candidate.sh .../frankenmoe-cand-json3/run
accepted components present        FAIL
exported by this artefact:         88 (7%)
```

This is path A, and path A carries 88 unit doors where the baseline carries 200 - the same gap measured on
every path-A build today. It gains the JSON path and still misses accepted components, so it is not a
candidate for measurement, let alone for the baseline.

## The remaining question, stated exactly

The baseline (`builds/flint-acc`, 200 doors, 20/20 components) already contains `json_parse` and
`json_dump` - it has the Goose parser. What it lacks is the **adapter** in `libllama-common`, because that
library was built before `json-adapter.cpp` existed.

So the shortest path to one artefact with everything is not another full build: it is

1. determine what produced `builds/flint-acc` (still the open provenance question), or
2. rebuild **only** `libllama-common` from the current sources - the adapter is in its CMake target - and
   pair it with the existing baseline `libllama`.

Option 2 is a two-minute build and would give a set with 200 doors, 20/20 components and both JSON halves.
It is recorded here, not done, because it changes which libraries a set contains and that decision belongs
with the baseline question.

## Correction on the way

The first attempt at this build (`frankenmoe-cand-json2`) linked the module without `--standalone` and
failed at run time with `goose_init must be called before exported functions`; an earlier attempt failed at
`dlopen` on `gs_panic`. Both are recorded in the bridge's comments so the next person does not repeat them.
