#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::dique
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "DIQUE";
    info.subtitle = u ("CLIPE DE PICO \u00b7 FORJA");
    info.tagline = u ("A \u00faltima comporta antes da sa\u00edda: o FinalClip arredonda picos "
                      "extremos acima de +4,18 dBFS. Abaixo disso, o sinal passa intacto.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Dique \u00e9 uma prote\u00e7\u00e3o de picos extremos. Ele n\u00e3o tem controles: "
               "recebe o sinal, limita a varia\u00e7\u00e3o entre amostras e arredonda o que passa "
               "do teto de +4,18 dBFS (pico linear 1,618)."),
            u ("Importante: n\u00e3o \u00e9 um limitador de 0 dBFS. Um sinal que chega em 0 dBFS "
               "ou at\u00e9 +4,18 dBFS sai sem redu\u00e7\u00e3o. Para limitar em 0 dBFS use o "
               "MURALHA; o DIQUE serve para picos que ainda ultrapassem +4,18 dBFS.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do FinalClip da Airwindows. Cada amostra sofre um pr\u00e9-clipe "
               "em \u00b14 e um limite de slew de 0,618033988749894, a propor\u00e7\u00e3o \u00e1urea. "
               "Quando o sinal passa de 1,618, o est\u00e1gio ClipOnly2 entra e reconstr\u00f3i a "
               "transi\u00e7\u00e3o com o buffer interno, evitando o corte quadrado."),
            u ("Medido no adaptador: senoides de 0 dBFS e +3 dBFS saem inalteradas; +6 dBFS "
               "saem em +4,18 dBFS; +12 e +18 dBFS tamb\u00e9m convergem para +4,18 dBFS."),
            u ("N\u00e3o h\u00e1 ganho, mix ou RNG. O plugin \u00e9 determin\u00edstico e a \u00fanica camada "
               "Forja \u00e9 o bypass com rampa de 10 ms."),
            u ("Lat\u00eancia: 1 amostra. O caminho seco do bypass recebe o mesmo atraso para o "
               "crossfade n\u00e3o virar filtro em peine.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Coloque no fim da cadeia, depois de est\u00e1gios que possam gerar overs, e "
               "observe o medidor: ele s\u00f3 mostra redu\u00e7\u00e3o quando o pico ultrapassa "
               "+4,18 dBFS. O bot\u00e3o BYPASS compara o sinal seco com o processado.")
        }
    });

    info.controls.push_back ({
        "Bypass",
        u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
        u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms e atraso de 1 amostra no caminho seco.")
    });

    info.provenance.source = "Airwindows FinalClip";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("Licen\u00e7a MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve processDoubleReplacing em double. "
                               "O original dimensiona intermediate[16] mas escreve no \u00edndice "
                               "spacing (at\u00e9 16); o adaptador usa 18 posi\u00e7\u00f5es para "
                               "eliminar o acesso fora dos limites em sample rates altos. "
                               "Bypass e medi\u00e7\u00e3o s\u00e3o camadas Forja; a lat\u00eancia "
                               "medida por impulso \u00e9 1 amostra.");

    return info;
}

} // namespace forja::dique
