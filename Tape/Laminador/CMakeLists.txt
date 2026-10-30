forja_add_plugin(Laminador "Laminador" Lami com.forja.laminador ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja \\xC2\\xB7 Fita")

target_sources(Laminador PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
