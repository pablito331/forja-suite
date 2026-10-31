forja_add_plugin(Cadinho "Cadinho" Cadi com.forja.cadinho ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Dynamics|EQ" "Forja - Channel Strip")

target_sources(Cadinho PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
