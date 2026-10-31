#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::gabinete
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "GABINETE";
    info.subtitle = u ("GABINETE FASEADO \u00b7 FORJA");
    info.tagline = u ("GABINETE \u00e9 a cor do alto-falante sem distor\u00e7\u00e3o: "
                      "dois filtros em cascata que fecham o topo e afinam o corpo, "
                      "como um cone real em um recinto. Serve a guitarra, baixo ou voz.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O Cabs2 da Airwindows n\u00e3o simula um amplificador: ele simula o "
               "alto-falante. Um banco de 21 polos de passa-altos e outro de 13 polos "
               "de passa-baixos entram em cascata, com curvas suaves e sem resson\u00e2ncia "
               "ressaltada. O resultado \u00e9 o arredondamento caracter\u00edstico de um cone."),
            u ("O adaptador n\u00e3o adiciona satura\u00e7\u00e3o, drive ou ganho de compensa\u00e7\u00e3o. "
               "\u00c9 um coloridor de fase e frequ\u00eancia; quem distorce antes dele define "
               "o car\u00e1ter.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("Cada polo \u00e9 um filtro de um polo cuja frequ\u00eancia vem de pow() sobre "
               "o par\u00e2metro, reescalada pela taxa de amostragem. Os polos s\u00e3o aplicados "
               "em sequ\u00eancia, dividindo o sinal por 21 (passa-altos) e por 13 "
               "(passa-baixos). N\u00e3o h\u00e1 pr\u00e9-warping, ent\u00e3o a curva muda de forma com "
               "a taxa: o mesmo ajuste soa um pouco diferente em 44,1 e 96 kHz, como "
               "no original."),
            u ("Com passa-baixos em 1,00 e passa-altos em 0,00 os dois blocos ficam em "
               "repouso e o sinal passa sem filtragem. \u00c9 o ponto de compara\u00e7\u00e3o A/B.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("PASSA-BAIXOS controla o topo: 1,00 deixa passar tudo; 0,70 a 0,85 fecha "
               "o brilho de um cone de guitarra; abaixo de 0,50 o som fica abafado. "
               "PASSA-ALTOS controla o corpo: 0,00 n\u00e3o corta nada; 0,20 a 0,40 afina "
               "graves de amp aberto; acima de 0,60 serve para voz e ambi\u00eancia."),
            u ("N\u00e3o existe bot\u00e3o de n\u00edvel: o gabinete pode mudar a loudness percebida, "
               "mas n\u00e3o tem ganho embutido. Compense na cadeia.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Guitarra: PASSA-BAIXOS 0,78 e PASSA-ALTOS 0,30 depois do amp. Baixo: "
               "PASSA-BAIXOS 0,65 e PASSA-ALTOS 0,15. Voz: PASSA-ALTOS 0,55 e "
               "PASSA-BAIXOS 0,90 para tirar peso sem fechar o timbre. BYPASS desvia "
               "com rampa de 10 ms. Processamento em double, sem aloca\u00e7\u00e3o e sem PDC.")
        }
    });

    info.controls.push_back ({ "Lowpass", u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("Fecha o topo do gabinete; 1,00 = aberto.") });
    info.controls.push_back ({ "Hipass", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Corta o corpo do gabinete; 0,00 = sem corte.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows Cabs2";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do Cabs2: "
                               "as duas cascatas de polos, os estados de repouso e o RNG. "
                               "Acrescenta medidores de pico, bypass suavizado e leitura de "
                               "clipe; n\u00e3o h\u00e1 aloca\u00e7\u00e3o no caminho de \u00e1udio.");

    return info;
}

} // namespace forja::gabinete
