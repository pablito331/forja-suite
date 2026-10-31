forja_add_plugin(Deluxe "Deluxe" Delx com.forja.deluxe ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Amp")

target_sources(Deluxe PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
