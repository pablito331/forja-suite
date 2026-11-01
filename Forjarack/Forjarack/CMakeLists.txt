forja_add_plugin(Forjarack "ForjaRack" Fjrk com.forja.rack ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx" "Forja - Rack")

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

    Source/ui/RackKnob.h
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

forja_link_soundfont_vendor(Forjarack)
