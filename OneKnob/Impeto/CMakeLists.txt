forja_add_plugin(Impeto "Impeto" Impe com.forja.impeto ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Dynamics" "Forja \\xC2\\xB7 Mix")

target_sources(Impeto PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
