# EXP403: loader mmap-alignment decision (integrated, unreachable on this path)

The decision is in Goose and wired, but it lives in `load_arch`'s `file != nullptr` branch — the branch
used when a GGUF is handed over as a `FILE*`. This profile loads by path, so the route never fires
(`FG403S` = 0).

Recorded as integrated-but-unreachable, the same category as EXP377. It is not counted.
