#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::canivete
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "CANIVETE";
    info.subtitle = u ("PLAYER SF2/SF3 \u00b7 FORJA");
    info.tagline = u ("CANIVETE \u00e9 o player de soundfont da su\u00edte: abre bancos "
                      ".sf2 (e .sf3 quando o build traz o decodificador Ogg) e toca "
                      "16 canais GM pelo MIDI da DAW.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("L\u00ea um banco da biblioteca Documents/Forja/Canivete e o exp\u00f5e "
               "como instrumento multi-timbral: cada canal MIDI carrega um preset "
               "pr\u00f3prio, o programa 10 (canal 9) \u00e9 tratado como bateria do "
               "GM, e program change, bank select MSB/LSB, CC e pitch bend seguem a "
               "norma. O knob BANK escolhe o arquivo; o VOLUME manda no trim do "
               "master."),
            u ("\u00c9 o caminho r\u00e1pido para tocar curadorias em SoundFont "
               "(arquivos de Polyphone, sf2edit ou bancos GM da vida) sem DAW de "
               "amostras separada.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("Por dentro toca o TinySoundFont v0.9 (MIT, Bernhard Schelling), um "
               "sintetizador SoundFont2 de um \u00fanico header, envolvido pelo "
               "adapter forja::canivete::SoundFontEngine. O bank \u00e9 lido em thread "
               "de background e entra no \u00e1udio trocado por token; os 16 canais e "
               "o pool de 128 vozes s\u00e3o pr\u00e9-criados na carga para o processBlock "
               "nunca alocar mem\u00f3ria."),
            u ("SF3 (amostras em Ogg/Vorbis) s\u00f3 existe quando a pasta de vendor "
               "traz o stb_vorbis \u2014 o build detecta e acende o formato sozinho. "
               "Nada de FluidSynth: a su\u00edte mant\u00e9m o bin\u00e1rio livre de "
               "GPL."),
            u ("\u00c9 um instrumento: n\u00e3o processa o \u00e1udio da DAW, s\u00f3 "
               "gera o seu. Lat\u00eancia medida: 0 amostras. O mesmo engine serve a "
               "gaveta de instrumento do ForjaRack.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Deposite os .sf2/.sf3 em Documentos/Forja/Canivete, toque ATUALIZAR "
               "e escolha o bank. A carga roda em segundo plano; o status mostra o "
               "resultado. Toque MIDI no canal 1 para o preset principal \u2014 "
               "program change troca o instrumento e o nome do preset aparece no "
               "cart\u00e3o. BYPASS silencia com rampa de 10 ms sem parar as vozes.")
        }
    });

    info.controls.push_back ({ "Bank", u ("lista da biblioteca \u00b7 padr\u00e3o none"),
                               u ("Arquivo .sf2/.sf3 tocado, lido de "
                                  "Documents/Forja/Canivete.") });
    info.controls.push_back ({ u ("Volume"), u ("-24 a +24 dB \u00b7 padr\u00e3o 0 dB"),
                               u ("Trim do master sobre a sa\u00edda do sintetizador.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Silencia a sa\u00edda com rampa de 10 ms.") });

    info.provenance.source = "TinySoundFont v0.9 (schellingb/TinySoundFont)";
    info.provenance.author = "Bernhard Schelling (tsf); Forja (adapter e UI)";
    info.provenance.license = u ("MIT (tsf). SF3 usa stb_vorbis, dom\u00ednio p\u00fablico. "
                                 "Sem FluidSynth (LGPL) por regra da su\u00edte.");
    info.provenance.commit = "vendor byte-exato em Forja/Source/forja/vendor/soundfont "
                             "(SHA-256 registrado em PROVENANCE.md)";
    info.provenance.notes = u ("A Forja n\u00e3o altera o DSP do TinySoundFont: o "
                               "adapter apenas envolve a API p\u00fablica (carga em "
                               "background, canais, render) e acrescenta a camada "
                               "padr\u00e3o da su\u00edte \u2014 bypass com crossfade, "
                               "trim do master, medidores e estado via APVTS. Sem "
                               "paridade com vendor Airwindows: os testes cobrem "
                               "carga de bank, MIDI multi-timbral, estado e aus\u00eancia "
                               "de aloca\u00e7\u00e3o no \u00e1udio.");

    return info;
}

} // namespace forja::canivete
