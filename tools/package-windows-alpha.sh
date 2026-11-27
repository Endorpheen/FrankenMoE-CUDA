#!/usr/bin/env bash
# Package a completed Windows x64 build with its existing branded assets.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build=$(cd "${1:?usage: package-windows-alpha.sh build-dir [release-tag]}" && pwd)
tag=${2:-v0.11.0-windows-alpha.1}
case "$tag" in *[!a-zA-Z0-9.-]*|'') exit 2;; esac
dir="$root/integration/goose-standalone/windows"
package="GooseInfer-$tag-windows-x64"
staging="$build/package/$package"
mkdir -p "$staging/licenses"
for exe in gooseinfer-server.exe gooseinfer-launcher.exe frankenmoe-server.exe frankenmoe-launcher.exe; do
    file "$build/$exe" | grep -q 'PE32+.*x86-64'
    install -m 0755 "$build/$exe" "$staging/$exe"
done
for f in start.bat settings.bat collect-diagnostics.bat README.md; do
    install -m 0644 "$dir/$f" "$staging/$f"
done
install -m 0644 "$root/LICENSE" "$staging/LICENSE"
install -m 0644 "$root/vendor/flint/LICENSE" "$staging/licenses/ggml-MIT.txt"
install -m 0644 "$root/runtime/LICENSE.BigMoeOnEdge" "$staging/licenses/BigMoeOnEdge.txt"
install -m 0644 "${MINGW_NOTICE:-/usr/share/doc/mingw-w64-common/copyright}" "$staging/licenses/MinGW-w64.txt"
install -m 0644 "${MINGW_GCC_NOTICE:-/usr/share/doc/gcc-mingw-w64-base/copyright}" "$staging/licenses/MinGW-GCC.txt"
test -s "$build/web/index.html"
cp -a "$build/web/." "$staging/web/"
python3 - "$staging" "$build/$package.zip" <<'PY'
import hashlib
from pathlib import Path
import sys
from zipfile import ZIP_DEFLATED, ZipFile
src, dest = map(Path, sys.argv[1:])
files = sorted(p for p in src.rglob("*") if p.is_file())
if any(p.is_symlink() or p.suffix.lower() == ".gguf" for p in files):
    raise SystemExit("Unexpected model weights or symlink in package")
with ZipFile(dest, "w", ZIP_DEFLATED, compresslevel=6) as archive:
    for p in files:
        archive.write(p, str(p.relative_to(src.parent)))
with ZipFile(dest) as archive:
    if archive.testzip() is not None:
        raise SystemExit("Windows ZIP integrity check failed")
    for exe in ("gooseinfer-server.exe", "gooseinfer-launcher.exe", "frankenmoe-server.exe", "frankenmoe-launcher.exe"):
        if archive.read(f"{src.name}/{exe}") != (src / exe).read_bytes():
            raise SystemExit(f"Executable bytes mismatch: {exe}")
digest = hashlib.sha256(dest.read_bytes()).hexdigest()
(dest.parent / "SHA256SUMS").write_text(f"{digest}  {dest.name}\n")
PY
(cd "$build" && sha256sum -c SHA256SUMS)
