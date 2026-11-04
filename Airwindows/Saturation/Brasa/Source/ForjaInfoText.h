#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::brasa
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "Brasa";
    info.subtitle = u ("SATURADOR DE V\u00c1LVULA \u00b7 FORJA");
    info.tagline = u ("Brasa \u00e9 o modelador de v\u00e1lvula assim\u00e9trico \u2014 o Tube2 da "
                      "Airwindows: acrescenta harm\u00f4nicos de segunda e terceira ordem, comprime "
                      "os picos de forma assim\u00e9trica e suaviza a transi\u00e7\u00e3o entre eles. "
                      "O sinal ganha peso harm\u00f4nico e uni\u00e3o sem perder o ataque, bom para "
                      "vocais, graves e bus de mixagem.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Brasa usa o algoritmo Tube2 da Airwindows como um est\u00e1gio de v\u00e1lvula. "
               "Ele adiciona harm\u00f4nicos de segunda e terceira ordem, comprime os picos de forma "
               "assim\u00e9trica e suaviza a transi\u00e7\u00e3o entre eles. O resultado \u00e9 um som mais "
               "denso e unido, bom para vocais, graves e bus de mixagem.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo \u00e9 uma curva de transfer\u00eancia n\u00e3o linear aplicada a cada amostra. "
               "Uma parte do sinal \u00e9 moldada por uma fun\u00e7\u00e3o de pot\u00eancia e outra por um termo "
               "de histerese, de modo que a curva n\u00e3o \u00e9 sim\u00e9trica entre os semiciclos positivo "
               "e negativo. Isso gera o car\u00e1ter de v\u00e1lvula em vez de uma distor\u00e7\u00e3o sim\u00e9trica."),
            u ("O par\u00e2metro de entrada controla o quanto o sinal entra na curva; o de v\u00e1lvula mistura "
               "a quantidade de curva aplicada. O est\u00e1gio final tem um clamp suave e dither de 64 bits "
               "para evitar numera\u00e7\u00e3o degenerada nos extremos. O adaptador Forja mant\u00e9m o caminho "
               "em double e aplica o bypass com rampa de 10 ms."),
            u ("Lat\u00eancia: 0 amostras. O processamento \u00e9 puramente amostra a amostra, sem buffers de atraso.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Os tr\u00eas controles s\u00e3o knobs cont\u00ednuos de 0 a 1. Duplo clique volta ao padr\u00e3o; "
               "as setas fazem ajuste fino. O bot\u00e3o BYPASS no card de sa\u00edda desvia o DSP com uma "
               "rampa curta, sem clique.")
        }
    });

    info.controls.push_back ({
        "Entrada",
        "0,00 a 1,00 \u00b7 padr\u00e3o 0,50",
        u ("Ganho que alimenta a curva da v\u00e1lvula. Mais entrada significa mais satura\u00e7\u00e3o e "
           "compress\u00e3o natural; menos entrada deixa o som mais limpo.")
    });
    info.controls.push_back ({
        u ("V\u00e1lvula"),
        "0,00 a 1,00 \u00b7 padr\u00e3o 0,50",
        u ("Mistura e intensidade do modelador. Valores altos deixam a curva mais presente e o "
           "ataque mais arredondado; valores baixos preservam a transiente.")
    });
    info.controls.push_back ({
        u ("Sa\u00edda"),
        "-24,0 a +24,0 dB \u00b7 padr\u00e3o 0,0 dB",
        u ("Ajuste de n\u00edvel depois da v\u00e1lvula. Use para compensar a compress\u00e3o percebida "
           "sem alterar o car\u00e1ter.")
    });
    info.controls.push_back ({
        "Bypass",
        u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
        u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms. Os medidores continuam mostrando a entrada "
           "e a sa\u00edda do plugin.")
    });

    info.provenance.source = "Airwindows Tube2";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("Licen\u00e7a MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja reescreve o processamento em double com estados "
                               "por inst\u00eancia, exp\u00f5e os par\u00e2metros no APVTS e adiciona bypass "
                               "suavizado e medidores. A curva, o clamp e o dither seguem o original.");

    return info;
}

} // namespace forja::brasa
