#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::minidrum
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "MINIDRUM";
    info.subtitle = u ("TOCADOR DE KITS \u00b7 FORJA");
    info.tagline = u ("MINIDRUM \u00e9 o Drumforge em formato de gaveta: puxa um kit da "
                      "biblioteca compartilhada, toca o premix gravado no pr\u00f3prio "
                      "arquivo e manda no n\u00edvel com um \u00fanico knob.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Carrega kits .dfkit, drumkit.txt do drumlabooh ou pastas de samples e "
               "toca o kit pelo MIDI da DAW. Onde o Drumforge abre a mesa de 16 canais, "
               "o mini respeita a mix gravada no kit: se o .dfkit chegou premixado, "
               "essa mix \u00e9 o som. O knob VOLUME manda no trim do master."),
            u ("A biblioteca \u00e9 a mesma do Drumforge (Documentos/Forja/Drumforge/Kits "
               "mais as pastas do drumlabooh na raiz da conta). O que voc\u00ea salvar "
               "l\u00e1, o mini toca aqui sem copiar nada.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("Por dentro toca o mesmo engine validado do Drumforge: 16 pads, 64 vozes, "
               "camadas, round-robin, choke e o limiter do master. A carga dos samples "
               "acontece em thread de background e o kit entra no \u00e1udio trocado por "
               "token, sem alocar no caminho de \u00e1udio e sem estalo."),
            u ("\u00c9 um instrumento: n\u00e3o processa o \u00e1udio da DAW, s\u00f3 "
               "gera o seu. Lat\u00eancia medida: 0 amostras. O mesmo caminho de "
               "\u00e1udio ser\u00e1 aproveitado pela gaveta de instrumento do ForjaRack.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Toque PUXAR KIT e escolha um kit da biblioteca (arquivo ou pasta). "
               "A foto do kit aparece no cart\u00e3o quando o arquivo a traz. Para "
               "mudar o som de verdade, abra o kit no Drumforge, ajuste a mesa e salve "
               "premixado; o mini passa a tocar essa mix. BYPASS silencia com rampa de "
               "10 ms sem parar as vozes.")
        }
    });

    info.controls.push_back ({ u ("Volume"), u ("-24 a +24 dB \u00b7 padr\u00e3o 0 dB"),
                               u ("Trim do master. A mix dos pads vem gravada no kit.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Silencia a sa\u00edda com rampa de 10 ms.") });

    info.provenance.source = "Forja Drumforge (engine)";
    info.provenance.author = "Forja";
    info.provenance.license = u ("MIT. DSP original da su\u00edte, sem vendor Airwindows.");
    info.provenance.commit = "engine do Drumforge, reutilizado como bloco proprio";
    info.provenance.notes = u ("O wrapper forja::minidrum::MiniDrum re\u00fane o engine "
                               "do Drumforge, o leitor de kits e a foto de preview em um "
                               "tocador de sa\u00edda est\u00e9reo \u00fanica. Sem "
                               "paridade com vendor: os testes cobrem carga de kit, MIDI, "
                               "estado e aus\u00eancia de aloca\u00e7\u00e3o no \u00e1udio.");

    return info;
}

} // namespace forja::minidrum
