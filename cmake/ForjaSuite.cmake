# LTO acelera o áudio e é o padrão local. A CI desliga para caber no tempo do runner.
option(FORJA_LTO "Enable link-time optimization for plugin targets" ON)

# O teste é feito no VST3 dentro da DAW, entao o Standalone nao faz parte do
# build por padrao. Ligue ON apenas quando precisar abrir um plugin sem DAW
# (por exemplo, para capturar a UI). Os pacotes de release nunca incluem Standalone.
option(FORJA_STANDALONE "Build standalone plugin targets" OFF)

# Configuração comum a plugins de efeito e instrumentos (Drumforge).
function(forja_configure_target target_name source_dir)
    target_include_directories(${target_name} PRIVATE ${source_dir})

    target_compile_definitions(${target_name} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_compile_options(${target_name} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    )

    set(forja_lto_flags "")
    if(FORJA_LTO)
        set(forja_lto_flags juce::juce_recommended_lto_flags)
    endif()

    target_link_libraries(${target_name}
        PRIVATE
        forja_ui
        juce::juce_audio_utils
        PUBLIC
        juce::juce_recommended_config_flags
        ${forja_lto_flags}
        juce::juce_recommended_warning_flags
    )
endfunction()

# Efeito: recebe áudio do host, não aceita MIDI.
# O host exibe "Produto (Fabricante)"; o fabricante carrega a família para
# que a categoria apareça no navegador de plugins da DAW.
#
# NEEDS_MIDI_INPUT nao e decorativo: ela liga a macro JucePlugin_WantsMidiInput
# do wrapper VST3, que (a) anuncia o barramento kEvent de entrada de MIDI ao
# host e (b) guarda a conversao data.inputEvents -> MidiBuffer dentro de
# process(). Com a macro em 0 o plugin recebe um MidiBuffer SEMPRE vazio, e o
# acceptsMidi() do C++ nao tem efeito nenhum no VST3. Um hibrido (o ForjaRack:
# cadeia de FX + gavetas-instrumento) precisa das duas entradas, entao seta
# FORJA_WANTS_MIDI_INPUT antes de chamar o helper.
function(forja_add_plugin target_name product_name plugin_code bundle_id source_dir)
    set(categories "Fx")
    if(ARGC GREATER 5 AND NOT "${ARGV5}" STREQUAL "")
        set(categories "${ARGV5}")
    endif()

    set(company "Forja")
    if(ARGC GREATER 6 AND NOT "${ARGV6}" STREQUAL "")
        set(company "${ARGV6}")
    endif()

    set(wants_midi_input FALSE)
    if(FORJA_WANTS_MIDI_INPUT)
        set(wants_midi_input TRUE)
    endif()

    set(formats VST3)
    if(FORJA_STANDALONE)
        list(APPEND formats Standalone)
    endif()

    juce_add_plugin(${target_name}
        COMPANY_NAME "${company}"
        BUNDLE_ID ${bundle_id}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT ${wants_midi_input}
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE Forj
        PLUGIN_CODE ${plugin_code}
        PRODUCT_NAME "${product_name}"
        FORMATS ${formats}
        VST3_CATEGORIES ${categories}
    )

    forja_configure_target(${target_name} ${source_dir})
endfunction()

# Instrumento (Drumforge): toca MIDI, gera áudio e não recebe entrada de áudio.
function(forja_add_instrument target_name product_name plugin_code bundle_id source_dir)
    set(categories "Instrument|Drum")
    if(ARGC GREATER 5 AND NOT "${ARGV5}" STREQUAL "")
        set(categories "${ARGV5}")
    endif()

    set(company "Forja")
    if(ARGC GREATER 6 AND NOT "${ARGV6}" STREQUAL "")
        set(company "${ARGV6}")
    endif()

    set(formats VST3)
    if(FORJA_STANDALONE)
        list(APPEND formats Standalone)
    endif()

    juce_add_plugin(${target_name}
        COMPANY_NAME "${company}"
        BUNDLE_ID ${bundle_id}
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE Forj
        PLUGIN_CODE ${plugin_code}
        PRODUCT_NAME "${product_name}"
        FORMATS ${formats}
        VST3_CATEGORIES ${categories}
    )

    forja_configure_target(${target_name} ${source_dir})
endfunction()

# Vendor do TinySoundFont (CANIVETE). A SoundFontEngine.cpp e o unico TU com
# TSF_IMPLEMENTATION e precisa da pasta de vendor no include path. SF3
# (amostras Ogg) so acende quando stb_vorbis.c esta na pasta: o CMake detecta
# e define FORJA_CANIVETE_SF3. Nao existe stb_vorbis.h no upstream: o .c e o
# header e a implementacao no mesmo arquivo. ForjaRack e os testes compilam a
# mesma engine por caminho, entao todos passam por aqui.
set(FORJA_SOUNDFONT_VENDOR_DIR ${CMAKE_SOURCE_DIR}/Forja/Source/forja/vendor/soundfont)

function(forja_link_soundfont_vendor target_name)
    target_include_directories(${target_name} PRIVATE ${FORJA_SOUNDFONT_VENDOR_DIR})

    if(EXISTS ${FORJA_SOUNDFONT_VENDOR_DIR}/stb_vorbis.c)
        target_compile_definitions(${target_name} PRIVATE FORJA_CANIVETE_SF3=1)
        message(STATUS "CANIVETE: SF3 habilitado (stb_vorbis encontrado)")
    else()
        message(STATUS "CANIVETE: apenas SF2 (stb_vorbis ausente na pasta de vendor)")
    endif()
endfunction()
