#!/bin/bash
# Compose the canonical module sets that are known to share one unit and check
# that Goose accepts each composition. Fast smoke test for the composition step;
# it does not link C or run the per-slice gates.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=$(mktemp -d /tmp/opencode/compose-check.XXXXXX)
trap 'rm -rf "$OUT"' EXIT

check() {
    local name=$1; shift
    if python3 "$ROOT/tools/compose-goose.py" "$OUT/$name.goose" "$@" >/dev/null 2>"$OUT/$name.err"; then
        if "$GOOSE" check "$OUT/$name.goose" >/dev/null 2>&1; then
            echo "ok   $name"
            return 0
        fi
        echo "FAIL $name: goose check"
    else
        echo "FAIL $name: $(grep -m1 'differs' "$OUT/$name.err" || echo 'compose error')"
    fi
    return 1
}

fails=0
check server   "$ROOT"/goose/server/{mcp-core,mcp-stdio,decision-core,server-schema,tools-pure}.goose || fails=$((fails+1))
check quant    "$ROOT"/goose/model/{quant-types,quant-allow,quant-rows,quant-rows2,quant-dequant,quant-dequant2}.goose || fails=$((fails+1))
check gguf     "$ROOT"/goose/model/{gguf-metadata,gguf-writer,gguf-tensors,gguf-split}.goose || fails=$((fails+1))
check p9-06    "$ROOT"/goose/cli/cli-protocol.goose "$ROOT"/goose/chat/{thinking-control,parse-params}.goose || fails=$((fails+1))
check jinja-schema "$ROOT"/goose/jinja/runtime.goose "$ROOT"/goose/json/format-grammar.goose || fails=$((fails+1))
check app      "$ROOT"/goose/server/{mcp-core,mcp-stdio,decision-core,server-schema,tools-pure}.goose \
               "$ROOT"/goose/chat/{thinking-control,parse-params}.goose \
               "$ROOT"/goose/cli/cli-protocol.goose || fails=$((fails+1))

if [ "$fails" -ne 0 ]; then
    echo "composition: $fails set(s) failed"
    exit 1
fi
echo "composition: 6 set(s) ok"
