# Standalone runtime architecture

Clients send HTTP requests to the standalone host. The
[Goose HTTP unit](../goose/server/inference-http.goose) handles request
parsing, chat formatting, text generation and response serialization. The
[OpenCode checklist](OPENCODE-SMOKE-TEST.md) covers the tool-call round trip:
the client executes approved tools and returns their results to the server.

The standalone host connects model loading, tokenization, KV and recurrent
state, graph execution and sampling. The [Goose source map](../GOOSE.md)
locates the modules. The [integration map](../integration/README.md) locates
the C adapters, the CLI, the packaged launcher and the CUDA driver.

For the cumulative Apex Linux build, the driver manages the GPU expert cache,
queues device copies and replays CUDA graphs. The
[checkpoint recipe](../integration/goose-standalone/apex-checkpoint/README.md)
identifies which source and prebuilt inputs produce that driver. Architectural
support and performance evidence are recorded by model in the
[model registry](goose-migration/MODEL-ARCHITECTURES.md).
