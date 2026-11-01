# Benchmark tool: renders Ambidex headless (no audio device, no UI) and
# checks the NEON kernels against the original scalar code. See bench/README.md.

add_executable(ambidex-bench
    AmbidexBench.cpp
    KernelBench.cpp
)

# Uses the app's shared code (Dexed engines, AmbidexAudioProcessor, JUCE), with
# the same compile definitions and include paths
target_link_libraries(ambidex-bench PRIVATE Ambidex)
target_compile_definitions(ambidex-bench PRIVATE $<TARGET_PROPERTY:Ambidex,COMPILE_DEFINITIONS>)
target_include_directories(ambidex-bench PRIVATE
    $<TARGET_PROPERTY:Ambidex,INCLUDE_DIRECTORIES>
    ${PROJECT_SOURCE_DIR}/ambidex
)
target_compile_features(ambidex-bench PRIVATE cxx_std_20)
