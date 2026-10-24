cmake_minimum_required(VERSION 3.24)

project(Pianolume VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# FluidSynth（LGPL-2.1）は公式の Windows 用ビルドを DLL として動的リンクする
if(NOT WIN32)
    message(FATAL_ERROR "FluidSynth の取得は現在 Windows のみ対応")
endif()

FetchContent_Declare(
    fluidsynth
    URL           https://github.com/FluidSynth/fluidsynth/releases/download/v2.6.1/fluidsynth-v2.6.1-win10-x64-cpp11.zip
    URL_HASH      SHA256=fab7a2e4b85675b66970f97a39bbc239729c5e0f237198b5922a6a73cbc8677c
    SOURCE_SUBDIR _no_cmake_
)
FetchContent_MakeAvailable(fluidsynth)

add_library(fluidsynth SHARED IMPORTED)
set_target_properties(fluidsynth PROPERTIES
    IMPORTED_LOCATION             "${fluidsynth_SOURCE_DIR}/bin/libfluidsynth-3.dll"
    IMPORTED_IMPLIB               "${fluidsynth_SOURCE_DIR}/lib/libfluidsynth-3.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${fluidsynth_SOURCE_DIR}/include"
)

set(FLUIDSYNTH_RUNTIME_DLLS
    "${fluidsynth_SOURCE_DIR}/bin/libfluidsynth-3.dll"
    "${fluidsynth_SOURCE_DIR}/bin/sndfile.dll"
)

juce_add_gui_app(Pianolume
    PRODUCT_NAME "Pianolume"
    COMPANY_NAME "Pianolume"
)

juce_add_binary_data(PianolumeBinaryData
    NAMESPACE PianolumeBinaryData
    SOURCES
        Resources/Fonts/Inter-Regular.ttf
        Resources/Fonts/Inter-Medium.ttf
        Resources/Fonts/Inter-SemiBold.ttf
        Resources/Fonts/Inter-Bold.ttf
        Resources/Fonts/JetBrainsMono-Regular.ttf
)

target_sources(Pianolume PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/Audio/AudioEngine.cpp
    Source/Audio/SoundFontSynth.cpp
    Source/Keyboard/KeyboardLayout.cpp
    Source/Midi/MidiInputManager.cpp
    Source/Projection/CalibrationModel.cpp
    Source/Projection/Homography.cpp
    Source/Projection/ProjectionCalibration.cpp
    Source/Projection/ProjectionView.cpp
    Source/Projection/ProjectionWindow.cpp
    Source/Song/Song.cpp
    Source/Platform/WindowsTitleBar.cpp
    Source/Song/SongPlayer.cpp
    Source/UI/CalibrationEditor.cpp
    Source/UI/HeaderBar.cpp
    Source/UI/Icons.cpp
    Source/UI/LogPanel.cpp
    Source/UI/PartsPanel.cpp
    Source/UI/PianolumeLookAndFeel.cpp
    Source/UI/SettingsPanel.cpp
    Source/UI/Theme.cpp
    Source/UI/ToolButton.cpp
    Source/UI/TransportBar.cpp
    Source/View/FallingNotesView.cpp
    Source/View/NotesRenderer.cpp
)

target_include_directories(Pianolume PRIVATE Source)

target_compile_definitions(Pianolume PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Pianolume,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Pianolume,JUCE_VERSION>"
)

if(MSVC)
    # ソース中の日本語コメントを正しく扱うため
    target_compile_options(Pianolume PRIVATE /utf-8)
endif()

target_link_libraries(Pianolume
    PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
        fluidsynth
        PianolumeBinaryData
        dwmapi
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# 実行ファイルの隣に FluidSynth の DLL を置く
add_custom_command(TARGET Pianolume POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FLUIDSYNTH_RUNTIME_DLLS} "$<TARGET_FILE_DIR:Pianolume>"
)
