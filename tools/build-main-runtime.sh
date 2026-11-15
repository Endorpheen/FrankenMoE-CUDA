#!/bin/bash
# Build the single composed Goose runtime core from the accepted canonical
# modules. This is the growing main build: every accepted module joins the list
# below, and the script proves on each run that the unit is compatible, that C
# is generated, that it compiles and that no global symbol is defined twice.
#
# It does not link a host and runs no model: the native helper entry points are
# extern symbols supplied by the per-slice fixtures until the runtime wiring
# stage lands them.
#
# Usage: bash tools/build-main-runtime.sh [out-dir | --repro]
#   out-dir defaults to build/goose-main (build artefacts are not committed).
#   --repro builds twice into the default directory and fails unless main.c and
#   main.o are byte-identical, which is the determinism gate for P10-06.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
REPRO=0
if [ "${1:-}" = "--repro" ]; then
    REPRO=1
    OUT=$ROOT/build/goose-main
else
    OUT=${1:-$ROOT/build/goose-main}
fi
UNIT=$OUT/main.goose
C=$OUT/main.c
H=$OUT/main.h
OBJ=$OUT/main.o
mkdir -p "$OUT"

# Accepted canonical modules. Model builders stay out: they are per-architecture
# alternatives, not one unit.
MODULES=(
    goose/model/goose-model-graph.goose
    goose/model/goose-model-loader.goose
    goose/model/file-io.goose
    goose/model/arch-registry.goose
    goose/model/quant-types.goose
    goose/model/quant-allow.goose
    goose/model/quant-rows.goose
    goose/model/quant-rows2.goose
    goose/model/quant-dequant.goose
    goose/model/quant-dequant2.goose
    goose/model/quant-weighted.goose
    goose/model/gguf-metadata.goose
    goose/model/gguf-writer.goose
    goose/model/gguf-tensors.goose
    goose/model/gguf-split.goose
    goose/model/expert-lru.goose
    goose/model/dense-place.goose
    goose/model/stream-plan.goose
    goose/model/hook-rules.goose
    goose/model/overlap-rules.goose
    goose/model/row-select.goose
    goose/cpu/goose-meta-backend.goose
    goose/cpu/goose-opt.goose
    goose/model/vocab-type.goose
    goose/model/control-vectors.goose
    goose/model/lora-apply.goose
    goose/model/special-partition.goose
    goose/misc/common-strings.goose
    goose/misc/edge0-pack-index.goose
    goose/misc/release-plan.goose
    goose/misc/build-env.goose
    goose/misc/generated-artifact-policy.goose
    goose/misc/ggml-type-table.goose
    goose/misc/public-enum-values.goose
    goose/misc/public-name-tables.goose
    goose/misc/file-reader-windows.goose
    goose/misc/proc-readers.goose
    goose/misc/shard-naming.goose
    goose/misc/vendor-licence-payload.goose
    goose/misc/vendor-provenance.goose
    goose/misc/vendor-selection.goose
    goose/misc/cpu-features.goose
    goose/misc/platform-paths.goose
    goose/misc/backend-select.goose
    goose/misc/backend-headers.goose
    goose/misc/gguf-headers.goose
    goose/misc/soname-exports.goose
    goose/misc/device-headers.goose
    goose/misc/cpp-headers.goose
    goose/misc/sched-eval.goose
    goose/cuda/dispatch.goose
    goose/jinja/runtime.goose
    goose/json/format-grammar.goose
    goose/cli/config-validate.goose
    goose/cli/config-resolve.goose
    goose/cli/cli-protocol.goose
    goose/cli/llama-bench-report.goose
    goose/cli/tokenize-cli.goose
    goose/cli/fit-params-line.goose
    goose/metrics/metrics-traces.goose
    goose/misc/spec-decode-decisions.goose
    goose/misc/session-lifecycle.goose
    goose/misc/runtime-facade.goose
    goose/misc/draft-source.goose
    goose/misc/load-admission.goose
    goose/misc/perplexity-plan.goose
    goose/misc/kv-turn-state.goose
    goose/misc/tallies.goose
    goose/misc/prefill-plan.goose
    goose/misc/trace-frame-order.goose
    goose/misc/moe-predict-report.goose
    goose/misc/sampler-chain.goose
    goose/misc/mtp-capture-warmup.goose
    goose/misc/cli-driver-loop.goose
    goose/server/mcp-core.goose
    goose/server/mcp-stdio.goose
    goose/server/mcp-integration.goose
    goose/server/model-manager.goose
    goose/server/hot-reload.goose
    goose/server/connectors.goose
    goose/server/decision-core.goose
    goose/server/server-schema.goose
    goose/server/tools-pure.goose
    goose/server/log-policy.goose
    goose/server/console-editing.goose
    goose/server/hf-cache-naming.goose
    goose/server/download-selection.goose
    goose/server/http-url-policy.goose
    goose/server/jsonparse.goose
    goose/server/route-table.goose
    goose/server/web-assets.goose
    goose/server/mime-table.goose
    goose/server/asset-etag.goose
    goose/chat/thinking-control.goose
    goose/chat/parse-params.goose
    goose/chat/session-loop.goose
    goose/cli/jsonparse.goose
    goose/json/json-dump.goose
    goose/json/ac.goose
    goose/json/allof.goose
    goose/json/ast-helpers.goose
    goose/json/builder.goose
    goose/json/json-parser.goose
    goose/json/peg-invalid.goose
)

echo "[1/5] compose ${#MODULES[@]} modules"
python3 "$ROOT/tools/compose-goose.py" "$UNIT" "${MODULES[@]/#/$ROOT/}"

echo "[2/5] typecheck"
"$GOOSE" check "$UNIT" 2>&1 | grep -E "typechecked ok|error" | tail -2

echo "[3/5] generate C"
"$GOOSE" --standalone -o "$C" --header "$H" "$UNIT" >/dev/null

echo "[4/5] compile object at -O1"
"$CC" -O1 -std=c11 -w -c "$C" -o "$OBJ"

echo "[5/5] symbol check"
defined=$(nm -g --defined-only "$OBJ" | wc -l)
dups=$(nm -g --defined-only "$OBJ" | awk '{print $3}' | sort | uniq -d | wc -l)
ext=$(nm -u "$OBJ" | wc -l)
if [ "$dups" -ne 0 ]; then
    nm -g --defined-only "$OBJ" | awk '{print $3}' | sort | uniq -d | sed 's/^/  duplicate: /'
    echo "main runtime: $dups duplicate global symbol(s)"
    exit 1
fi
printf 'main runtime ok: %s global symbols, 0 duplicates, %s extern, C %s bytes, object %s bytes\n' \
    "$defined" "$ext" "$(stat -c %s "$C")" "$(stat -c %s "$OBJ")"

if [ "$REPRO" -eq 1 ]; then
    first_c=$(sha256sum "$C" | cut -d' ' -f1)
    first_o=$(sha256sum "$OBJ" | cut -d' ' -f1)
    bash "$0" "$OUT" >/dev/null
    second_c=$(sha256sum "$C" | cut -d' ' -f1)
    second_o=$(sha256sum "$OBJ" | cut -d' ' -f1)
    if [ "$first_c" != "$second_c" ] || [ "$first_o" != "$second_o" ]; then
        echo "repro: FAIL (C $first_c -> $second_c, object $first_o -> $second_o)"
        exit 1
    fi
    echo "repro: ok (C ${first_c:0:16}, object ${first_o:0:16})"
fi
