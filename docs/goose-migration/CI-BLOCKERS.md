# CI blockers found before writing the workflow

The goal is a pull-request gate that needs no weights, no GPU and no local
paths. Inspecting the tree produced two real blockers:

1. The tracked C++ copy (`vendor/flint`) has no `app/` directory, while its
   `CMakeLists.txt` calls `add_subdirectory(app)` unconditionally. A CI
   configure therefore fails before compiling anything. The full tree that does
   build (`llama.cpp-flint-cuda`) is not tracked. Options, none applied yet:
   add the missing target to the repository, or let the workflow create a
   throw-away `app/CMakeLists.txt` stub in the runner only. Changing the
   runtime CMake to make CI pass is not allowed.
2. The pinned Goose compiler is not in the repository. Its release archive and
   sha256 are recorded (`COMPILER-2026-3-STRING.md`:
   `5c6fdf1694...`), the download URL is not. `tools/check.sh fast` needs the
   compiler for compose/typecheck/build, so that step must stay conditional
   until a URL or a release asset exists (`GOOSE_URL` + `GOOSE_SHA256`
   repository secrets are the natural place).

What the workflow can already gate without either blocker:

- `python3 tools/check-goose-tree.py` (tree hashes, experiments, coverage);
- `python3 tools/check-generated-files.py` (no generated C above the limit);
- `python3 -m py_compile` on the tracked tools and tests;
- a static check that every architecture in `llama-arch.h` has a name row and a
  factory branch (cheap, catches the exact class of mistake this work made).

Branch protection: enabled on `main` via the API. Required checks are the two
blocking jobs (`tree, generated files, tools, architecture registry` and
`C++ type check of the changed model sources`); force pushes and deletions are
disabled. The Goose job stays optional until GOOSE_URL/GOOSE_SHA256 exist.
