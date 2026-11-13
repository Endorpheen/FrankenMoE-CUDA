# Where the Goose sources live

One place: [`goose/`](goose/README.md). Start there.

The tree is grouped by subsystem, not by experiment:

```
goose/jinja/      Jinja stack (lexer, parser, value, built-ins, runtime)
goose/json/       JSON parsing, rendering and JSON-schema
goose/grammar/    GBNF grammar parsing
goose/server/     HTTP server decisions
goose/cpu/        CPU backends, quantized kernels, MoE, allocator
goose/cuda/       CUDA operators
goose/kv/         KV cache, memory, batch, context
goose/model/      model loading, GGUF, tokenizer
goose/sampling/   sampler decisions
goose/chat/       chat templates and tool calls
goose/misc/       slices with no named native file
goose/_archive/   preserved sources from other checkouts (not current code)
```

4061 `.goose` files. A complete module is one file named after the native
module (`jinja/lexer.goose`); a partial slice is named after the decision it
implements (`server/route-table.goose`). `goose/MANIFEST.json` maps every file
to its experiment, native path, gate result and SHA256.

The experiments keep the proof (fixture, build, mutations); they are no longer
the place to read the code. Current project state:
[`docs/goose-migration/STATUS.md`](docs/goose-migration/STATUS.md).
