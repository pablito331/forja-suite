#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::@@SLUG@@
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "@@PRODUCT@@";
    info.subtitle = u ("@@SUBTITLE@@ \u00b7 FORJA");
    info.tagline = u ("TODO: uma frase curta sobre o papel musical do plugin, "
                      "sem prometer o que o DSP n\u00e3o faz.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("TODO: descreva o efeito, de onde ele vem na Airwindows e o que o "
               "usu\u00e1rio deve esperar ouvir.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("TODO: explique o la\u00e7o do original (detector, filtro, satura\u00e7\u00e3o, "
               "buffer) e o que o adaptador Forja acrescenta.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("TODO: pontos de partida por fonte, intera\u00e7\u00e3o com o bypass e "
               "lat\u00eancia.")
        }
    });

    info.controls.push_back ({ "@@PARAM_NAME@@", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("TODO: descreva o controle no papel musical.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows @@AIRWINDOWS@@";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("TODO: registre os desvios do adaptador (estado por "
                               "inst\u00e2ncia, bypass, medidores, camadas Forja).");

    return info;
}

} // namespace forja::@@SLUG@@
