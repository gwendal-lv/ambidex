# Ambidex: a standalone app running two Dexed engines (A and B) with a
# crossfader between them. Dexed's own sources (source/) are compiled into
# this target unchanged.

juce_add_plugin(Ambidex
        VERSION "0.1.0"
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

# Reuse Dexed's source list as-is, so that upstream changes are picked up.
# Its paths are relative to source/, make them absolute.
get_target_property(DEXED_RELATIVE_SOURCES Dexed SOURCES)
get_target_property(DEXED_SOURCE_DIR Dexed SOURCE_DIR)
set(DEXED_SOURCES "")
foreach(DEXED_SOURCE ${DEXED_RELATIVE_SOURCES})
    get_filename_component(DEXED_SOURCE ${DEXED_SOURCE} ABSOLUTE BASE_DIR ${DEXED_SOURCE_DIR})
    list(APPEND DEXED_SOURCES ${DEXED_SOURCE})
endforeach()

target_sources(Ambidex PRIVATE
    AmbidexProcessor.cpp
    AmbidexEditor.cpp
    ${DEXED_SOURCES}
)

# Dexed's PluginProcessor.cpp defines createPluginFilter(), the function JUCE
# calls to create the app's processor. Rename it for this target only, so that
# Ambidex can provide its own (in AmbidexProcessor.cpp).
set_source_files_properties(${PROJECT_SOURCE_DIR}/source/PluginProcessor.cpp PROPERTIES
    COMPILE_DEFINITIONS createPluginFilter=createDexedPluginFilter
)

target_include_directories(Ambidex PRIVATE
    ${PROJECT_SOURCE_DIR}/source
    ${PROJECT_SOURCE_DIR}/libs/MTS-ESP/Client
)

# Same configuration as the Dexed target (see source/CMakeLists.txt)
if(DEXED_NO_ALSA)
    set(DEXED_ALSA FALSE)
else()
    set(DEXED_ALSA TRUE)
endif()

find_package(Jack QUIET)

target_compile_definitions(Ambidex PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_JACK=$<IF:$<BOOL:${Jack_FOUND}>,1,0>
    JUCE_ALSA=$<IF:$<BOOL:${DEXED_ALSA}>,1,0>
    JUCE_MODAL_LOOPS_PERMITTED=0 # needed for FileBrowser in CartManager
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(Ambidex
PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    surgesynthteam_tuningui
    tuning-library
    clap_juce_extensions
    DexedResources
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
)
