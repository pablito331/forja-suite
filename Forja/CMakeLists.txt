add_library(forja_ui STATIC
    Source/forja/ui/ForjaLookAndFeel.cpp
    Source/forja/ui/ForjaMeter.cpp
    Source/forja/ui/ForjaTopBar.cpp
    Source/forja/ui/ForjaStatusBar.cpp
    Source/forja/ui/ForjaGainReductionMeter.cpp
    Source/forja/ui/ForjaGRHistory.cpp
    Source/forja/ui/ForjaInfoPanel.cpp
    Source/forja/ui/ForjaEqCurve.cpp
    Source/forja/ui/ForjaChoice.cpp
    Source/forja/ui/ForjaTapeArt.cpp
    Source/forja/ui/ForjaEditorBase.cpp

    Source/forja/ui/ForjaTheme.h
    Source/forja/ui/ForjaCard.h
    Source/forja/ui/ForjaKnob.h
    Source/forja/ui/ForjaLookAndFeel.h
    Source/forja/ui/ForjaMeter.h
    Source/forja/ui/ForjaTopBar.h
    Source/forja/ui/ForjaStatusBar.h
    Source/forja/ui/ForjaGainReductionMeter.h
    Source/forja/ui/ForjaGRHistory.h
    Source/forja/ui/ForjaInfoContent.h
    Source/forja/ui/ForjaInfoPanel.h
    Source/forja/ui/ForjaEqCurve.h
    Source/forja/ui/ForjaEqResponse.h
    Source/forja/ui/ForjaChoice.h
    Source/forja/ui/ForjaTapeArt.h
    Source/forja/ui/ForjaEditorBase.h
    Source/forja/ui/ForjaCanvas.h
)

target_include_directories(forja_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(forja_ui PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_compile_options(forja_ui PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_link_libraries(forja_ui PUBLIC
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
)
