forja_add_plugin(Malho "Malho" Malh com.forja.malho ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Dynamics" "Forja \\xC2\\xB7 Din\\xC3\\xA2mica")

target_sources(Malho PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
