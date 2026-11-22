# Windows Alpha transport gate

This test serves the Goose HTTP stack through WinSock with a deterministic
fixture evaluator and tokenizer. It checks UTF-16 file mapping, health,
the retained Ember bundle, path rejection, model discovery, completion
JSON and SSE. No GGUF inference or CUDA is represented by this oracle.
Set `FRANKENMOE_WINDOWS_BUILD` to a complete Windows Alpha build, then run
`bash tests/windows/alpha-transport/run.sh` on Linux with MinGW, Wine and Xvfb.
