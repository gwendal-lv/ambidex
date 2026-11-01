Presets
=======

Ambidex plays two presets at once, A and B, and the morph slider crossfades between them. Both
are chosen from your **preset library**.

What a preset is
----------------

A preset holds everything you hear from one side of the morph:

* the **FM patch**: the DX7 voice played by the Dexed engine (operators, envelopes, algorithm,
  LFO...), with your edits;
* the **FM engine settings**: Dexed's tune, cutoff, resonance, output level, mono mode, pitch bend
  and modulation wheel settings, and tuning (scales);
* the values of all the **FX knobs** (see `Effects.md`).

Choosing presets A and B
------------------------

Click the name of preset A (or B), at the top of the window: this moves the morph slider fully to
that side, and opens the preset menu. Choose a preset in the list to load it.

The preset of a side can only be edited while the morph slider is fully on that side (see the
README).

Editing and saving
------------------

Your edits change the preset of the side, not the library: a modified preset shows a `*` after its
name (e.g. `Laurie *`). The preset menu then offers:

* **Save**: replaces the preset in the library with your edited version;
* **Save as new preset...**: adds your edited version to the library, under a new name;
* **Revert to saved**: discards your edits, and loads the preset from the library again;
* **Rename...** and **Delete...**: rename the preset, or delete it from the library. After a
  deletion, the side keeps playing the preset, as an unsaved preset (`*`): save it as a new preset
  to keep it.

Choosing another preset while the current one has unsaved edits asks for confirmation first.

The same preset can be chosen for A and B: each side then has its own copy, which you can edit
differently (e.g. to morph only the FX).

Starting from a DX7 patch
-------------------------

**Load FM patch** (in the Dexed editor) opens a menu with Dexed's cartridges and their voices: the
chosen voice replaces the FM patch of the preset being edited. The FM engine settings and the FX
of the preset are kept. Save the result as a new preset to keep it.

The FM patch of a preset is shown at the bottom left of the Dexed editor ("FM patch: ...").
**FM Engine Settings** opens Dexed's settings: engine type (emulation), pitch bend range, MIDI...

The cartridges are only a source of FM patches: Ambidex never writes to them. They are in the
`Cartridges` folder of Ambidex's data folder (see below), where Dexed's default cartridges are put
on first launch. Other DX7 cartridges (`.syx` files) copied there also appear in the menu.

Automatic save
--------------

When Ambidex quits normally (closing its window, or quitting from the menu or with Cmd+Q on
macOS), it remembers presets A and B as they are, including their unsaved edits, and restores them
the next time it starts. If Ambidex crashes or is killed, the changes made since it was started are
lost (the library is only changed by the preset menu).

Not remembered: the morph slider (Ambidex always starts fully on A, with preset A editable), and
the FX switch (always on at start).

The **Options** button at the top left of the window can also save the state of both sides to a
file, and load it again ("Save current state...", "Load a saved state..."), or reset them to the
default presets.

Where presets are stored
------------------------

Each preset is a file named after it (`<name>.ambidexpreset`), in the `Presets` folder of
Ambidex's data folder:

* macOS: `~/Library/Application Support/Ambidex/Presets`
* Windows: `%APPDATA%\Ambidex\Presets`
* Linux: `~/.local/share/Ambidex/Presets` (or `$XDG_DATA_HOME/Ambidex/Presets`)

Preset files can be copied to back them up, or to share them: they appear in the menu once copied
into this folder.

On first launch, the folder is created with the factory presets: "Laurie", "Pharoh", "Chorus
Rhodes", "Driven Bass", "Space Bells", "Phased Strings", "Flanged Clav" and "Filtered Pad". Ambidex
then starts with "Chorus Rhodes" as preset A, and "Pharoh" as preset B. To get the factory presets
back, delete (or rename) the `Presets` folder: it is created again at the next launch.

The automatic save (presets A and B, and the audio and MIDI settings) is a separate file:
`~/Library/Application Support/Ambidex.settings` on macOS, `%APPDATA%\Ambidex\Ambidex.settings` on
Windows, `~/.config/Ambidex.settings` on Linux.

Known limitation
----------------

In the version of Dexed that Ambidex is built on, the 4 **PITCH EG LEVEL** knobs don't change the
sound: the levels of the pitch envelope stay those of the FM patch (they are kept in presets).
Most patches don't use the pitch envelope (all levels at 50).
