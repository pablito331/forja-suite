#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::lobo
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "LOBO";
    info.subtitle = u ("TRANSFORMADOR DI \u00b7 FORJA");
    info.tagline = u ("LOBO \u00e9 um DI de transformador sem controles: dois preditores "
                      "Kalman em s\u00e9rie limpam e d\u00e3o corpo ao sinal antes do amp, "
                      "como um transformador de entrada bem escolhido.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O Wolfbot da Airwindows n\u00e3o \u00e9 um amp nem um equalizador. Ele "
               "condiciona a entrada com dois est\u00e1gios de predi\u00e7\u00e3o Kalman: um "
               "passa-altos suave e um passa-baixos suave, cada um com seu pr\u00f3prio "
               "controle de ganho interno. O resultado \u00e9 um sinal com mais corpo e "
               "menos aspereza, do jeito que um bom transformador de DI faz."),
            u ("N\u00e3o h\u00e1 satura\u00e7\u00e3o nem distor\u00e7\u00e3o. \u00c9 um condicionador: "
               "coloque antes de amps, pedais ou at\u00e9 de um compressor de voz.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("Cada preditor mant\u00e9m tr\u00eas amostras de hist\u00f3ria, tr\u00eas "
               "velocidades e um ganho adaptativo. A cada amostra, ele estima a tend\u00eancia "
               "do sinal e mistura a previs\u00e3o com o valor real; se o erro cresce, o "
               "ganho de corre\u00e7\u00e3o sobe. O primeiro est\u00e1gio \u00e9 um passa-altos "
               "(tira o excesso de grave), o segundo \u00e9 um passa-baixos (tira a "
               "aspereza). A sa\u00edda final passa por uma curva de seno escalada."),
            u ("Como o original n\u00e3o exp\u00f5e par\u00e2metros, o adaptador tamb\u00e9m n\u00e3o "
               "inventa nenhum. O \u00fanico controle \u00e9 BYPASS.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Guitarra ou baixo: LOBO antes do amp, com o n\u00edvel de entrada da "
               "interface em torno de -18 dBFS. Voz: LOBO antes do compressor adiciona "
               "corpo sem mexer no timbre. Como o efeito \u00e9 sutil, compare com BYPASS "
               "em passagens de corpo e ataque. Processamento em double, sem "
               "aloca\u00e7\u00e3o e sem PDC.")
        }
    });

    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows Wolfbot";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do "
                               "Wolfbot: dois preditores Kalman por canal, o passa-altos e o "
                               "passa-baixos em s\u00e9rie e o RNG. Acrescenta medidores de "
                               "pico, bypass suavizado e leitura de clipe; n\u00e3o h\u00e1 "
                               "aloca\u00e7\u00e3o no caminho de \u00e1udio.");

    return info;
}

} // namespace forja::lobo
