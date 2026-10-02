#!/bin/bash
# The website (docs/) as it runs on GitHub Pages: served on this computer only (127.0.0.1), then opened.
# Opened as a plain file, browsers refuse the 3D rack's modules; served, everything runs.
cd "$(dirname "$0")/../docs" || exit 1
port=${1:-8765}
python3 -m http.server "$port" --bind 127.0.0.1 >/dev/null 2>&1 &
pid=$!
sleep 0.6
xdg-open "http://127.0.0.1:$port/index.html" >/dev/null 2>&1 &
echo "Serving the site at http://127.0.0.1:$port/ - press Ctrl+C to stop."
trap 'kill $pid' INT TERM
wait $pid
