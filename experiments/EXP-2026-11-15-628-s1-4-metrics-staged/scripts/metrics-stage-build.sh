#!/bin/bash
# Staged S1.4: metrics/trace text encoders behind FG_GOOSE_METRICS.
#
# Reproduces, in order: composition of the canonical module, the standalone C
# translation unit, both staged builds, the differential gate against the pristine
# carrier sinks, the byte parity of the three output trees and the symbol audits.
# Everything happens inside this staged copy; no repository path is written and no
# model, server or GPU is involved.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CARRIER=${CARRIER:-/home/end0/AI/FrankenMoE-CUDA}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
COMPOSER=${COMPOSER:-$CARRIER/tools/compose-goose.py}
J=${J:-32}

cd "$ROOT"
mkdir -p goose-gen evidence

# 1. Composition: the canonical module into one translation unit.
python3 "$COMPOSER" goose-gen/metrics-traces.composed.goose \
        goose/metrics/metrics-traces.goose | tee evidence/goose-compose.log

# 2. goose --standalone: composed Goose -> one C file + its exported ABI header.
#    The command also typechecks (90 specializations) and reports the optimization.
"$GOOSE" --standalone -o goose-gen/metrics-traces.c --header goose-gen/metrics-traces.h \
         goose-gen/metrics-traces.composed.goose 2>&1 | tee evidence/goose-generate.log

# 3. Staged builds through this directory's CMakeLists; the flag selects the path.
for mode in OFF ON; do
    cmake -S . -B "build-metrics-${mode,,}" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
          -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_METRICS="$mode" \
          > "evidence/${mode,,}-configure.log" 2>&1
    cmake --build "build-metrics-${mode,,}" -j "$J" \
          --target fg-goose-metrics-smoke fg-goose-metrics-smoke-base \
          > "evidence/${mode,,}-build.log" 2>&1
done

# 4. Gate: 52 cases, each compared byte for byte with the native format.
rm -rf evidence/out-off evidence/out-on evidence/out-base
mkdir -p evidence/out-off evidence/out-on evidence/out-base
./build-metrics-off/fg-goose-metrics-smoke      evidence/out-off  | tee evidence/smoke-off.txt
./build-metrics-on/fg-goose-metrics-smoke       evidence/out-on   | tee evidence/smoke-on.txt
./build-metrics-on/fg-goose-metrics-smoke-base  evidence/out-base | tee evidence/smoke-base.txt

# 5. Byte parity: the pristine carrier, the switch-off build and the switch-on build
#    must have written the same files.
{ diff -r evidence/out-base evidence/out-off && echo "base == off: identical";
  diff -r evidence/out-base evidence/out-on  && echo "base == on : identical";
  diff -r evidence/out-off  evidence/out-on  && echo "off  == on : identical"; } | tee evidence/parity.txt

# 6. Symbol audit.
ON_LIB=build-metrics-on/libfg-bmoe-metrics.so
OFF_LIB=build-metrics-off/libfg-bmoe-metrics.so
nm "$ON_LIB"  > evidence/nm-on.txt
nm "$OFF_LIB" > evidence/nm-off.txt

# One definition per mt_ export in ON, none at all in OFF.
while read -r sym; do
    on=$(nm "$ON_LIB"  | grep -cE " [TtWw] ${sym}\$" || true)
    off=$(nm "$OFF_LIB" | grep -cE " [TtWw] ${sym}\$" || true)
    printf '%-28s on=%s off=%s\n' "$sym" "$on" "$off"
done < <(grep -oE '^int64_t (mt_[a-z0-9_]+)\(' goose-gen/metrics-traces.h | sed -E 's/^int64_t (mt_[a-z0-9_]+)\(/\1/') \
    | tee evidence/nm-export-counts.txt

# The library's dynamic symbol table must not move when the flag is flipped.
nm -D --defined-only "$OFF_LIB" | awk '{print $2, $3}' | sort > evidence/abi-off.txt
nm -D --defined-only "$ON_LIB"  | awk '{print $2, $3}' | sort > evidence/abi-on.txt
nm -D --defined-only build-metrics-on/libfg-metrics-base.so | awk '{print $2, $3}' | sort > evidence/abi-base.txt
diff evidence/abi-off.txt evidence/abi-on.txt && echo "ABI off == on : identical" | tee evidence/abi-parity.txt

# 7. Hashes.
sha256sum goose/metrics/metrics-traces.goose goose-gen/metrics-traces.composed.goose \
          goose-gen/metrics-traces.c goose-gen/metrics-traces.h \
          runtime/core/src/metrics/decode_trace_sink.cpp runtime/core/src/metrics/route_trace_sink.cpp \
          reference/native/metrics/decode_trace_sink.cpp reference/native/metrics/route_trace_sink.cpp \
          common/fg-goose-metrics-bridge.cpp common/fg-goose-metrics-bridge.h \
          tests/fg_goose_metrics_smoke.cpp CMakeLists.txt \
          scripts/metrics-stage-build.sh scripts/metrics-mutate.py scripts/metrics-mutation.sh \
          patches/s1-4/decode_trace_sink.cpp.diff patches/s1-4/route_trace_sink.cpp.diff \
          > evidence/sha-sources.txt
sha256sum "$ON_LIB" "$OFF_LIB" build-metrics-on/libfg-metrics-base.so \
          build-metrics-on/fg-goose-metrics-smoke build-metrics-off/fg-goose-metrics-smoke \
          build-metrics-on/CMakeFiles/fg-goose-metrics.dir/goose-gen/metrics-traces.c.o \
          > evidence/sha-artifacts.txt
stat -c '%10s  %n' goose/metrics/metrics-traces.goose goose-gen/metrics-traces.composed.goose \
          goose-gen/metrics-traces.c goose-gen/metrics-traces.h \
          build-metrics-on/CMakeFiles/fg-goose-metrics.dir/goose-gen/metrics-traces.c.o \
          "$OFF_LIB" "$ON_LIB" build-metrics-on/libfg-metrics-base.so \
          build-metrics-on/fg-goose-metrics-smoke > evidence/sizes.txt
cat evidence/sha-sources.txt evidence/sha-artifacts.txt evidence/sizes.txt
