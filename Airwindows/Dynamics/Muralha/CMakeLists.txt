forja_add_plugin(Muralha "Muralha" Murl com.forja.muralha ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Dynamics" "Forja - Dinamica")

target_sources(Muralha PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
