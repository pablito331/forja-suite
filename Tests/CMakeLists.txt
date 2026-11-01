set(FORJA_VENDOR "${CMAKE_SOURCE_DIR}/Forja/Source/forja/vendor/airwindows")

set(BUTTERCOMP2_SOURCES
    ${FORJA_VENDOR}/ButterComp2/ButterComp2.cpp
    ${FORJA_VENDOR}/ButterComp2/ButterComp2_plug.cpp)

set(PRESSURE5_SOURCES
    ${FORJA_VENDOR}/Pressure5/Pressure5.cpp
    ${FORJA_VENDOR}/Pressure5/Pressure5_plug.cpp)

set(POP2_SOURCES
    ${FORJA_VENDOR}/Pop2/Pop2.cpp
    ${FORJA_VENDOR}/Pop2/Pop2_plug.cpp)

set(PARAMETRIC_SOURCES
    ${FORJA_VENDOR}/Parametric/Parametric.cpp
    ${FORJA_VENDOR}/Parametric/ParametricProc.cpp)

set(BAXANDALL3_SOURCES
    ${FORJA_VENDOR}/Baxandall3/Baxandall3.cpp
    ${FORJA_VENDOR}/Baxandall3/Baxandall3Proc.cpp)

set(SMOOTHEQ2_SOURCES
    ${FORJA_VENDOR}/SmoothEQ2/SmoothEQ2.cpp
    ${FORJA_VENDOR}/SmoothEQ2/SmoothEQ2Proc.cpp)

set(DEBESS_SOURCES
    ${FORJA_VENDOR}/DeBess/DeBess.cpp
    ${FORJA_VENDOR}/DeBess/DeBessProc.cpp)

set(CSTRIP2_SOURCES
    ${FORJA_VENDOR}/CStrip2/CStrip2.cpp
    ${FORJA_VENDOR}/CStrip2/CStrip2Proc.cpp)

set(X2BUSS_SOURCES
    ${FORJA_VENDOR}/X2Buss/X2Buss.cpp
    ${FORJA_VENDOR}/X2Buss/X2BussProc.cpp)

set(TOTAPE9_SOURCES
    ${FORJA_VENDOR}/ToTape9/ToTape9.cpp
    ${FORJA_VENDOR}/ToTape9/ToTape9Proc.cpp)

set(IRONOXIDE5_SOURCES
    ${FORJA_VENDOR}/IronOxide5/IronOxide5.cpp
    ${FORJA_VENDOR}/IronOxide5/IronOxide5Proc.cpp)

set(TAPEDELAY2_SOURCES
    ${FORJA_VENDOR}/TapeDelay2/TapeDelay2.cpp
    ${FORJA_VENDOR}/TapeDelay2/TapeDelay2Proc.cpp)

set(ECHOJAM_SOURCES
    ${FORJA_VENDOR}/Echojam/Echojam.cpp
    ${FORJA_VENDOR}/Echojam/EchojamProc.cpp)

set(KROCKSTAR2_SOURCES
    ${FORJA_VENDOR}/kRockstar2/kRockstar2.cpp
    ${FORJA_VENDOR}/kRockstar2/kRockstar2Proc.cpp)

set(KSTATION_SOURCES
    ${FORJA_VENDOR}/kStation/kStation.cpp
    ${FORJA_VENDOR}/kStation/kStationProc.cpp)

set(DEREZ5_SOURCES
    ${FORJA_VENDOR}/DeRez5/DeRez5.cpp
    ${FORJA_VENDOR}/DeRez5/DeRez5Proc.cpp)

set(PURESTDRIVE_SOURCES
    ${FORJA_VENDOR}/PurestDrive/PurestDrive.cpp
    ${FORJA_VENDOR}/PurestDrive/PurestDriveProc.cpp)

set(SWEETEN_SOURCES
    ${FORJA_VENDOR}/Sweeten/Sweeten.cpp
    ${FORJA_VENDOR}/Sweeten/SweetenProc.cpp)

set(PURESTWARM3_SOURCES
    ${FORJA_VENDOR}/PurestWarm3/PurestWarm3.cpp
    ${FORJA_VENDOR}/PurestWarm3/PurestWarm3Proc.cpp)

set(MOJO_SOURCES
    ${FORJA_VENDOR}/Mojo/Mojo.cpp
    ${FORJA_VENDOR}/Mojo/MojoProc.cpp)

set(DYNO_SOURCES
    ${FORJA_VENDOR}/Dyno/Dyno.cpp
    ${FORJA_VENDOR}/Dyno/DynoProc.cpp)

set(SLEW3_SOURCES
    ${FORJA_VENDOR}/Slew3/Slew3.cpp
    ${FORJA_VENDOR}/Slew3/Slew3Proc.cpp)

