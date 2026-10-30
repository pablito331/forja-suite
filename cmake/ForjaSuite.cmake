# LTO acelera o áudio e é o padrão local. A CI desliga para caber no tempo do runner.
option(FORJA_LTO "Enable link-time optimization for plugin targets" ON)

function(forja_add_plugin target_name product_name plugin_code bundle_id source_dir)
    set(categories "Fx")
    if(ARGC GREATER 5 AND NOT "${ARGV5}" STREQUAL "")
        set(categories "${ARGV5}")
    endif()

    # O host exibe "Produto (Fabricante)"; o fabricante carrega a família para
    # que a categoria apareça no navegador de plugins da DAW.
    set(company "Forja")
    if(ARGC GREATER 6 AND NOT "${ARGV6}" STREQUAL "")
        set(company "${ARGV6}")
    endif()

    juce_add_plugin(${target_name}
        COMPANY_NAME "${company}"
        BUNDLE_ID ${bundle_id}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE Forj
        PLUGIN_CODE ${plugin_code}
        PRODUCT_NAME "${product_name}"
        FORMATS VST3 Standalone
        VST3_CATEGORIES ${categories}
    )

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
