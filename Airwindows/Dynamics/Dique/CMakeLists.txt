forja_add_plugin(Dique "Dique" Dque com.forja.dique ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Dynamics" "Forja - Dinamica")

target_sources(Dique PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
