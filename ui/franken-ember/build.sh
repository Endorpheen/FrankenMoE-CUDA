#!/usr/bin/env bash
# Build the Ember Goose site from the shipped llama-ui bundle.
#
# The bundle in this tree is prebuilt and gzipped, and it is not modified in place: this script copies it, decompresses it, and
# adds two things to index.html - a stylesheet link and the ember mark. The result is a complete static site that the server can
# mount with --path, and the original stays untouched for rollback.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
SRC=${SRC:-$ROOT/vendor/flint/tools/ui/dist}
SITE=$HERE/site

[ -d "$SRC/_app" ] || { echo "no UI bundle at $SRC"; exit 1; }

rm -rf "$SITE"
mkdir -p "$SITE"
cp -a "$SRC/." "$SITE/"

# Every file in the bundle is gzip-compressed; the server serves them with a content-encoding, but a mounted directory is served
# as-is, so the copy is decompressed once here.
python3 - "$SITE" <<'PY'
import pathlib, sys
site = pathlib.Path(sys.argv[1])
n = 0
for p in site.rglob("*"):
    if not p.is_file():
        continue
    with open(p, "rb") as f:
        magic = f.read(2)
    if magic == b"\x1f\x8b":
        import gzip
        data = gzip.decompress(p.read_bytes())
        p.write_bytes(data)
        n += 1
print(f"decompressed {n} file(s)")
PY

# Theme and mark, placed where the static site can reach them.
# The bundle's assets are immutable-cached by the browser, so the theme file carries a content hash in its name: a change to the
# CSS always lands as a new URL, and a stale theme can never survive an edit.
HASH=$(sha256sum "$HERE/theme/ember.css" | cut -c1-10)
cp "$HERE/theme/ember.css" "$SITE/_app/immutable/assets/ember.$HASH.css"
cp "$HERE/assets/gooselogo.png"     "$SITE/_app/immutable/assets/gooselogo.png"
cp "$HERE/assets/gooselogo-big.png" "$SITE/_app/immutable/assets/gooselogo-big.png"
cp "$HERE/assets/favicon.svg" "$SITE/favicon.svg"

python3 - "$SITE" <<'PY'
import pathlib, re, sys
site = pathlib.Path(sys.argv[1])
index = site / "index.html"
html = index.read_text()
import hashlib
css = sorted((site / "_app/immutable/assets").glob("ember.*.css"))[-1]
link = f'\t\t<link href="./_app/immutable/assets/{css.name}" rel="stylesheet">\n'
html = re.sub(r'\t\t<link href="./_app/immutable/assets/ember[^"]*\.css" rel="stylesheet">\n', "", html)
html = html.replace("</head>", link + "\t</head>")
index.write_text(html)
print("index.html carries the theme link")
PY

echo "built: $SITE"
