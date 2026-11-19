# EXP-2026-11-19-1250: bmoe_core builds without llama.cpp and not with ours

## What works, and it is enough for differentials

```
cmake -S runtime -B builds/bmoe -DCMAKE_BUILD_TYPE=Release     # llama.cpp present = OFF
cmake --build builds/bmoe -j 8                                 # Built target bmoe_core
```

That pure-policy build exports `find_moe_recipe`, `moe_recipe_at`, `n_moe_recipes`, `ngram_draft` and
`validate`, and it has already produced two real-reference differentials (EXP-1248, EXP-1249).

## What does not

Pointing `runtime/third_party/llama.cpp` at `vendor/flint` configures (`llama.cpp present = ON`) and then fails
in the engine:

```
chat_parse.cpp:11  'class common_peg_arena' has no member named 'load'
chat_parse.cpp:12  'struct common_chat_parser_params' has no member named 'reasoning_format'
```

The engine is written against the **bmoe fork** of llama.cpp (it also mentions an expert-ready hook the public
API lacks), and `vendor/flint` is a different revision. So the full engine - session, streaming, chat glue - is
not buildable against this tree, and the functions behind it stay unavailable to a differential.

## A real fix that came out of the attempt

`vendor/flint/src/CMakeLists.txt` hardcoded `${CMAKE_CURRENT_SOURCE_DIR}/../../../tools` and `.../goose` for the
Goose tooling and module dependencies. Reached through a symlink those became `runtime/tools/...` and the build
stopped with "No rule to make target .../runtime/tools/build-sampler-chain.sh". It now resolves `FG_REPO_ROOT`
from a list of candidates (overridable, and it prints which one it chose), which is correct for both layouts.

Verified after the change: the normal build is unaffected (`target llama` rc=0).

## Consequence for the roadmap

P9-03/P9-04 (dense weights, streaming, LRU, overlap) remain verifiable only through transcriptions or a live
server, because their native code sits in the engine that cannot be built here. P9-01 and the draft side of
P9-05 are the parts the pure-policy build can actually compare, and those are now done.
