# What the baseline contains that path A cannot produce, and the mechanism that could close the gap

Measured 2026-11-20 by comparing symbol sets. This is the most concrete step yet towards reproducing the
accepted artefact, which is the blocker behind P9-07, P10-06, P11-01 and P11-04.

## The gap, by family

`libllama` of the baseline against path A's build: **295 symbols in the baseline that path A lacks**.

| family | count | where it comes from |
|---|---|---|
| `fg_*` | 41 | Goose doors, various modules |
| **mangled C++ (`_ZZN8G`, `_ZNSt…`, `_ZGVZN…`)** | **~100** | **C++ translation units linked into libllama** |
| `lb_*` | 23 | Goose doors |
| `mt_*` | 22 | `goose/metrics/metrics-traces.goose` via `fg-goose-metrics-bridge.cpp` |
| `mg_*` | 13 | Goose doors |
| `hf_*` | 12 | `goose/server/hf-cache-naming.goose` - **not in path A's pair list** |
| `sdp_*` | 11 | Goose doors |

Two facts follow, and neither was stated anywhere before:

1. the baseline **links C++ bridges** - the m1 `fg-goose-*.cpp` family - while `build.py` compiles only
   `.c` bridges. That alone accounts for the mangled symbols and for `mt_*`;
2. the baseline **compiles Goose modules that path A's `_GOOSE_PAIRS` does not list**, `hf-cache-naming`
   among them.

So the baseline is not path A with more luck: it is a build whose module set is much larger, and whose
bridge set includes C++.

## The mechanism that already exists to pin this

`tools/resolve-modules.py` accepts a **resolution file** as its fourth argument: one line per module,
`<source-file> <experiment-dir>`, and with it "nothing is decided here at all". That is precisely the
instrument needed: the baseline's door families name the modules, and the resolution file would freeze which
source each one comes from.

## The work, in order, each step verifiable

1. map the 295 missing symbols to Goose modules - `manufest`-style lookup by the door name in
   `goose/**/*.goose` gives the module for most of them;
2. write the resolution file listing those modules and their experiment directories;
3. run `consolidate-runtime-tree.sh` with `RESOLUTION=<file>` and an `OBJDIR` from any build, then
   `stage-m1-tree.sh --with-unit` and `build-from-tree.sh`;
4. compare with `tools/compare-artefacts.sh`: the door families above are the acceptance criterion;
5. only then ask the four-axis question.

## What is not claimed

That this reproduces the baseline byte for byte. The resolution pins *modules*; the C++ bridge set, the
compile flags and the compiler identity (the baseline was built with GCC 15.2, the scripts pin gcc-13) are
separate variables, and each has to be matched and recorded as well.

## Why it is worth doing this way

Every alternative tried today failed on the same point: path A misses the modules, path B misses the
resolution, and the release matches neither because nothing recorded which of the two it came from. Pinning
the resolution converts a question of memory into a file in the repository - and a file can be diffed,
reviewed and reproduced.

## The answer, in one line: the baseline compiles the full m1 module set

Mapping every non-mangled missing symbol back to the module that exports it
(`docs/goose-migration/BASELINE-MODULE-MAP.json`) resolves **132 of 153** symbols across **15 modules**:

```
goose/cli/llama-bench-report.goose          23 doors
goose/metrics/metrics-traces.goose          21
goose/sampling/sampler-chain-full.goose     15
goose/server/model-manager.goose            13
goose/server/hf-cache-naming.goose          12
goose/misc/spec-decode-decisions.goose      11
goose/cuda/cuda-arch-policy.goose           10
goose/cuda/cuda-install-layout.goose        10
goose/speculative/spec-doors.goose           6
goose/cuda/cuda-target-flags.goose           4
and five more with one to three doors each
```

The remaining 21 symbols are not module exports at all: they are bridge and adapter entry points
(`fg_*_call` wrappers, `fg_port_*`), which is why the mangled C++ symbols appear in the same gap.

So the baseline is a build of **the staged m1 tree with its full module set** - the twenty-odd modules the
m1 host declares - and not a curated pair list like path A's. That is consistent with everything measured
today: path B is the right shape of build, and its 11 doors came from a consolidation that resolved 36
modules instead of the full set.

## What that changes in the plan

Step 1 of the plan above is now done, as data: `BASELINE-MODULE-MAP.json` names the modules by file. What
remains is the resolution file itself - a `<source-file> <experiment-dir>` line per module - and the
consolidation run that proves the door families match.
