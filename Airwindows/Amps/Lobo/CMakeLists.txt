forja_add_plugin(Lobo "Lobo" Lobo com.forja.lobo ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Amp")

target_sources(Lobo PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
