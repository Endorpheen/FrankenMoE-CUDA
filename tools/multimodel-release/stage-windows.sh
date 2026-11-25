#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a fresh Windows staging directory}
apex=${2:?Specify an extracted Windows Apex package}
server=${3:?Specify the linked Windows multi-model executable}
mtp=${4:?Specify the linked Windows Bonsai MTP executable}
linux=${5:?Specify the verified Linux release staging directory}
mkdir "$out"
cp -a "$apex/." "$out/"
if [[ -f "$out/README.md" && ! -f "$out/README-Apex.md" ]]; then
    mv "$out/README.md" "$out/README-Apex.md"
fi
mkdir -p "$out/"{bonsai,cuda,profiles}
install -m 755 "$server" "$out/frankenmoe-multimodel.exe"
install -m 755 "$mtp" "$out/bonsai/bonsai-mtp.exe"
install -m 644 "$src/start-windows.ps1" "$out/Start-Model.ps1"
install -m 644 "$linux/profiles/inkling.env" "$out/profiles/inkling.env"
install -m 644 "$linux/cuda/"{bonsai-http.cubin,frozen-goose.ptx} "$out/cuda/"
install -m 644 "$linux/cuda/"{bonsai-mtp.cubin,frozen-goose.ptx} "$out/bonsai/"
install -m 644 "$root/docs/WINDOWS-MULTIMODEL.md" "$out/README.md"
(
    cd "$out"
    find . -type f ! -name CONTENTS.sha256 -print0 | sort -z | xargs -0 sha256sum > CONTENTS.sha256
)
