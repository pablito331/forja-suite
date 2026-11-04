forja_add_plugin(Traco "TRACO EQ" Trac com.forja.traco ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|EQ" "Forja - EQ")

# O TRACO nao reusa adaptador Airwindows: o DSP e o engine proprio
# (engine/TracoEngine.h, header-only). A UI usa os componentes compartilhados
# da forja_ui; o RTA e o ForjaSpectrum ja validado no CINZEL e na LIMA.
target_sources(Traco PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/ForjaInfoText.h

    Source/engine/TracoEngine.h
)
