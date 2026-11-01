#!/usr/bin/env bash
# Runs the benchmark tool (bench/) of one or more presets, on this machine or
# on another Linux machine with the same CPU architecture (e.g. a Raspberry Pi),
# pinned to one CPU core. The output is also saved in build/bench-results/.
#
# Usage: scripts/bench.sh [--host <ssh-host>] [--core N] [preset...] [-- bench arguments]
#   presets: release (default), release-scalar
#   bench arguments: see `ambidex-bench --help`, e.g. -- app --seconds 20
# The presets must be built first: cmake --preset <p> && cmake --build --preset bench-<p>
set -euo pipefail

host=""
core=2
presets=()
while [ $# -gt 0 ]; do
    case "$1" in
        --host) host=${2:?}; shift 2 ;;
        --core) core=${2:?}; shift 2 ;;
        --) shift; break ;;
        *) presets+=("$1"); shift ;;
    esac
done
[ ${#presets[@]} -gt 0 ] || presets=(release)
bench_args=("$@")

cd "$(dirname "$0")/.."
results_dir="build/bench-results"
mkdir -p "$results_dir"
machine=${host:-$(hostname)}
remote_dir="ambidex-bench"   # relative to the remote home folder

# Machine state, printed before each run
info='echo "# $(hostname), $(uname -r), $(grep -m1 -E "model name|Model" /proc/cpuinfo || true)";
      echo "# governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo n/a)";
      if command -v vcgencmd >/dev/null; then echo "# $(vcgencmd measure_temp), $(vcgencmd measure_clock arm), $(vcgencmd get_throttled)"; fi'

for preset in "${presets[@]}"; do
    binary="build/$preset/bench/ambidex-bench"
    [ -x "$binary" ] || { echo "not built: $binary" >&2; exit 1; }
    out="$results_dir/$machine-$preset.txt"
    echo "=== $preset on $machine -> $out"

    if [ -n "$host" ]; then
        ssh "$host" "mkdir -p $remote_dir/$preset"
        rsync -az "$binary" "$host:$remote_dir/$preset/"
        # (the bench creates its default settings in an "Ambidex" folder next to the binary)
        ssh "$host" "$info; echo '# preset: $preset'; cd $remote_dir/$preset && taskset -c $core ./ambidex-bench ${bench_args[*]:-}" | tee "$out"
    else
        bash -c "$info; echo '# preset: $preset'; taskset -c $core $binary ${bench_args[*]:-}" | tee "$out"
    fi
done
