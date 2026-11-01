Ambidex benchmark
=================

`ambidex-bench` renders Ambidex headless (no audio device, no UI) to measure its CPU load, and
checks that the NEON FM kernels give exactly the same output as Dexed's original scalar code.

```
ambidex-bench [kernel|check|app] [--block N] [--seconds S] [--repeats R]
```

* `check`: compares the kernels (`FmOpKernel::compute/compute_pure`, `EngineMkI::compute/compute_pure`)
  with copies of the original scalar loops (`ReferenceKernels.h`): every phase, then 1 million random
  calls each. Exit code 1 if any output differs.
* `kernel`: `check`, then times the kernels (ns per 64-sample call, scalar reference vs current build)
  and one 6-operator voice (`render()`, mean over the 32 algorithms).
* `app`: creates `AmbidexAudioProcessor` (2 Dexed engines, default presets) and times each
  `processBlock` call for several scenarios, played by both engines:
  * `held N`: N keys held during the whole run;
  * `chords 8`: 8-note chords struck every second (all 16 voices cycle);
  * `released 16`: a 16-note chord released after 0.9 s, measured from 1 s (release tails).

  Each scenario runs with both engine types (Modern and Mark I), and reports the CPU load (time in
  `processBlock` / audio duration, for one core), the 99th percentile and worst block (relative to the
  block duration), and a hash of the rendered audio: builds that render the same audio give the same
  hash (e.g. scalar vs NEON, or VM vs Raspberry Pi).
* no mode: `kernel`, then `app`.

Lines starting with `csv,` repeat the results in a machine-readable form.

The benchmark starts from Ambidex's default settings: it creates an `Ambidex` folder next to its
executable, used as Ambidex's data folder (see `README.md`). Heap allocations are zeroed in this tool,
so that Dexed's few uninitialised members (`Lfo::phase_` and `randstate_`, `Dx7Note::fb_buf_`) don't
make the audio differ from run to run.

Building and running
--------------------

The benchmark is built with the `release*` presets (`AMBIDEX_BUILD_BENCH=ON`):

| configure preset | FM kernels                       |
|------------------|----------------------------------|
| `release`        | NEON (on ARM)                    |
| `release-scalar` | scalar (the original Dexed code) |

```
cmake --preset release && cmake --build --preset bench-release
```

`scripts/bench.sh` runs the benchmark of one or more presets, pinned to one CPU core, either locally
or on another Linux machine with the same architecture (copied there with rsync), and saves the
output in `build/bench-results/<machine>-<preset>.txt`:

```
scripts/bench.sh release-scalar release                  # this machine
scripts/bench.sh --host <ssh-host> release-scalar release  # e.g. a Raspberry Pi
scripts/bench.sh --host <ssh-host> release -- app --seconds 30
```

(`scripts/bench.sh` uses `taskset`: Linux only.)
