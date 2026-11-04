#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::tempera
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = u ("T\u00c9MPERA");
    info.subtitle = u ("COMPRESSOR DE BUS \u00b7 FORJA");
    info.tagline = u ("T\u00eampera \u00e9 o compressor de cola: quatro compressores paralelos que "
                      "se alternam a cada amostra unem os elementos do bus e arredondam transientes "
                      "sem apagar o ataque. O movimento \u00e9 suave e o car\u00e1ter, musical \u2014 "
                      "pensado para mix bus, bus de bateria e grupos onde o objetivo \u00e9 uni\u00e3o, "
                      "n\u00e3o impacto.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("T\u00eamp\u00e9ra \u00e9 um compressor de cola. Ele reduz a dist\u00e2ncia entre os elementos "
               "do bus, arredonda transientes e mant\u00e9m a sensa\u00e7\u00e3o de conjunto. \u00c9 pensado "
               "para mix bus, bus de bateria e grupos onde o objetivo \u00e9 uni\u00e3o, n\u00e3o impacto.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do ButterComp2 da Airwindows. O sinal \u00e9 dividido em duas metades "
               "positiva e negativa e cada uma alimenta dois controles de envelope independentes, "
               "A e B. A cada amostra o plugin alterna entre os pares A e B, de modo que quatro "
               "compressores paralelos trabalham em conjunto. O resultado \u00e9 uma redu\u00e7\u00e3o de "
               "ganho densa, mas com movimento menos \u00f3bvio do que um compressor \u00fanico."),
            u ("O envelope de cada controle acompanha o n\u00edvel de sa\u00edda da amostra anterior, "
               "o que faz a compress\u00e3o reagir ao pr\u00f3prio sinal j\u00e1 processado. O algoritmo "
               "tamb\u00e9m soma um ru\u00eddo de ar de n\u00edvel muito baixo, parte do car\u00e1ter original. "
               "N\u00e3o h\u00e1 detector est\u00e9reo linkado: os canais t\u00eam envelopes pr\u00f3prios."),
            u ("Lat\u00eancia: 0 amostras. O adaptador Forja processa em double, aplica o bypass com "
               "rampa de 10 ms e exp\u00f5e a redu\u00e7\u00e3o de ganho m\u00ednima entre os canais.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Os tr\u00eas knobs s\u00e3o cont\u00ednuos de 0 a 1. Duplo clique volta ao padr\u00e3o; as "
               "setas fazem ajuste fino. O medidor GR mostra a redu\u00e7\u00e3o de ganho em dB e o "
               "gr\u00e1fico de movimento guarda os \u00faltimos segundos. O bot\u00e3o BYPASS desvia o DSP "
               "sem clique.")
        }
    });

    info.controls.push_back ({
        u ("Compress\u00e3o"),
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
        u ("Ganho de entrada e intensidade da compress\u00e3o. Em 0 o sinal passa praticamente "
           "intacto; acima de 0,3 o bus come\u00e7a a ficar denso e unido.")
    });
    info.controls.push_back ({
        u ("Sa\u00edda"),
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
        u ("N\u00edvel de sa\u00edda do compressor. Use para compensar a perda de pico e manter o "
           "volume percebido ao comparar com o bypass.")
    });
    info.controls.push_back ({
        "Mistura",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
        u ("Mistura dry/wet. Valores intermedi\u00e1rios permitem compress\u00e3o paralela dentro "
           "do pr\u00f3prio plugin.")
    });
    info.controls.push_back ({
        "Bypass",
        u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
        u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms. Os medidores continuam ativos.")
    });

    info.provenance.source = "Airwindows ButterComp2";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("Licen\u00e7a MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double, move o ru\u00eddo "
                               "de ar para o estado da inst\u00e2ncia (o original usa vari\u00e1veis "
                               "static dentro do la\u00e7o), adiciona bypass suavizado e medidores. "
                               "A lei de compress\u00e3o e a altern\u00e2ncia A/B seguem o original.");

    return info;
}

} // namespace forja::tempera
