#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::corpo
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "CORPO";
    info.subtitle = u ("CORPO E GRAVES \u00b7 FORJA");
    info.tagline = u ("CORPO \u00e9 o ganho com forma: o Mojo empurra a onda para dentro de "
                      "uma curva que engorda o sinal, dando peso e graves sem a aspereza de "
                      "um drive comum.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O controle \u00e9 um ganho de entrada de \u221212 a +12 dB. Esse ganho entra "
               "na curva do Mojo, que usa a raiz quarta do m\u00f3dulo do sinal para decidir "
               "quanto curva aplicar: sinais pequenos passam quase retos, sinais grandes "
               "ganham corpo."),
            u ("Na pr\u00e1tica, subir o controle esquenta o grave e o corpo da fonte; descer "
               "deixa o sinal mais magro e controlado. \u00c9 o irm\u00e3o de peso do "
               "\u00cdMPETO, que usa a mesma entrada com uma curva de compress\u00e3o.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O ganho \u00e9 `10^((controle \u00b7 24 \u2212 12) / 20)`. Em seguida o original "
               "calcula `mojo = |x|^0,25` e aplica `sin(x \u00b7 mojo \u00b7 \u03c0/2) / mojo "
               "\u00b7 0,987654321`. O fator final de 0,987654321 mant\u00e9m o n\u00edvel "
               "pr\u00f3ximo do original."),
            u ("O resultado \u00e9 uma curva suave de corpo, sem estado al\u00e9m dos \u00faltimos "
               "valores do RNG. N\u00e3o h\u00e1 filtro nem atraso.")
        }
    });

    info.sections.push_back ({
        u ("O controle"),
        {
            u ("O padr\u00e3o \u00e9 0,50, que corresponde a 0,0 dB de ganho \u2014 mas a curva "
               "de corpo continua atuando. Para um empurr\u00e3o de graves, 0,60 a 0,70; para "
               "deixar a fonte mais leve, 0,40 a 0,45."),
            u ("N\u00e3o h\u00e1 ganho de compensa\u00e7\u00e3o: o n\u00edvel de sa\u00edda "
               "acompanha a curva. Ajuste na cadeia, n\u00e3o no plugin.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Em voz, 0,58 para corpo sem embolar. Em baixo, 0,65. Em mix bus, 0,52 s\u00f3 "
               "para assentar. BYPASS desvia o DSP com rampa de 10 ms; o processamento \u00e9 "
               "em double, sem aloca\u00e7\u00e3o e sem PDC.")
        }
    });

    info.controls.push_back ({ "Corpo", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho de entrada de \u221212 a +12 dB antes da curva.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows Mojo";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do Mojo: o "
                               "ganho de entrada, a raiz quarta do m\u00f3dulo, a curva de "
                               "seno e o fator 0,987654321. Acrescenta medidores de pico, "
                               "bypass suavizado e leitura de clipe; nenhuma aloca\u00e7\u00e3o "
                               "dentro de process().");

    return info;
}

} // namespace forja::corpo