set(CABS2_SOURCES
    ${FORJA_VENDOR}/Cabs2/Cabs2.cpp
    ${FORJA_VENDOR}/Cabs2/Cabs2Proc.cpp)

set(POINTYGUITAR_SOURCES
    ${FORJA_VENDOR}/PointyGuitar/PointyGuitar.cpp
    ${FORJA_VENDOR}/PointyGuitar/PointyGuitarProc.cpp)

set(CHIMEYGUITAR2_SOURCES
    ${FORJA_VENDOR}/ChimeyGuitar2/ChimeyGuitar2.cpp
    ${FORJA_VENDOR}/ChimeyGuitar2/ChimeyGuitar2Proc.cpp)

set(CRICKBASS_SOURCES
    ${FORJA_VENDOR}/CrickBass/CrickBass.cpp
    ${FORJA_VENDOR}/CrickBass/CrickBassProc.cpp)

set(WOLFBOT_SOURCES
    ${FORJA_VENDOR}/Wolfbot/Wolfbot.cpp
    ${FORJA_VENDOR}/Wolfbot/WolfbotProc.cpp)

set(CHIMEYDELUXE_SOURCES
    ${FORJA_VENDOR}/ChimeyDeluxe/ChimeyDeluxe.cpp
    ${FORJA_VENDOR}/ChimeyDeluxe/ChimeyDeluxeProc.cpp)

set(ADCLIP9_SOURCES
    ${FORJA_VENDOR}/ADClip9/ADClip9.cpp
    ${FORJA_VENDOR}/ADClip9/ADClip9Proc.cpp)

set(FINALCLIP_SOURCES
    ${FORJA_VENDOR}/FinalClip/FinalClip.cpp
    ${FORJA_VENDOR}/FinalClip/FinalClipProc.cpp)

set_source_files_properties(${FORJA_VENDOR}/ButterComp2/ButterComp2_plug.cpp
    PROPERTIES COMPILE_DEFINITIONS "createEffectInstance=createButterComp2Instance")

set_source_files_properties(${FORJA_VENDOR}/Pressure5/Pressure5_plug.cpp
    PROPERTIES COMPILE_DEFINITIONS "createEffectInstance=createPressure5Instance")

set_source_files_properties(${FORJA_VENDOR}/Pop2/Pop2_plug.cpp
    PROPERTIES COMPILE_DEFINITIONS "createEffectInstance=createPop2Instance")

add_executable(forja_parity
    Source/parity_main.cpp
    Source/parity_tempera.cpp
    Source/parity_prensa.cpp
    Source/parity_malho.cpp
    ${BUTTERCOMP2_SOURCES}
    ${PRESSURE5_SOURCES}
    ${POP2_SOURCES})

target_include_directories(forja_parity PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vst2_shim
    ${CMAKE_SOURCE_DIR}/Forja/Source
    ${FORJA_VENDOR}/ButterComp2
    ${FORJA_VENDOR}/Pressure5
    ${FORJA_VENDOR}/Pop2)

target_compile_definitions(forja_parity PRIVATE _USE_MATH_DEFINES)
target_compile_features(forja_parity PRIVATE cxx_std_20)
add_test(NAME parity_legacy COMMAND forja_parity)

add_executable(forja_alloc_guard Source/alloc_guard.cpp)
target_include_directories(forja_alloc_guard PRIVATE ${CMAKE_SOURCE_DIR}/Forja/Source)
target_compile_features(forja_alloc_guard PRIVATE cxx_std_20)
add_test(NAME alloc_guard COMMAND forja_alloc_guard)

# Engine do Drumforge: sampler proprio, sem vendor Airwindows e sem paridade.
# O teste cobre camadas, round-robin, choke, roubo de voz, auxiliares, bypass,
# mapa MIDI e ausencia de alocacao no caminho de audio.
add_executable(forja_drumforge_engine
    Source/drumforge_engine.cpp
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source/engine/DrumEngine.cpp
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source/engine/SampleStore.cpp)

target_include_directories(forja_drumforge_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source
    ${CMAKE_SOURCE_DIR}/Forja/Source)

