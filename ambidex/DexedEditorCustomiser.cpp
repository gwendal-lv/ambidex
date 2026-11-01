// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "DexedEditorCustomiser.h"

namespace {
// Colours of Dexed's global panel background (GlobalEditor_1728x288.png)
const Colour panelColour { 0xff4e4332 };
const Colour displayColour { 0xff141212 };

Component* findChild(Component& root, const std::function<bool(Component&)>& matches) {
    for (auto* child : root.getChildren()) {
        if (matches(*child))
            return child;
        if (auto* found = findChild(*child, matches))
            return found;
    }
    return nullptr;
}

Component* findChildNamed(Component& root, const String& name) {
    return findChild(root, [&name](Component& c) { return c.getName() == name; });
}

Component* findChildTitled(Component& root, const String& title) {
    return findChild(root, [&title](Component& c) { return c.getTitle() == title; });
}

void hide(Component* component) {
    if (component != nullptr)
        component->setVisible(false);
}
} // namespace

//==============================================================================
void DexedEditorCustomiser::FmPatchDisplay::paint(Graphics& g) {
    g.setColour(displayColour);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.0f);

    auto area = getLocalBounds().reduced(8, 0);
    g.setFont(FontOptions(14.0f));
    const String caption("FM patch: ");
    g.setColour(Colours::white.withAlpha(0.5f));
    g.drawText(caption, area, Justification::centredLeft);
    g.setColour(Colours::white);
    g.drawText(text, area.withTrimmedLeft(GlyphArrangement::getStringWidthInt(g.getCurrentFont(), caption)),
               Justification::centredLeft);
}

void DexedEditorCustomiser::LogoCover::paint(Graphics& g) {
    g.fillAll(panelColour);
}

//==============================================================================
DexedEditorCustomiser::DexedEditorCustomiser(Component& dexedEditor, std::function<void(Component&)> onLoadFmPatch) {
    auto* cartButton = dynamic_cast<TextButton*>(findChildNamed(dexedEditor, "cartButton"));
    auto* globalPanel = cartButton != nullptr ? cartButton->getParentComponent() : nullptr;
    jassert(globalPanel != nullptr); // (has Dexed's GlobalEditor changed?)
    if (globalPanel == nullptr)
        return;

    hide(findChildNamed(*globalPanel, "initButton"));
    hide(findChildNamed(*globalPanel, "storeButton"));
    hide(findChildTitled(*globalPanel, "Program selector"));
    hide(findChildTitled(*globalPanel, "About DEXED")); // (the logo's button)

    // The logo is part of the panel's background image
    globalPanel->addAndMakeVisible(logoCover);
    logoCover.setBounds(6, 6, 142, 74);
    logoCover.toBack();

    // Both buttons, stacked where the logo was (coordinates of Dexed's panel)
    cartButton->setButtonText("Load FM patch");
    cartButton->setTitle("Load FM patch");
    cartButton->setBounds(6, 9, 141, 30);
    cartButton->onClick = [cartButton, onLoadFmPatch] { onLoadFmPatch(*cartButton); };

    if (auto* parmButton = dynamic_cast<TextButton*>(findChildNamed(*globalPanel, "parmButton"))) {
        parmButton->setButtonText("FM Engine Settings");
        parmButton->setTitle("FM Engine Settings");
        parmButton->setBounds(6, 45, 141, 30);
    }

    // Where the CART, PARM and INIT buttons, the program selector and STORE were
    globalPanel->addAndMakeVisible(fmPatchDisplay);
    fmPatchDisplay.setBounds(6, 112, 314, 26);
}

void DexedEditorCustomiser::setFmPatchName(const String& name) {
    if (fmPatchDisplay.text != name) {
        fmPatchDisplay.text = name;
        fmPatchDisplay.repaint();
    }
}
