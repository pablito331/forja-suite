forja_add_instrument(Pianoforge "Pianoforge" Pfng com.forja.pianoforge ${CMAKE_CURRENT_SOURCE_DIR}/Source "Instrument|Piano" "Forja - Pianos")

# Piano de amostras + modelo fisico (base Pianos). Sem vendor Airwindows e sem
# paridade MIT. O unico TU com a implementacao do TinySoundFont e
# engine/PianoEngine.cpp (define TSF_IMPLEMENTATION); o vendor nunca entra em
# target_sources, so no include path via forja_link_soundfont_vendor (que tambem
# acende o SF3). O TU proprio mantem o caminho GM do CANIVETE intacto.
target_sources(Pianoforge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h

    Source/ui/PianoLidArt.h

    Source/engine/Params.h
    Source/engine/PianoEngine.h
    Source/engine/PianoEngine.cpp
    Source/engine/Bundle.h
    Source/engine/Bundle.cpp

    Source/engine/dsp/Biquad.h
    Source/engine/dsp/Hammer.h
    Source/engine/dsp/Resonance.h
    Source/engine/dsp/Filters.h
    Source/engine/dsp/Space.h
    Source/engine/dsp/Master.h
)

forja_link_soundfont_vendor(Pianoforge)
