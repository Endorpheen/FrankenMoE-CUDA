#!/bin/bash
# Build the gate binaries from the objects the server links.
#
#   gate-on       : bridge + switches + unit + the shipped asset table, from build-on
#   gate-off      : bridge + switches from build-off (the unit-isolation build,
#                   where every wrapper is compiled to -1)
#   gate-default  : the same as gate-on, from build-default when it exists
#
# Nothing here compiles server logic: the objects are taken from the CMake build
# directories, which is what makes the gate a measurement of the shipped code.
# libllama-ui.a is the embedded asset table the MIME site serves, so the gate
# compares the module against the table the build actually embedded.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
tree="$root/flint-cpu"
gate="$root/gate"
mkdir -p "$gate/obj"

CXX=g++-13
INCS="-I $tree/tools/server/fg-goose-server -I $tree/common -I $tree/vendor -I $tree -I $tree/ggml/include"

serveobj() { # <build-dir> <basename>
    local dir="$1" base="$2"
    local f
    f=$(find "$tree/$dir" -path "*fg-goose-server-core.dir*" -name "$base" | head -1)
    if [ -z "$f" ]; then
        echo "missing object $base in $dir" >&2
        exit 1
    fi
    echo "$f"
}

build_gate() { # <out> <build-dir> <with-unit yes|no>
    local out="$1" dir="$2" with_unit="$3"
    local bridge switches ui
    bridge=$(serveobj "$dir" fg-goose-server.cpp.o)
    switches=$(serveobj "$dir" fg-goose-switches.cpp.o)
    ui="$tree/$dir/tools/ui/libllama-ui.a"
    local objs="$bridge $switches"
    if [ "$with_unit" = yes ]; then
        objs="$objs $(serveobj "$dir" server.c.o) $(serveobj "$dir" host_server.c.o)"
    fi
    $CXX -O1 -std=c++17 -w $INCS -I "$tree/$dir/tools/ui" "$gate/gate.cpp" $objs "$ui" -o "$out" -lz
    echo "built $out <- $dir ($with_unit unit)"
}

build_gate "$gate/gate-on"  build-on  yes
build_gate "$gate/gate-off" build-off no
if [ -d "$tree/build-default" ]; then
    build_gate "$gate/gate-default" build-default yes
fi
