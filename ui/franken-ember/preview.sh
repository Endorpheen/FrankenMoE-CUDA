#!/usr/bin/env bash
# Show Ember Goose without a model, without CUDA and without touching an accepted build.
#
# Two servers are offered because they answer different questions:
#   server - the shipped llama-server with --path, an empty --models-dir and --no-models-autoload. This is the path the interface
#            is actually delivered by, so it is the one that proves the theme works where it ships.
#   dev    - a plain static file server on the same site directory, for a quick look with live reload of nothing but CSS.
#
# Port 9137 by default: it does not collide with the Apex port 9101 or the router check on 9102.
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
MODE=${1:-server}
PORT=${PORT:-9137}
MODELDIR=${MODELDIR:-$HERE/models-empty}

[ -f "$HERE/site/index.html" ] || bash "$HERE/build.sh"
mkdir -p "$MODELDIR"

if ss -ltn 2>/dev/null | grep -q ":$PORT\b"; then
	echo "port $PORT is already in use; choose another with PORT=…"; exit 1
fi

case "$MODE" in
server)
	BIN=${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin/llama-server}
	[ -x "$BIN" ] || { echo "no llama-server at $BIN"; exit 1; }
	echo "Ember Goose (llama-server --path): http://127.0.0.1:$PORT/"
	echo "  theme:  file://$HERE/theme/ember.css"
	echo "  stop:   Ctrl-C"
	LD_LIBRARY_PATH=$(dirname "$BIN") exec "$BIN" \
		--path "$HERE/site" --models-dir "$MODELDIR" --no-models-autoload \
		--host 127.0.0.1 --port "$PORT"
	;;
dev)
	echo "Ember Goose (static dev server): http://127.0.0.1:$PORT/"
	echo "  stop: Ctrl-C"
	exec python3 -m http.server "$PORT" --bind 127.0.0.1 --directory "$HERE/site"
	;;
*)
	echo "usage: preview.sh [server|dev]   (PORT=9137 by default)"; exit 2 ;;
esac
