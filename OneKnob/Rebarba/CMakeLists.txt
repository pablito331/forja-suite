forja_add_plugin(Rebarba "Rebarba" Reba com.forja.rebarba ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Filter" "Forja \\xC2\\xB7 Mix")

target_sources(Rebarba PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
