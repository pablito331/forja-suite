forja_add_plugin(Minisat "Minisat" Msat com.forja.minisat ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Minis")

# O MiniSat nao duplica DSP: reutiliza o adaptador forja::BrasaDSP (ja no
# include compartilhado Forja/Source) e, para o cartao de curva, o
# ForjaTransferCurve do BRASA compilado por caminho (padrao dos harnesses).
# MiniSatRackModule.cpp fica fora daqui: quem o compila e o Forjarack e o
# executavel de teste, ambos por caminho.
target_sources(Minisat PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h

    Source/minisat/MiniSat.h
    Source/minisat/MiniSat.cpp

    ${CMAKE_SOURCE_DIR}/Airwindows/Saturation/Brasa/Source/forja/ui/ForjaTransferCurve.cpp
    ${CMAKE_SOURCE_DIR}/Airwindows/Saturation/Brasa/Source/forja/ui/ForjaTransferCurve.h
)

target_include_directories(Minisat PRIVATE
    ${CMAKE_SOURCE_DIR}/Airwindows/Saturation/Brasa/Source/forja/ui
)
