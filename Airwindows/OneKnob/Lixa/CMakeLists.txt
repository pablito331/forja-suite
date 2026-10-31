forja_add_plugin(Lixa "Lixa" Lixa com.forja.lixa ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Mix")

target_sources(Lixa PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
