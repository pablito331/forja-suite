forja_add_plugin(Repique "Repique" Repi com.forja.repique ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Delay" "Forja \\xC2\\xB7 Espa\\xC3\\xA7o")

target_sources(Repique PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
