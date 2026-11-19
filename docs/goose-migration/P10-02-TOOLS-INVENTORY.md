# P10-02: which command-line tools this tree actually has

Recorded 2026-11-19. The requirement asks every maintained tool to have a working Goose
implementation and every absent tool to be accounted for explicitly, instead of being called
"wired".

## What the tree builds

`vendor/flint/tools/CMakeLists.txt` adds exactly three subdirectories:

```
add_subdirectory(mtmd)
add_subdirectory(ui)
add_subdirectory(server)
```

There is no `llama-bench` host and no `tokenize` host in this tree: `find vendor/flint -name "*bench*.cpp"`
is empty and no `tokenize*.cpp` exists. This is a property of the fork, not of the Goose port.

## Decision per tool

| tool | host in this tree | Goose decision | decision |
|---|---|---|---|
| `llama-bench` report | **absent** | `goose/misc/llama-bench-report.goose` | composed, **no caller**: excluded from the supported set until a bench host exists. Not claimed as wired. |
| `tokenize` utility | **absent** | `goose/cli/tokenize-cli.goose` | the decisions are used by the standalone entry point `frankenmoe-tokenize` (see `tools/build-standalone-runtime.sh`); the upstream utility binary itself is not built here, so the *utility* is excluded, the *decisions* are delivered. |
| fit-params line | not a tool: the `llama-fit-params` flag lives in `common/arg.cpp` | `goose/cli/fit-params-line.goose` | wired where the flag is parsed and printed; the live call site exists, so this is the one item of the three that is genuinely reachable. |

## What "excluded" means here

An excluded tool is not a failing requirement. It means: this tree does not build that binary, so
there is no call site to switch, and the Goose module that carries its formatting stays composed
and gated but is not counted as serving a path. Adding the host later is a separate, verifiable
step: the host comes back, the door is switched, the gate compares output byte for byte.

## Consequence for the requirement

P10-02 stays PARTIAL with one open item: a permanent gate for the fit-params line at its real call
site (parse and print), which needs a host run and is therefore not measured here. The two absent
utilities are accounted for above rather than silently counted as done.
