#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-459-p9-02-shards
OUT=$F/runtime/exp459/build
GOOSE=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p "$OUT"
"$GOOSE" --standalone -o "$E/source/shards-gen.c" --header "$E/source/shards-gen.h" "$E/source/shards.goose"
gcc-13 -O2 -std=c11 -c "$E/source/shards-gen.c" -o "$OUT/gen.o"
g++-13 -O2 -std=c++17 -Wall -Wextra -I"$E/source" -c "$E/tests/shards_fixture.cpp" -o "$OUT/fixture.o"
g++-13 "$OUT/fixture.o" "$OUT/gen.o" -o "$OUT/shards-fixture"
sha256sum "$E/source/shards-gen.c" "$E/source/shards.goose" | awk '{print $1}' > "$OUT/artifact.sha256"
python3 - <<'PY'
import hashlib, json, subprocess
from pathlib import Path
E = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-13-459-p9-02-shards')
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'
ver = subprocess.run([G, 'version'], capture_output=True, text=True).stdout.strip()
json.dump({'compiler': G,
           'compiler_sha256': hashlib.sha256(Path(G).read_bytes()).hexdigest(),
           'version': ver,
           'source_sha256': hashlib.sha256((E/'source/shards.goose').read_bytes()).hexdigest(),
           'generated_sha256': hashlib.sha256((E/'source/shards-gen.c').read_bytes()).hexdigest()},
          open(E/'provenance.json','w'), indent=2)
open(E/'provenance.json','a').write('\n')
PY
"$OUT/shards-fixture"
sha256sum "$E/source/shards-gen.c" "$E/source/shards.goose" | awk '{print $1}' > "$OUT/artifact.sha256"
python3 - <<'PY'
import hashlib, json, subprocess
from pathlib import Path
E = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-13-459-p9-02-shards')
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'
ver = subprocess.run([G, 'version'], capture_output=True, text=True).stdout.strip()
json.dump({'compiler': G,
           'compiler_sha256': hashlib.sha256(Path(G).read_bytes()).hexdigest(),
           'version': ver,
           'source_sha256': hashlib.sha256((E/'source/shards.goose').read_bytes()).hexdigest(),
           'generated_sha256': hashlib.sha256((E/'source/shards-gen.c').read_bytes()).hexdigest()},
          open(E/'provenance.json','w'), indent=2)
open(E/'provenance.json','a').write('\n')
PY
