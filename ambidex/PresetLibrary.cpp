// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "PresetLibrary.h"
#include "AmbidexFactoryPresets.h"

namespace {
const Identifier presetId("ambidexPreset");
const Identifier formatVersionId("formatVersion");
const Identifier nameId("name");
const Identifier fmId("fm");
const Identifier voiceId("voice");
const Identifier engineStateId("engineState");
const Identifier fxId("fx");

constexpr int formatVersion = 1;
const String fileExtension(".ambidexpreset");
} // namespace

//==============================================================================
ValueTree AmbidexPreset::toValueTree() const {
    ValueTree tree(presetId);
    tree.setProperty(formatVersionId, formatVersion, nullptr);
    tree.setProperty(nameId, name, nullptr);

    ValueTree fm(fmId);
    fm.setProperty(voiceId, MemoryBlock(voice.data(), voice.size()).toBase64Encoding(), nullptr);
    fm.setProperty(engineStateId, engineState.toBase64Encoding(), nullptr);
    tree.appendChild(fm, nullptr);

    ValueTree fxTree(fxId);
    for (int param = 0; param < fx::numParams; ++param)
        fxTree.setProperty(fx::getParamInfo(param).id, fxValues[(size_t) param], nullptr);
    tree.appendChild(fxTree, nullptr);

    return tree;
}

bool AmbidexPreset::fromValueTree(const ValueTree& tree) {
    MemoryBlock voiceData;
    const auto fm = tree.getChildWithName(fmId);
    if (!tree.hasType(presetId) || !voiceData.fromBase64Encoding(fm[voiceId].toString())
        || voiceData.getSize() != voice.size())
        return false;

    name = tree[nameId].toString();
    std::copy_n(static_cast<const uint8_t*>(voiceData.getData()), voice.size(), voice.begin());
    engineState.reset();
    engineState.fromBase64Encoding(fm[engineStateId].toString());

    const auto fxTree = tree.getChildWithName(fxId);
    for (int param = 0; param < fx::numParams; ++param) {
        const auto& info = fx::getParamInfo(param);
        fxValues[(size_t) param] = (float) fxTree.getProperty(info.id, info.defaultValue);
    }
    return true;
}

//==============================================================================
PresetLibrary::PresetLibrary(const File& presetFolder) : folder(presetFolder) {
    if (!folder.isDirectory()) {
        folder.createDirectory();
        createFactoryPresets();
    }
}

StringArray PresetLibrary::getNames() const {
    StringArray names;
    for (const auto& file : folder.findChildFiles(File::findFiles, false, "*" + fileExtension))
        names.add(file.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

bool PresetLibrary::contains(const String& name) const {
    return name.isNotEmpty() && getFile(name).existsAsFile();
}

bool PresetLibrary::load(const String& name, AmbidexPreset& preset) const {
    if (!contains(name))
        return false;

    auto xml = parseXML(getFile(name));
    if (xml == nullptr || !preset.fromValueTree(ValueTree::fromXml(*xml)))
        return false;

    preset.name = name; // (the file name is the reference)
    return true;
}

bool PresetLibrary::save(const AmbidexPreset& preset) {
    auto xml = preset.toValueTree().createXml();
    return preset.name.isNotEmpty() && xml != nullptr && xml->writeTo(getFile(preset.name));
}

bool PresetLibrary::remove(const String& name) {
    return contains(name) && getFile(name).deleteFile();
}

bool PresetLibrary::rename(const String& oldName, const String& newName) {
    AmbidexPreset preset;
    if (!load(oldName, preset) || contains(newName))
        return false;

    preset.name = newName;
    return save(preset) && remove(oldName);
}

String PresetLibrary::toValidName(const String& name) {
    return File::createLegalFileName(name.trim()).trim();
}

File PresetLibrary::getFile(const String& name) const {
    return folder.getChildFile(name + fileExtension);
}

void PresetLibrary::createFactoryPresets() {
    // The preset files of the repository's ambidex/presets folder, embedded in the app
    for (int i = 0; i < AmbidexFactoryPresets::namedResourceListSize; ++i) {
        const char* resourceName = AmbidexFactoryPresets::namedResourceList[i];
        int size = 0;
        const char* data = AmbidexFactoryPresets::getNamedResource(resourceName, size);
        const String fileName = AmbidexFactoryPresets::getNamedResourceOriginalFilename(resourceName);
        if (data != nullptr && fileName.endsWith(fileExtension))
            folder.getChildFile(fileName).replaceWithData(data, (size_t) size);
    }
}
