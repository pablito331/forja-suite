forja_add_plugin(Chime "Chime" Chim com.forja.chime ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Amp")

target_sources(Chime PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
