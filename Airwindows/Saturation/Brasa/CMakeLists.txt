forja_add_plugin(Brasa "Brasa" Brsa com.forja.brasa ${CMAKE_CURRENT_SOURCE_DIR}/Source "Fx|Distortion" "Forja - Saturacao")

target_sources(Brasa PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/forja/ui/ForjaTransferCurve.cpp
    Source/forja/ui/ForjaValve.cpp

    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/forja/dsp/ForjaBrasa.h
    Source/forja/ui/ForjaTransferCurve.h
    Source/forja/ui/ForjaValve.h

    # Código original do Airwindows mantido como referência de proveniência.
    # Não é compilado: depende do SDK VST2 e o adaptador em dsp/ForjaBrasa.h
    # reproduz o laço equivalente.
    Source/forja/vendor/airwindows/Tube2/Tube2.h
    Source/forja/vendor/airwindows/Tube2/Tube2.cpp
    Source/forja/vendor/airwindows/Tube2/Tube2Proc.h
    Source/forja/vendor/airwindows/Tube2/Tube2Proc.cpp
    Source/forja/vendor/airwindows/Tube2/LICENSE
    Source/forja/vendor/airwindows/Tube2/README.md
)

set_source_files_properties(
    Source/forja/vendor/airwindows/Tube2/Tube2.h
    Source/forja/vendor/airwindows/Tube2/Tube2.cpp
    Source/forja/vendor/airwindows/Tube2/Tube2Proc.h
    Source/forja/vendor/airwindows/Tube2/Tube2Proc.cpp
    Source/forja/vendor/airwindows/Tube2/LICENSE
    Source/forja/vendor/airwindows/Tube2/README.md
    PROPERTIES HEADER_FILE_ONLY TRUE
)
