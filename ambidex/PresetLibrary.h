// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

#include "FxParameters.h"

/**
 * An Ambidex preset: a complete FM engine (a DX7 voice and Dexed's own
 * settings), and the values of all the FX knobs.
 */
struct AmbidexPreset {
    static constexpr int voiceSize = 155; // a DX7 voice, unpacked (see Dexed's Program)

    String name;
    std::array<uint8_t, voiceSize> voice {};
    MemoryBlock engineState; // Dexed's state (empty = Dexed's default settings)
    std::array<float, fx::numParams> fxValues {};

    ValueTree toValueTree() const;
    /** Returns false if the tree doesn't hold a preset. */
    bool fromValueTree(const ValueTree& tree);
};

//==============================================================================
/**
 * The user presets: one file per preset in a folder, named after the preset.
 * The folder is filled with the factory presets the first time.
 */
class PresetLibrary {
public:
    explicit PresetLibrary(const File& folder);

    StringArray getNames() const; // sorted
    bool contains(const String& name) const;
    bool load(const String& name, AmbidexPreset& preset) const;
    bool save(const AmbidexPreset& preset);
    bool remove(const String& name);
    bool rename(const String& oldName, const String& newName);

    /** Returns a name that can be used for a preset file (or an empty string). */
    static String toValidName(const String& name);

private:
    File getFile(const String& name) const;
    void createFactoryPresets();

    File folder;
};
