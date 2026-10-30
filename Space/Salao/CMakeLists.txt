forja_add_plugin(Salao "Salao" Sala com.forja.salao ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Reverb" "Forja \\xC2\\xB7 Espa\\xC3\\xA7o")

target_sources(Salao PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
