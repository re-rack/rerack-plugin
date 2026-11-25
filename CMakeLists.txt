cmake_minimum_required(VERSION 3.22)

project(GMajor2 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.6
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(JUCE)

set(GM2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libgmajor2" CACHE PATH "Path to the libgmajor2 source tree")

file(GLOB GM2_SOURCES CONFIGURE_DEPENDS "${GM2_DIR}/src/*.c")

if(NOT GM2_SOURCES)
    message(FATAL_ERROR "No libgmajor2 sources found under ${GM2_DIR}/src")
endif()

add_library(gm2 STATIC ${GM2_SOURCES})
target_include_directories(gm2 PUBLIC "${GM2_DIR}/src")
set_target_properties(gm2 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_STANDARD 17
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden)
target_compile_options(gm2 PRIVATE -Wall -Wextra -Wpedantic)

add_library(gm2_resampler STATIC Source/Resampler.cpp)
target_include_directories(gm2_resampler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set_target_properties(gm2_resampler PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
target_compile_options(gm2_resampler PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)

juce_add_plugin(GMajor2
    FORMATS VST3
    PRODUCT_NAME "GMajor2"
    COMPANY_NAME "tapir"
    PLUGIN_MANUFACTURER_CODE Tapr
    PLUGIN_CODE Gm2x
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE)

target_sources(GMajor2 PRIVATE
    Source/EffectCatalog.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp)

target_compile_definitions(GMajor2 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(GMajor2
    PRIVATE
        juce::juce_audio_utils
        gm2
        gm2_resampler
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

enable_testing()

add_executable(resampler_test
    tests/resampler_test.cpp
    Source/Resampler.cpp)

target_include_directories(resampler_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
set_target_properties(resampler_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
target_compile_options(resampler_test PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
add_test(NAME resampler_test COMMAND resampler_test)
