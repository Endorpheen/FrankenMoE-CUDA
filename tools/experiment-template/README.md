# Experiment template

Copy this directory to `experiments/EXP-YYYY-MM-DD-NNN-<slug>/` and fill in:

- `source/runtime.goose` — the slice, copied from the base commit named in the task;
- `tests/fixture.cpp` — the differential gate (native oracle vs port);
- `tests/mutate.py` — the mutation table; use `--jobs N` and two halves;
- `README.md` — hypothesis, oracle, gate numbers, recorded approximations.

Rules:

1. `tests/build.sh` takes no absolute paths: it derives the experiment root from
   its own location. `FG_LLAMA`, `FG_GOOSE`, `FG_OUT`, `FG_LIBS` override defaults.
2. The gate runs at `-O1`; mutations at `-O0`. The gen.o cache is keyed by the
   source SHA, the optimisation flag and the compiler version.
3. Never commit `source/*-gen.c`, `source/*-gen.h`, `tests/*.o` or the gate binary.
4. Record the base commit SHA in the card. Rebase on the current canon before
   integration and re-run the gate.
