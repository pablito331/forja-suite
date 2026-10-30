forja_add_plugin(Torno "Torno" Torn com.forja.torno ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|EQ" "Forja \\xC2\\xB7 EQ")

target_sources(Torno PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
