forja_add_plugin(Crick "Crick" Cric com.forja.crick ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Amp")

target_sources(Crick PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
