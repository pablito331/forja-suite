forja_add_plugin(Cinzel "Cinzel" Cinz com.forja.cinzel ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|EQ" "Forja \\xC2\\xB7 EQ")

target_sources(Cinzel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
