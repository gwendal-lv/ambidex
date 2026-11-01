Ambidex
=======

Ambidex is a standalone FM synth running two [Dexed](https://github.com/asb2m10/dexed) engines,
A and B, with a different preset in each. A horizontal "morph" slider crossfades from one to the
other, and the A / B buttons choose which preset is shown in the (original) Dexed editor.

Default presets: A = "2. LAURIE", B = "4. PHAROH" (from Dexed's `Dexed_01.syx` cartridge).

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
  keyboard.

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

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Ambidex_Standalone
```

The app is built in `build/ambidex/Ambidex_artefacts/<config>/Standalone/`. On Linux, see Dexed's
[Linux build dependencies](https://github.com/asb2m10/dexed/wiki/Linux-build-dependencies).

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
