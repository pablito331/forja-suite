forja_add_plugin(Corpo "Corpo" Corp com.forja.corpo ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Mix")

target_sources(Corpo PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
