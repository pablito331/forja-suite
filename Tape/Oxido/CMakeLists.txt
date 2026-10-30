forja_add_plugin(Oxido "Oxido" Oxid com.forja.oxido ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja \\xC2\\xB7 Fita")

target_sources(Oxido PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
