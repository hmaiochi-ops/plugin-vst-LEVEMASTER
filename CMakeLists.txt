cmake_minimum_required(VERSION 3.22)

# Windows: runtime estático (não exige instalar o Visual C++ Redistributable)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")
# macOS: binário universal (Apple Silicon + Intel)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

project(LeveMaster VERSION 0.1.0)

option(LEVE_COPY_AFTER_BUILD "Copiar o VST3 para a pasta de plugins do sistema após compilar" OFF)
set(LEVE_JUCE_PATH "" CACHE PATH "Caminho de um checkout local do JUCE (opcional)")

if(LEVE_JUCE_PATH)
    add_subdirectory(${LEVE_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(LeveMaster
    COMPANY_NAME "Henrique"
    PLUGIN_MANUFACTURER_CODE Hnrq
    PLUGIN_CODE LvMs
    FORMATS VST3
    PRODUCT_NAME "LeveMaster"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD ${LEVE_COPY_AFTER_BUILD})

juce_generate_juce_header(LeveMaster)

target_sources(LeveMaster PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_features(LeveMaster PRIVATE cxx_std_17)

target_compile_definitions(LeveMaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(LeveMaster
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
