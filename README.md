Ambidex
=======

Ambidex is a standalone FM synth running two [Dexed](https://github.com/asb2m10/dexed) engines,
A and B, with a different preset in each. A horizontal "morph" slider crossfades from one to the
other, and the A / B buttons choose which preset is shown in the (original) Dexed editor.

The mix of both engines goes through an FX chain (distortion, filter, compressor, modulation, delay
and reverb), whose knobs are also morphed between their A and B values, then a soft clipper that
keeps the output below 0 dBFS: see `doc/Effects.md`.

Presets A and B are chosen from a library of Ambidex presets, which hold the FM patch (a DX7
voice), Dexed's settings and the FX settings. Dexed's cartridges are only used to load FM patches.
Presets A and B, with their unsaved edits, are restored at each launch: see `doc/Presets.md`.

Default presets: A = "Chorus Rhodes", B = "Pharoh" (factory presets).

Based on Dexed
--------------

Ambidex is built on 
[asb2m10/dexed@fea2f0f](https://github.com/asb2m10/dexed/commit/fea2f0f0f2c397799f59b27eb11c038cd34b22c5)
(Dexed 1.1.0 development version).

* `source/` is Dexed's code, kept unmodified as much as possible. Its CMake file now only defines
  the `dexed` library used by Ambidex (the Dexed plugin targets, Projucer project and installers
  were removed).
* `ambidex/` is the Ambidex app: `AmbidexProcessor` hosts the two Dexed engines and crossfades
  them, `AmbidexEditor` wraps the Dexed editor with the A/B buttons, the morph slider and the
  keyboard, and `FxChain` / `FxPanel` are the FX chain (built on JUCE's DSP classes).
  `PresetLibrary` stores the presets, and `DexedEditorCustomiser` adapts Dexed's editor to them
  (hides the logo, INIT, STORE and the program list, turns CART into "Load FM patch"...) from the
  outside, without changing Dexed's code.

All of Dexed's default cartridges are embedded in the app: on first launch, they are unpacked
into Ambidex's data folder, which also holds the preferences (`Dexed.xml`) and an optional
`DexedTheme.xml` (see `doc/DexedTheme.md`):

* macOS: `~/Library/Application Support/Ambidex`
* Windows: `%APPDATA%\Ambidex`
* Linux: `$XDG_DATA_HOME/Ambidex`, or `~/.local/share/Ambidex`

A folder named `Ambidex` next to the app (or next to the `.app` bundle on macOS) is used instead,
if it exists (portable setup).

How to build
------------

Requirements: CMake 3.21+, [Ninja](https://ninja-build.org/) and a C++17 compiler (GCC or Clang
on Linux, Xcode on macOS).

On Linux (Debian / Ubuntu), install the build dependencies with:

```
sudo apt install build-essential cmake ninja-build pkg-config \
    libasound2-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
    libxcursor-dev libfreetype-dev libfontconfig1-dev libgl-dev libgtk-3-dev \
    libwebkit2gtk-4.1-dev libcurl4-openssl-dev
```

Then build with the CMake presets (`debug` or `release`):

```
cmake --preset release
cmake --build --preset release
```

The app is built in `build/<preset>/ambidex/Ambidex_artefacts/<Debug|Release>/Standalone/`.
JUCE's sources are heavy to compile: if the build runs out of memory, limit the number of parallel
jobs, e.g. `cmake --build --preset release -j 4`.

To test on another Linux machine with the same CPU architecture (e.g. a Raspberry Pi),
`scripts/run-remote.sh <ssh-host> [debug|release]` copies the built app to `~/ambidex-dev` on that
machine and runs it on its display.

Factory presets
---------------

The factory presets are the preset files in `ambidex/presets/`: they are embedded in the app when
it is built, and copied into the user's preset library on first launch (when the library folder
doesn't exist yet). To change them, edit and save presets in Ambidex, then copy them from your local
library into the repository:

```
scripts/update-factory-presets.sh                  # updates all the factory presets
scripts/update-factory-presets.sh "My New Preset"  # adds (or updates) a preset
```

Then rebuild, and commit the changed files. To delete a factory preset, delete its file from
`ambidex/presets/` (and re-run CMake). Existing libraries are not updated: to get the new factory
presets, delete (or rename) the local `Presets` folder (see `doc/Presets.md`).

Dependencies are vendored in `libs/` (no git submodules): JUCE 8.0.11, clap-juce-extensions,
MTS-ESP, tuning-library and surgesynthteam_tuningui. JUCE is trimmed to `modules/` and
`extras/Build/` (no examples, no Projucer or other extras), so `JUCE_BUILD_EXAMPLES` and
`JUCE_BUILD_EXTRAS` must stay off.

License
-------

Ambidex, like Dexed, is licensed under the GPL v3 (see `LICENSE`). The msfa component (in
`source/msfa`) stays under the Apache 2.0 license.

Dexed credits
-------------
* Dexed by Pascal Gauthier and contributors, imported from
[asb2m10/dexed@fea2f0f](https://github.com/asb2m10/dexed/commit/fea2f0f0f2c397799f59b27eb11c038cd34b22c5)
* DX Synth engine : Raph Levien and the [msfa](https://github.com/google/music-synthesizer-for-android) team
* [Surge Synth Team](https://surge-synth-team.org/) for substantial contributions like microtuning and MPE support.
* Graphical design : AZur Studio
* [Sentinel77](https://github.com/Sentinel77) for numerous engine fixes
* LP Filter : Filatov Vadim (2DaT); taken from the excellent [Obxd](https://obxd.wordpress.com) project
* PPPlay : Great [OPL3](https://github.com/stohrendorf/ppplay) implementation, with documented code :D
* DX7 program compilation : Jean-Marc Desprez (author of [SynprezFM](http://www.synprez.com/SynprezFM))
* DX7 programs : Dave Benson, Frank Carvalho, Tim Conrardy, Jack Deckard, Chris Dodunski, Tim Garrett, Hitaye, Stephan Ibsen, Christian Jezreel, Narfman, Godric Wilkie
* falkTX [distrho](http://distrho.sourceforge.net/)
