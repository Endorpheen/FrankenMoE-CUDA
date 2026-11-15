# EXP-2026-11-15-681 — the Goose builder's nodes execute

Base: `b8d7da00`. Build and gate only. No model, no server, no GPU.

## 1. What this establishes

The graph primitives were bound in EXP680 but nothing drove them, so the runtime
could describe a graph and not compute one. This closes that: a node created by
`goose/cpu/goose-graph-builders.goose` is expanded into a ggml graph, executed on
the CPU backend, and its output checked against arithmetic the host computed
independently.

```
FG_GRAPH_EXEC ADD backend=cpu status=0 result=[11 22 33 44] PASS
FG_GRAPH_EXEC MUL backend=cpu status=0 result=[10 40 90 160] PASS
```

**Update (backend seam).** The compute now goes through `ggml_backend`, not
through a bare `ggml_graph_compute` on a CPU context: `ggml_backend_cpu_init`,
`ggml_backend_alloc_ctx_tensors`, `ggml_backend_graph_compute`,
`ggml_backend_tensor_set/get`. Only public API is used, so no struct field is
touched and the gate cannot drift from ggml's internals.

That abstraction is the seam a GPU backend plugs into. Proving it on CPU is the
prerequisite for wiring CUDA without changing the Goose side: the module still
chooses the node, and the backend only decides where the arithmetic runs.

Inputs are `a = [1 2 3 4]`, `b = [10 20 30 40]`; the expected values are the
host's own sums and products, not a copy of what the graph produced.

## 2. Who decides what

| decision | owner |
|---|---|
| which mode, which op, which operands, result shape | `gb_common` in the Goose module |
| how `ggml_new_tensor`/`set_op`/`set_src` are spelled | `graph-bindings.cpp` |
| graph expansion and execution | ggml |

The gate packs an eight-word argument array the module reads through
`fg158_arg`, then asks ggml to compute. It contains no node policy: changing the
op in the array changes the node, and the module chooses everything else.

## 3. Two ops, deliberately

One agreeing case could be a node that happened to be initialised to the right
values, because `ggml_dup_tensor` gives an uninitialised tensor of the same shape.
ADD and MUL together make that implausible: they produce different results from
the same inputs, and both match the host's independent arithmetic.

## 4. Placement: a test, not an adapter

This check first lived in `host_v4.c`, which pushed adapter debt from 1294 to 1334
lines. That is the wrong direction for a rule that says adapters must shrink, so
it moved to `integration/goose-standalone/tests/graph_exec.c`. A gate holds no
runtime logic and nothing links it into the shipped host, so
`tools/migration-status.sh` now excludes `tests/` from the debt count. Debt is
back to 5 files / 1294 lines.

## 5. Where the engine stands

| layer | state |
|---|---|
| GGUF container, metadata, tensor directory | yes |
| tensor payload in memory | yes (65–75 tensors, mismatches=0) |
| graph node construction | yes (16 primitives, 15 predicates) |
| graph execution on CPU | **yes** (two ops, verified) |
| allocator | arithmetic + verified placement |
| backend seam (CPU/GPU dispatch) | no |
| KV cache buffers | no |
| tokenizer mechanics | no |
| HTTP server | no |
| model load + real graph from weights | no |

The next layer is the backend seam: today the gate creates a ggml context
directly, which is the shape a real runtime must not hard-code.

## 6. Reproduce

```
gcc-13 -O1 -std=c11 -I upstream/llama.cpp/ggml/include \
    -c integration/goose-standalone/tests/graph_exec.c -o /tmp/graph_exec.o
g++-13 -O1 -fopenmp build/goose-main/main.o builds/goose-standalone/host.o \
    builds/goose-standalone/graph-bindings.o /tmp/graph_exec.o \
    -L<off ggml>/ -lggml-base -lggml -lggml-cpu -lm -pthread -lstdc++ \
    -o /tmp/graph_exec && /tmp/graph_exec
```
