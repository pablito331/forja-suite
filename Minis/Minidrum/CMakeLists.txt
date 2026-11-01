forja_add_instrument(Minidrum "Minidrum" Mndr com.forja.minidrum ${CMAKE_CURRENT_SOURCE_DIR}/Source "" "Forja - Minis")

# O engine e o mesmissimo do Drumforge, compilado por caminho (padrao dos
# harnesses de teste). O mini nao duplica nem forkea DSP: reutiliza o bloco.
target_sources(Minidrum PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h

    Source/minidrum/MiniDrum.h
    Source/minidrum/MiniDrum.cpp
    Source/minidrum/KitLibrary.h

    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source/engine/DrumEngine.cpp
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source/engine/SampleStore.cpp
)

target_include_directories(Minidrum PRIVATE
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source
)
