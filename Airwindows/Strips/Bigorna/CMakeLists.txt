forja_add_plugin(Bigorna "Bigorna" Bigo com.forja.bigorna ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Dynamics|EQ" "Forja - Channel Strip")

target_sources(Bigorna PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
