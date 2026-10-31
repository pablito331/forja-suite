forja_add_plugin(Gabinete "Gabinete" Cabs com.forja.gabinete ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Filter" "Forja - Amp")

target_sources(Gabinete PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
