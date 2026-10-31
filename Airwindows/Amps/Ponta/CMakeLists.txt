forja_add_plugin(Ponta "Ponta" Pont com.forja.ponta ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Amp")

target_sources(Ponta PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
