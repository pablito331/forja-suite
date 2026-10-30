forja_add_plugin(Folego "Folego" Fole com.forja.folego ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Reverb" "Forja \\xC2\\xB7 Espa\\xC3\\xA7o")

target_sources(Folego PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
