# Windows Alpha transport gate

This test serves the Goose HTTP stack through WinSock with a deterministic
fixture evaluator and tokenizer. It checks UTF-16 file mapping, health,
the retained Ember bundle, path rejection, model discovery, completion
JSON and SSE. No GGUF inference or CUDA is represented by this oracle.
Set `FRANKENMOE_WINDOWS_BUILD` to a complete Windows Alpha build, then run
`bash tests/windows/alpha-transport/run.sh` on Linux with MinGW, Wine and Xvfb.

The Alpha.2 comparison covers plain and structured text messages,
nonempty and empty tools, tool choice, response format, the full Ember
request, native and OpenAI text SSE, route errors, and UTF-8 BOM handling.
For the original Alpha.1 artifacts set `FRANKENMOE_HTTP_LEGACY=1`
to reproduce the earlier HTTP statuses. In Alpha.2 the gate also
checks warnings, timing output and the absence of fixture prompts
and query strings in the HTTP log.
