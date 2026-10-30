forja_add_plugin(Mel "Mel" Mel com.forja.mel ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Filter" "Forja \\xC2\\xB7 Mix")

target_sources(Mel PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