target_compile_definitions(forja_drumforge_engine PRIVATE _USE_MATH_DEFINES)
target_compile_options(forja_drumforge_engine PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_features(forja_drumforge_engine PRIVATE cxx_std_20)

target_link_libraries(forja_drumforge_engine PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_events
    juce::juce_graphics
    juce::juce_recommended_config_flags)

add_test(NAME drumforge_engine COMMAND forja_drumforge_engine)

# Engine do ForjaRack: rack serie de modulos Airwindows MIT. Sem paridade do
# rack em si (cada modulo ja e parity-testado isolado); o teste cobre cadeia em
# serie, pass-through, bypass/mute/solo de slot, fase e isolamento por instancia.
add_executable(forja_forjarack_engine
    Source/forjarack_engine.cpp
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source/rack/RackEngine.cpp
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source/rack/ModuleCatalog.cpp)

target_include_directories(forja_forjarack_engine PRIVATE
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source
    ${CMAKE_SOURCE_DIR}/Forja/Source)

target_compile_definitions(forja_forjarack_engine PRIVATE _USE_MATH_DEFINES)
target_compile_options(forja_forjarack_engine PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_features(forja_forjarack_engine PRIVATE cxx_std_20)

add_test(NAME forjarack_engine COMMAND forja_forjarack_engine)

# Gaveta HOSPEDADA: hosting dinamico de plugins de Documents/Forja/Rack. Sem
# paridade (o DSP vem de fora); um stub de AudioPluginInstance cobre adocao de
# efeito/instrumento, deteccao de papel por bus, mapa dinamico de params, trava
# da UI nativa, MIDI nativo (void* do core) e propagacao de latencia no engine.
# Liga juce_audio_processors: HostedModule so existe no mundo JUCE.
add_executable(forja_forjarack_hosted
    Source/forjarack_hosted.cpp
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source/rack/HostedModule.cpp
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source/rack/RackEngine.cpp
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source/rack/ModuleCatalog.cpp)

target_include_directories(forja_forjarack_hosted PRIVATE
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source
    ${CMAKE_SOURCE_DIR}/Forja/Source)

target_compile_definitions(forja_forjarack_hosted PRIVATE _USE_MATH_DEFINES)
target_compile_options(forja_forjarack_hosted PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_features(forja_forjarack_hosted PRIVATE cxx_std_20)

target_link_libraries(forja_forjarack_hosted PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)

add_test(NAME forjarack_hosted COMMAND forja_forjarack_hosted)

# MiniDrum: o wrapper que toca o engine do Drumforge em formato mini (pequeno
# VST e gaveta do ForjaRack). Sem paridade (DSP proprio herdado, ja coberto
# pelo teste do engine); cobre carga adocao, volume, premix do kit, latencia,
# bypass, medidor, troca com reap, ausencia de alocacao e registro da gaveta.
add_executable(forja_minidrum
    Source/minidrum.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Minidrum/Source/minidrum/MiniDrum.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Minidrum/Source/minidrum/MiniDrumRackModule.cpp
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source/rack/ModuleCatalog.cpp
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source/engine/DrumEngine.cpp
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source/engine/SampleStore.cpp)

target_include_directories(forja_minidrum PRIVATE
    ${CMAKE_SOURCE_DIR}/Minis/Minidrum/Source
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source
    ${CMAKE_SOURCE_DIR}/drumforge/Drumforge/Source
    ${CMAKE_SOURCE_DIR}/Forja/Source)

target_compile_definitions(forja_minidrum PRIVATE _USE_MATH_DEFINES JUCE_MODAL_LOOPS_PERMITTED=1)
target_compile_options(forja_minidrum PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_features(forja_minidrum PRIVATE cxx_std_20)

target_link_libraries(forja_minidrum PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)

add_test(NAME minidrum COMMAND forja_minidrum)

# MiniSat: mapeamento documentado do DRIVE, identidade bit a bit com o
# adaptador bruto forja::BrasaDSP (a paridade mora no BRASA), bypass, latencia
# medida, RT-safety do wrapper e a gaveta registrada no catalogo do rack.
add_executable(forja_minisat
    Source/minisat.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Minisat/Source/minisat/MiniSat.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Minisat/Source/minisat/MiniSatRackModule.cpp
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source/rack/ModuleCatalog.cpp)

target_include_directories(forja_minisat PRIVATE
    ${CMAKE_SOURCE_DIR}/Minis/Minisat/Source
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source
    ${CMAKE_SOURCE_DIR}/Forja/Source)

target_compile_definitions(forja_minisat PRIVATE _USE_MATH_DEFINES)
target_compile_options(forja_minisat PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_features(forja_minisat PRIVATE cxx_std_20)

target_link_libraries(forja_minisat PRIVATE
    juce::juce_audio_basics
    juce::juce_events
    juce::juce_graphics
    juce::juce_recommended_config_flags)

add_test(NAME minisat COMMAND forja_minisat)

# Canivete Suico: player SoundFont (TinySoundFont MIT) multi-timbral. Sem
# paridade (DSP nao e nosso; o wrapper so envolve a API publica do tsf). O
# SoundFontEngine.cpp e o unico TU com TSF_IMPLEMENTATION; o teste gera um SF2
# minimo em disco e cobre carga/adocao, volume, latencia, bypass, medidor por
# bloco, program change/CC multi-timbral, troca com reap e ausencia de alocacao.
add_executable(forja_canivete
    Source/canivete.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Canivete/Source/canivete/SoundFontEngine.cpp
    ${CMAKE_SOURCE_DIR}/Minis/Canivete/Source/canivete/CaniveteRackModule.cpp
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source/rack/ModuleCatalog.cpp)

target_include_directories(forja_canivete PRIVATE
    ${CMAKE_SOURCE_DIR}/Minis/Canivete/Source
    ${CMAKE_SOURCE_DIR}/Forjarack/Forjarack/Source
    ${CMAKE_SOURCE_DIR}/Forja/Source)

forja_link_soundfont_vendor(forja_canivete)

target_compile_definitions(forja_canivete PRIVATE _USE_MATH_DEFINES JUCE_MODAL_LOOPS_PERMITTED=1)
target_compile_options(forja_canivete PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_features(forja_canivete PRIVATE cxx_std_20)

target_link_libraries(forja_canivete PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)

add_test(NAME canivete COMMAND forja_canivete)

# One parity executable per model: each compiles only its own vendored sources,
# so a failure in one model never masks another.
function(forja_add_parity_model slug model_name vendor_dir)
    add_executable(forja_parity_${slug}
        Source/parity_single_main.cpp
        Source/parity_${slug}.cpp
        ${ARGN})

    target_compile_definitions(forja_parity_${slug} PRIVATE
        _USE_MATH_DEFINES
        FORJA_PARITY_ENTRY=run${model_name}Parity)

    target_include_directories(forja_parity_${slug} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vst2_shim
        ${CMAKE_SOURCE_DIR}/Forja/Source
        ${FORJA_VENDOR}/${vendor_dir})

    target_compile_features(forja_parity_${slug} PRIVATE cxx_std_20)
    add_test(NAME parity_${slug} COMMAND forja_parity_${slug})
endfunction()

forja_add_parity_model(cinzel Cinzel Parametric ${PARAMETRIC_SOURCES})
forja_add_parity_model(torno Torno Baxandall3 ${BAXANDALL3_SOURCES})
forja_add_parity_model(lima Lima SmoothEQ2 ${SMOOTHEQ2_SOURCES})
forja_add_parity_model(escoria Escoria DeBess ${DEBESS_SOURCES})
forja_add_parity_model(bigorna Bigorna CStrip2 ${CSTRIP2_SOURCES})
forja_add_parity_model(cadinho Cadinho X2Buss ${X2BUSS_SOURCES})
forja_add_parity_model(laminador Laminador ToTape9 ${TOTAPE9_SOURCES})
forja_add_parity_model(oxido Oxido IronOxide5 ${IRONOXIDE5_SOURCES})
forja_add_parity_model(repique Repique TapeDelay2 ${TAPEDELAY2_SOURCES})
forja_add_parity_model(eco Eco Echojam ${ECHOJAM_SOURCES})
forja_add_parity_model(salao Salao kRockstar2 ${KROCKSTAR2_SOURCES})
forja_add_parity_model(folego Folego kStation ${KSTATION_SOURCES})
forja_add_parity_model(granalha Granalha DeRez5 ${DEREZ5_SOURCES})
forja_add_parity_model(lixa Lixa PurestDrive ${PURESTDRIVE_SOURCES})
forja_add_parity_model(mel Mel Sweeten ${SWEETEN_SOURCES})
forja_add_parity_model(cera Cera PurestWarm3 ${PURESTWARM3_SOURCES})
forja_add_parity_model(corpo Corpo Mojo ${MOJO_SOURCES})
forja_add_parity_model(impeto Impeto Dyno ${DYNO_SOURCES})
forja_add_parity_model(rebarba Rebarba Slew3 ${SLEW3_SOURCES})
forja_add_parity_model(gabinete Gabinete Cabs2 ${CABS2_SOURCES})
forja_add_parity_model(ponta Ponta PointyGuitar ${POINTYGUITAR_SOURCES})
forja_add_parity_model(chime Chime ChimeyGuitar2 ${CHIMEYGUITAR2_SOURCES})
forja_add_parity_model(crick Crick CrickBass ${CRICKBASS_SOURCES})
forja_add_parity_model(lobo Lobo Wolfbot ${WOLFBOT_SOURCES})
forja_add_parity_model(deluxe Deluxe ChimeyDeluxe ${CHIMEYDELUXE_SOURCES})
forja_add_parity_model(muralha Muralha ADClip9 ${ADCLIP9_SOURCES})
forja_add_parity_model(dique Dique FinalClip ${FINALCLIP_SOURCES})

