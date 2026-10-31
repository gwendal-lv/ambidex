# Dexed's engine and editor, as an INTERFACE library: its sources are compiled
# into the app target that links it (with that target's JUCE configuration).
add_library(dexed INTERFACE)

set(DEXED_SOURCES
	msfa/dx7note.cc
	msfa/env.cc
	msfa/exp2.cc
	msfa/fm_core.cc
	msfa/fm_op_kernel.cc
	msfa/freqlut.cc
	msfa/lfo.cc
	msfa/pitchenv.cc
	msfa/sin.cc
	msfa/tuning.cc
	msfa/porta.cpp

	PluginProcessor.cpp

	PluginData.cpp
	PluginFx.cpp
	Tuning.cpp

	core/Cartridge.cpp
	core/Program.cpp

    engine/EngineMkI.cpp
	engine/EngineOpl.cpp

    ui/PluginEditor.cpp
		ui/window/CartManager.cpp
	ui/DXLookNFeel.cpp
    ui/GlobalEditor.cpp
	ui/OperatorEditor.cpp
		ui/window/ParamDialog.cpp
		ui/window/TuningShow.cpp
	ui/component/AlgoDisplay.cpp
	ui/component/DXComponents.cpp
	ui/component/EnvDisplay.cpp
	ui/component/ProgramListBox.cpp
	ui/component/VUMeter.cpp

	midi/SysexComm.cpp
	midi/PluginMidi.cpp

	parameter/DexedApvts.cpp
	parameter/ParametersMapper.cpp
	parameter/State.cpp
    parameter/Preference.cpp

        debugger/ValueTreeDebugger.cpp
        debugger/MemoryLogger.h

    # until MTS-ESP has is own cmake project
    ../libs/MTS-ESP/Client/libMTSClient.cpp
)
# (interface sources are compiled in the linking target's directory)
list(TRANSFORM DEXED_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)

target_sources(dexed INTERFACE ${DEXED_SOURCES})

target_include_directories(dexed INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/libs/MTS-ESP/Client
)

if(DEXED_NO_ALSA)
    set(DEXED_ALSA FALSE)
    message(STATUS "Building Dexed without ALSA support")
else()
    set(DEXED_ALSA TRUE)
endif()

find_package(Jack QUIET)

target_compile_definitions(dexed INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_JACK=$<IF:$<BOOL:${Jack_FOUND}>,1,0>
    JUCE_ALSA=$<IF:$<BOOL:${DEXED_ALSA}>,1,0>
    JUCE_MODAL_LOOPS_PERMITTED=0 # needed for FileBrowser in CartManager
    JUCE_REPORT_APP_USAGE=0
    # JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1     # REVERT BACK SINCE THERE IS AN ISSUE WITH JUCE 7.0.0
)

target_link_libraries(dexed INTERFACE
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
    # DexedAudioProcessor implements clap_juce_extensions::clap_properties
    clap_juce_extensions
    DexedResources
)
