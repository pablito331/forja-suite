forja_add_instrument(Drumforge "Drumforge" Drfg com.forja.drumforge ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_sources(Drumforge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h

    Source/engine/DrumEngine.cpp
    Source/engine/SampleStore.cpp

    Source/engine/ChannelStrip.h
    Source/engine/Compressor.h
    Source/engine/DrumEngine.h
    Source/engine/Filters.h
    Source/engine/Kit.h
    Source/engine/Mapping.h
    Source/engine/MasterBus.h
    Source/engine/SampleStore.h
    Source/engine/VoicePool.h

    Source/loops/LoopLibrary.h
    Source/loops/LoopPlayer.h

    Source/ui/KitPanel.cpp
    Source/ui/LoopsPanel.cpp
    Source/ui/MixerPanel.cpp

    Source/ui/KitPanel.h
    Source/ui/LoopsPanel.h
    Source/ui/MiniKnob.h
    Source/ui/MixerPanel.h
)
