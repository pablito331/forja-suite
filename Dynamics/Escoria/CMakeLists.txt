forja_add_plugin(Escoria "Escoria" Esco com.forja.escoria ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Dynamics" "Forja \\xC2\\xB7 Din\\xC3\\xA2mica")

target_sources(Escoria PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
