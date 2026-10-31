forja_add_plugin(Cera "Cera" Cera com.forja.cera ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Mix")

target_sources(Cera PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
