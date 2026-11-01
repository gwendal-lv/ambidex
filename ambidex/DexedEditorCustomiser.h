// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include <JuceHeader.h>

/**
 * Adapts the global panel of a Dexed editor to Ambidex, without changing
 * Dexed's code: hides the logo and the INIT, STORE and program selector
 * controls, turns CART into "Load FM patch" and PARM into "FM Engine
 * Settings", and shows the name of the FM patch (the DX7 voice) instead of the
 * program list. Must be deleted before the Dexed editor.
 */
class DexedEditorCustomiser {
public:
    DexedEditorCustomiser(Component& dexedEditor, std::function<void(Component& button)> onLoadFmPatch);

    void setFmPatchName(const String& name);

private:
    struct FmPatchDisplay : public Component {
        String text;
        void paint(Graphics& g) override;
    };

    struct LogoCover : public Component {
        void paint(Graphics& g) override;
    };

    LogoCover logoCover;
    FmPatchDisplay fmPatchDisplay;
};
