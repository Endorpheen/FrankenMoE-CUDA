#!/usr/bin/env bash
# Every undefined symbol in the shipped library must be one a linked library provides.
#
# Why: metrics-traces.goose declared `extern "mt_fmt_g6"` and nothing in the tree provided it. The
# module composed, typechecked and counted in the symbol total, and the library it joined could not
# be loaded - `undefined symbol: mt_fmt_g6`. That is invisible to every check that looks at the
# build, and fatal to every run that loads the library.
#
# Allowed: libc and libstdc++ versioned symbols (GLIBC, CXXABI, GCC and GLIBCXX versions), the ggml/gguf families provided by the sibling
# libraries, and the three toolchain bookkeeping symbols below. Anything else is a promise the tree
# has not kept.
set -uo pipefail
LIB=${1:-builds/flint-acc/bin/libllama.so.0}
[ -f "$LIB" ] || { echo "check-unresolved-symbols: no such library: $LIB"; exit 2; }

bad=$(nm -u "$LIB" | awk '{print $2}' \
    | grep -vE '@(GLIBC|CXXABI|GCC|GLIBCXX)_' \
    | grep -vE '^(ggml|gguf)_' \
    | grep -vE '^(__gmon_start__|_ITM_deregisterTMCloneTable|_ITM_registerTMCloneTable)$' \
    | sort -u)

if [ -n "$bad" ]; then
    echo "check-unresolved-symbols: FAIL, $(echo "$bad" | wc -l) symbol(s) no linked library provides:"
    echo "$bad" | sed 's/^/  /' | head -20
    exit 1
fi
echo "check-unresolved-symbols: ok, every undefined symbol is provided (libc, libstdc++, ggml)"
