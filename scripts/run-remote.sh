#!/usr/bin/env bash
# Copies the locally built Ambidex app to another Linux machine with the same CPU
# architecture (e.g. a Raspberry Pi), then runs it there on the local display (:0).
# Stopping this script (Ctrl+C) also stops the remote app.
#
# Usage: scripts/run-remote.sh <ssh-host> [debug|release]
set -euo pipefail

host=${1:?usage: $0 <ssh-host> [debug|release]}
preset=${2:-release}
case "$preset" in
    debug) config=Debug ;;
    release) config=Release ;;
    *) echo "unknown preset: $preset" >&2; exit 1 ;;
esac

cd "$(dirname "$0")/.."
binary="build/$preset/ambidex/Ambidex_artefacts/$config/Standalone/Ambidex"
remote_dir="ambidex-dev"   # relative to the remote home folder

ssh "$host" "mkdir -p $remote_dir"
rsync -az --progress "$binary" "$host:$remote_dir/"

# -t: the remote app gets a terminal, and is closed with it
exec ssh -t "$host" "pkill -x Ambidex; cd $remote_dir && DISPLAY=\${DISPLAY:-:0} exec ./Ambidex"
