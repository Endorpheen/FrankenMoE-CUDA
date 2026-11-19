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
