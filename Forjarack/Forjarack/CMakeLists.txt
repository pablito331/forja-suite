# O rack e hibrido: cadeia de FX (entrada de audio) E gavetas-instrumento (MIDI
# do teclado). FORJA_WANTS_MIDI_INPUT liga a macro JucePlugin_WantsMidiInput do
# wrapper VST3, que anuncia a fila de eventos e converte as notas em MidiBuffer;
# sem ela o processBlock recebe MIDI vazio no VST3 e os slots-instrumento ficam
# mudos. A categoria soma Instrument ao Fx de sempre para as DAWs que filtram
# instrumento de efeito no navegador. O JUCE espera lista CMake (";") e monta o
# "|" sozinho: passar "Instrument|Fx" sai duplicado ("Fx|Instrument|Fx").
set(FORJA_WANTS_MIDI_INPUT TRUE)
forja_add_plugin(Forjarack "ForjaRack" Fjrk com.forja.rack ${CMAKE_CURRENT_SOURCE_DIR}/Source "Instrument;Fx" "Forja - Rack")
unset(FORJA_WANTS_MIDI_INPUT)

target_sources(Forjarack PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h

    Source/rack/RackState.h
    Source/rack/Module.h
    Source/rack/ModuleCatalog.h
    Source/rack/ModuleCatalog.cpp
    Source/rack/RackEngine.h
    Source/rack/RackEngine.cpp

    # Gaveta hospedada: carrega plugins de Documents/Forja/Rack em runtime
    # (efeito + instrumento) via AudioPluginFormatManager. Puxa JUCE de
    # processors; fica fora do core headless do rack (teste engine nao linka).
    Source/rack/HostedModule.h
    Source/rack/HostedModule.cpp
    Source/rack/HostedLibrary.h
    Source/rack/HostedLibrary.cpp

    Source/ui/RackKnob.h
    Source/ui/HostedSlot.h
    Source/ui/SlotComponent.h
    Source/ui/SlotComponent.cpp

    # Gaveta instrument MINIDRUM: mesmo wrapper do plugin standalone e o
    # engine do Drumforge compilado por caminho (padrao dos testes).
    ${CMAKE_SOURCE_DIR}/Minis/Minidrum/Source/minidrum/MiniDrumRackModule.h
    ${CMAKE_SOURCE_DIR}/Minis/Minidrum/Source/minidrum/MiniDrumRackModule.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Minidrum/Source/minidrum/MiniDrum.h
    ${CMAKE_SOURCE_DIR}/Minis/Minidrum/Source/minidrum/MiniDrum.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Minidrum/Source/minidrum/KitLibrary.h
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source/engine/DrumEngine.cpp
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source/engine/SampleStore.cpp

    # Gaveta drive MINISAT: mesmo wrapper do plugin standalone, adaptador
    # forja::BrasaDSP compartilhado via include de forja_ui.
    ${CMAKE_SOURCE_DIR}/Minis/Minisat/Source/minisat/MiniSatRackModule.h
    ${CMAKE_SOURCE_DIR}/Minis/Minisat/Source/minisat/MiniSatRackModule.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Minisat/Source/minisat/MiniSat.h
    ${CMAKE_SOURCE_DIR}/Minis/Minisat/Source/minisat/MiniSat.cpp

    # Gaveta instrument CANIVETE: mesmo player SoundFont do plugin standalone,
    # compilado por caminho. SoundFontEngine.cpp e o unico TU com o TSF.
    ${CMAKE_SOURCE_DIR}/Minis/Canivete/Source/canivete/CaniveteRackModule.h
    ${CMAKE_SOURCE_DIR}/Minis/Canivete/Source/canivete/CaniveteRackModule.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Canivete/Source/canivete/SoundFontEngine.h
    ${CMAKE_SOURCE_DIR}/Minis/Canivete/Source/canivete/SoundFontEngine.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Canivete/Source/canivete/BankLibrary.h
)

target_include_directories(Forjarack PRIVATE
    ${CMAKE_SOURCE_DIR}/Minis/Minidrum/Source
    ${CMAKE_SOURCE_DIR}/Minis/Minisat/Source
    ${CMAKE_SOURCE_DIR}/Minis/Canivete/Source
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source
)

# Hosting de plugins de terceiros nas gavetas HOSPEDADO. Liga os formatos que o
# AudioPluginFormatManager reconhece (VST3 em todo SO; AU no macOS). Sem isto o
# scan da pasta nao acha nada e a Gaveta fica muda.
target_compile_definitions(Forjarack PRIVATE
    JUCE_PLUGINHOST_VST3=1
    $<$<PLATFORM_ID:Darwin>:JUCE_PLUGINHOST_AU=1>
)

forja_link_soundfont_vendor(Forjarack)
