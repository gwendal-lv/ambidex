// SPDX-License-Identifier: GPL-3.0-or-later
// Ambidex - two Dexed engines with a morphing crossfader.

#include "PresetLibrary.h"
#include "core/Cartridge.h"

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

//==============================================================================
// Factory presets: voices from Dexed's built-in cartridges, with FX that show
// what the chain (and its morphing) can do. Values not listed are the defaults
// (i.e. all FX bypassed).
struct FactoryPreset {
    const char* name;
    const char* cartridge; // in Dexed's built-in cartridges
    int voiceIndex;
    std::vector<std::pair<fx::Param, float>> fxValues;
};

const std::vector<FactoryPreset>& getFactoryPresets() {
    using namespace fx;
    static const std::vector<FactoryPreset> presets {
        { "Laurie", "Dexed_01.syx", 1, {} },
        { "Pharoh", "Dexed_01.syx", 3, {} },
        { "Chorus Rhodes", "SynprezFM_02.syx", 15,
          { { modulationRate, 0.6f }, { modulationDepth, 0.5f }, { modulationMix, 0.5f },
            { reverbSize, 0.5f }, { reverbMix, 0.2f } } },
        { "Driven Bass", "SynprezFM_03.syx", 17,
          { { distortionDrive, 18.0f }, { distortionShape, 0.2f }, { distortionTone, 5000.0f }, { distortionMix, 0.6f },
            { filterCutoff, 1500.0f }, { filterResonance, 1.5f }, { filterMix, 1.0f },
            { compressorThreshold, -24.0f }, { compressorRatio, 4.0f }, { compressorAttack, 5.0f },
            { compressorRelease, 120.0f }, { compressorMix, 1.0f } } },
        { "Space Bells", "SynprezFM_14.syx", 7,
          { { delayTime, 420.0f }, { delayFeedback, 0.45f }, { delayTone, 4000.0f }, { delaySpread, 1.0f }, { delayMix, 0.35f },
            { reverbSize, 0.85f }, { reverbDamping, 0.4f }, { reverbMix, 0.4f } } },
        { "Phased Strings", "SynprezFM_10.syx", 1,
          { { modulationRate, 0.2f }, { modulationDepth, 0.7f }, { modulationFeedback, 0.5f },
            { modulationCharacter, 1.0f }, { modulationMix, 0.5f },
            { reverbSize, 0.7f }, { reverbMix, 0.3f } } },
        { "Flanged Clav", "SynprezFM_21.syx", 22,
          { { modulationRate, 0.3f }, { modulationDepth, 0.6f }, { modulationFeedback, 0.6f },
            { modulationCharacter, 0.5f }, { modulationMix, 0.5f },
            { delayTime, 180.0f }, { delayFeedback, 0.2f }, { delayMix, 0.2f } } },
        { "Filtered Pad", "SynprezFM_12.syx", 5,
          { { filterCutoff, 600.0f }, { filterResonance, 4.0f }, { filterMix, 1.0f },
            { modulationRate, 0.15f }, { modulationDepth, 0.4f }, { modulationMix, 0.4f },
            { reverbSize, 0.8f }, { reverbMix, 0.35f } } },
    };
    return presets;
}

bool loadBuiltInVoice(const String& cartridgeName, int voiceIndex, std::array<uint8_t, AmbidexPreset::voiceSize>& voice) {
    MemoryInputStream zipStream(BinaryData::builtin_pgm_zip, BinaryData::builtin_pgm_zipSize, false);
    ZipFile zip(zipStream);
    std::unique_ptr<InputStream> stream(zip.createStreamForEntry(zip.getIndexOfFileName(cartridgeName)));
    Cartridge cartridge;
    if (stream == nullptr || cartridge.load(*stream) == -1)
        return false;

    const Program program = cartridge.unpackProgram(voiceIndex);
    std::copy_n(program.getRawData(), voice.size(), voice.begin());
    return true;
}
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
    for (const auto& factoryPreset : getFactoryPresets()) {
        AmbidexPreset preset;
        preset.name = factoryPreset.name;
        if (!loadBuiltInVoice(factoryPreset.cartridge, factoryPreset.voiceIndex, preset.voice))
            continue;

        for (int param = 0; param < fx::numParams; ++param)
            preset.fxValues[(size_t) param] = fx::getParamInfo(param).defaultValue;
        for (const auto& [param, value] : factoryPreset.fxValues)
            preset.fxValues[(size_t) param] = value;

        save(preset);
    }
}
