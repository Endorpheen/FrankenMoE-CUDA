# tests/ — the permanent test system

Everything here is permanent. A check that must keep passing belongs in this tree, not in an
experiment. Experiments stay the place for hypotheses, one-off diagnostics, measurements and
reports; when an experiment's result is accepted, its regression check moves here and the
experiment card points at the new path.

## Layout

| directory | what belongs there |
|-----------|--------------------|
| `cpu/` | CPU kernels, quantized dot products, thread and SIMD paths |
| `cuda/` | CUDA kernels and launch policy; needs a GPU to run |
| `ggml/` | tensor/graph surfaces, ggml adapter behaviour |
| `kv/` | KV cache, state arena, context reuse |
| `tokenizer/` | vocabulary load, pre-tokenisation, encode and decode |
| `sampler/` | sampler chain, RNG, distribution pick |
| `grammar/` | grammar automaton, constrained generation |
| `server/` | HTTP routes, MCP tools, server lifecycle |
| `models/` | loader, metadata, per-architecture graphs |
| `memory/` | expert cache, placement, LRU, budget |
| `integration/` | two or more subsystems together |
| `performance/` | measurements with a controlled load; never part of `fast` |
| `regression/` | a defect that was fixed once and must not come back |
| `fixtures/` | inputs shared by more than one test (tables, small models, corpora) |
| `oracles/` | independent reference implementations used to judge the port |

Empty directories carry a `.gitkeep`. A directory gets a test when the test exists, not in
advance.

## A test is a directory with `run.sh`

```
tests/<area>/<test-name>/
├── run.sh        # executable; derives every path from its own location; exit 0 = pass
├── README.md     # what it proves, the oracle, the EXP that produced it, the requirement
└── ...           # fixture sources, tables, expected output
```

`run.sh` rules, the same ones the experiment template already follows:

1. No absolute paths. The repository root is derived from `$BASH_SOURCE`.
2. Build outputs go to a gitignored directory (`build/`), never into the tree.
3. Never commit generated C, objects, or binary gates.
4. Every run writes its full log from the first line and prints the same bytes; the log is the
   evidence.
5. A test that needs a model, a GPU or the Apex lock must say so in its first lines and must
   refuse to run without its precondition.
6. A test that judges a port against native C++ keeps the native side in `tests/oracles/` or
   compiles it into the fixture; it never links the oracle into the production runtime.

## Running

```
bash tests/run.sh fast                 # every area that needs no model and no GPU
bash tests/run.sh grammar              # one area
bash tests/run.sh grammar reject-alternates
bash tests/run.sh integration
bash tests/run.sh full                 # delegates to tools/check.sh full, then the tree
bash tests/run.sh performance          # explicit, never implied by full
bash tests/run.sh list
```

`run.sh` does not reimplement the build and acceptance machinery: it delegates to
`tools/check.sh` for `integration` and `full`, and to `tools/apex-test.sh` for anything that
needs the Apex lock. `tools/check.sh` stays the authority on what `fast`, `integration` and
`full` mean for the migration build.

## Registry

`tests/REGISTRY.md` is the single list of permanent tests: area, name, path, the EXP that
produced it, the requirement it covers, its tier and its status. A test that is not in the
registry is not part of the system; add the row in the same commit as the test.
