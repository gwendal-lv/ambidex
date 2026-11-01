// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#pragma once

#include "FxPanel.h"

/**
 * The plot of an FX module: a picture of what the FX does, calculated from the
 * values shown by the module's knobs (so it follows the edits and the morph),
 * and from the live levels for some of them. See doc/Effects.md.
 */
class FxPlot : public Component {
public:
    FxPlot(const FxModule& module, const FxLiveData& liveData) : module(module), live(liveData) {}

    void paint(Graphics& g) override;

    /** The plot of a module. */
    static std::unique_ptr<FxPlot> create(int module, const FxModule& owner, const FxLiveData& liveData);

protected:
    virtual void paintPlot(Graphics& g, Rectangle<float> area) = 0;

    float get(int param) const { return module.getValue(param); }

    const FxModule& module;
    const FxLiveData& live;
};
