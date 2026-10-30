forja_add_plugin(@@PREFIX@@ "@@PRODUCT@@" @@CODE@@ @@BUNDLE@@ ${CMAKE_CURRENT_SOURCE_DIR}/Source "@@CATEGORIES@@" "@@COMPANY@@")

target_sources(@@PREFIX@@ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h
)
