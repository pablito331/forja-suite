forja_add_plugin(Eco "Eco" Ecoj com.forja.eco ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Delay" "Forja - Espaco")

target_sources(Eco PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
