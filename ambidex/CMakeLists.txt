# Ambidex: a standalone app running two Dexed engines (A and B) with a
# crossfader between them.

juce_add_plugin(Ambidex
        VERSION ${PROJECT_VERSION}
        ICON_BIG "../assets/ui/dexedIcon.png"
        ICON_SMALL "../assets/ui/dexedIcon.png"
        COMPANY_NAME "Ambidex"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        PLUGIN_MANUFACTURER_CODE Ambx
        PLUGIN_CODE Ambx
        FORMATS Standalone
        PRODUCT_NAME "Ambidex"
        DESCRIPTION "Two Dexed engines with a morphing crossfader"
        BUNDLE_ID "com.ambidex.Ambidex"
)

juce_generate_juce_header(Ambidex)

target_sources(Ambidex PRIVATE
    AmbidexProcessor.cpp
    AmbidexEditor.cpp
    DexedEditorCustomiser.cpp
    FxChain.cpp
    FxPanel.cpp
    FxParameters.cpp
    FxPlots.cpp
    PresetLibrary.cpp
)

# Dexed's PluginProcessor.cpp defines createPluginFilter(), the function JUCE
# calls to create the app's processor. Rename it for this target, so that
# Ambidex can provide its own (in AmbidexProcessor.cpp).
set_source_files_properties(${PROJECT_SOURCE_DIR}/source/PluginProcessor.cpp PROPERTIES
    COMPILE_DEFINITIONS createPluginFilter=createDexedPluginFilter
)

# Factory presets: copied into the user's preset library on first launch (see
# scripts/update-factory-presets.sh to update them from a local library)
file(GLOB AMBIDEX_FACTORY_PRESETS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/presets/*.ambidexpreset)
juce_add_binary_data(AmbidexFactoryPresets
    HEADER_NAME AmbidexFactoryPresets.h
    NAMESPACE AmbidexFactoryPresets
    SOURCES ${AMBIDEX_FACTORY_PRESETS}
)

target_link_libraries(Ambidex
PRIVATE
    dexed
    AmbidexFactoryPresets
    juce::juce_dsp
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
)
