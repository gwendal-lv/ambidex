#!/usr/bin/env bash
# Copies presets from the local Ambidex preset library to the factory presets
# of the repository (ambidex/presets), which are embedded in the app at the
# next build. Commit the changed files afterwards.
#
# Usage:
#   scripts/update-factory-presets.sh             updates all the factory presets
#   scripts/update-factory-presets.sh "Name" ...  adds (or updates) these presets
#
# The local library is found as Ambidex does (set AMBIDEX_PRESETS to use another one).
set -euo pipefail

cd "$(dirname "$0")/.."
factory_dir="ambidex/presets"

if [[ -n "${AMBIDEX_PRESETS:-}" ]]; then
    library_dir="$AMBIDEX_PRESETS"
elif [[ "$(uname)" == "Darwin" ]]; then
    library_dir="$HOME/Library/Application Support/Ambidex/Presets"
else
    library_dir="${XDG_DATA_HOME:-$HOME/.local/share}/Ambidex/Presets"
fi

if [[ ! -d "$library_dir" ]]; then
    echo "No preset library found in: $library_dir" >&2
    exit 1
fi

names=("$@")
if [[ ${#names[@]} -eq 0 ]]; then
    for file in "$factory_dir"/*.ambidexpreset; do
        names+=("$(basename "$file" .ambidexpreset)")
    done
fi

status=0
for name in "${names[@]}"; do
    source_file="$library_dir/$name.ambidexpreset"
    if [[ ! -f "$source_file" ]]; then
        echo "missing:   $name (not in $library_dir)" >&2
        status=1
    elif cmp -s "$source_file" "$factory_dir/$name.ambidexpreset"; then
        echo "unchanged: $name"
    else
        cp "$source_file" "$factory_dir/"
        echo "updated:   $name"
    fi
done
exit $status
