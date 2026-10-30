forja_add_plugin(Lima "Lima" Lima com.forja.lima ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|EQ" "Forja \\xC2\\xB7 EQ")

target_sources(Lima PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
