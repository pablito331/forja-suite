forja_add_plugin(Mel "Mel" Mell com.forja.mel ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Filter" "Forja - Mix")

target_sources(Mel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
