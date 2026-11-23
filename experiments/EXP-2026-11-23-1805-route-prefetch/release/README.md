# v0.12.0-chat-alpha build record

- Linux `frankenmoe-server` c852fa04215552e0344b59ebbd7392380102382cc766a71c676dc784a7aa2989:
  composed EXP-1786 Goose unit + `goose/server/inference-http.goose` (via
  `tools/compose-alpha-runtime.py`, now aware of the EXP-1724 sampler scope wrapper),
  KeelWing ownership v2 (139373305b88...), chat CLI with the accepted runtime defaults,
  linked against the EXP-1805 `libfranken-inference.a` (59fc73ae...).
- Windows `frankenmoe-server.exe` 0b25cdfe22d4c2c52472927a9473898c7410c89ea774fbe4345bf3e49a28fdf6:
  MinGW 13 posix; members unchanged since EXP-1721 reuse the windows-alpha-2 objects,
  `cuda-driver`, `graph-execute`, `model-host` are three-way merges (EXP-1805 line +
  Windows guards from `codex/windows-alpha`, no conflicts), plus `sampler-scan` and the
  EXP-1744 planner. Recipe: `build-native.sh`.
- Checks: Linux GPU chat (`chat-new`, `chat-v0112`), Windows transport gate under Wine
  (`wine-transport.log`, PASS), Windows CPU-only real-model start under Wine
  (`wine-model-server.log`: model loaded, prefill finished, stopped before decode).
