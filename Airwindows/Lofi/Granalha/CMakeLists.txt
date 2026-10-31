forja_add_plugin(Granalha "Granalha" Gran com.forja.granalha ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Lo-Fi")

target_sources(Granalha PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
