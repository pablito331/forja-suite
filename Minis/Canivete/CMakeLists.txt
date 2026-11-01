forja_add_instrument(Canivete "Canivete" CnvS com.forja.canivete ${CMAKE_CURRENT_SOURCE_DIR}/Source "Instrument|Sampler" "Forja - Minis")

# Player SoundFont proprio (base Minis): sem vendor Airwindows e sem paridade.
# O unico TU com a implementacao do TinySoundFont e canivete/SoundFontEngine.cpp
# (define TSF_IMPLEMENTATION); o vendor nunca entra em target_sources, so no
# include path via forja_link_soundfont_vendor (que tambem acende o SF3).
target_sources(Canivete PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h

    Source/canivete/SoundFontEngine.h
    Source/canivete/SoundFontEngine.cpp
    Source/canivete/BankLibrary.h
)

forja_link_soundfont_vendor(Canivete)
