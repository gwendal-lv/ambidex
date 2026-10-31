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
)

# Dexed's PluginProcessor.cpp defines createPluginFilter(), the function JUCE
# calls to create the app's processor. Rename it for this target, so that
# Ambidex can provide its own (in AmbidexProcessor.cpp).
set_source_files_properties(${PROJECT_SOURCE_DIR}/source/PluginProcessor.cpp PROPERTIES
    COMPILE_DEFINITIONS createPluginFilter=createDexedPluginFilter
)

target_link_libraries(Ambidex
PRIVATE
    dexed
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
)
