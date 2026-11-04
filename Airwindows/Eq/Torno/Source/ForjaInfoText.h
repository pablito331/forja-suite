#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::torno
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "TORNO";
    info.subtitle = u ("SHELVES DE TOM \u00b7 FORJA");
    info.tagline = u ("Torno \u00e9 um EQ de tom de duas bandas \u2014 agudos e graves \u2014 com "
                      "dois shelves Bessel entrela\u00e7ados e um est\u00e1gio de Console: a "
                      "inclina\u00e7\u00e3o musical para abrir ou encorpar uma fonte sem mexer no "
                      "centro do espectro. O ganho de entrada empurra o Console e muda o "
                      "timbre junto.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Torno \u00e9 um equalizador de tom de duas bandas: agudos e graves. N\u00e3o \u00e9 um "
               "EQ cir\u00fargico nem um Pultec: \u00e9 um controle musical de inclina\u00e7\u00e3o, do tipo "
               "que se usa para deixar uma fonte mais aberta ou mais encorpada sem mexer no "
               "centro do espectro."),
            u ("O ganho de entrada tamb\u00e9m faz parte do car\u00e1ter. Ele empurra o sinal para "
               "dentro do est\u00e1gio de Console, ent\u00e3o a mesma posi\u00e7\u00e3o de agudos soa "
               "diferente com mais ou menos entrada.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do Baxandall3 da Airwindows. O sinal passa por uma codifica\u00e7\u00e3o "
               "senoidal (Console5), dois filtros shelving de segunda ordem com Q de Bessel "
               "(0,577) \u2014 um para agudos e outro para graves \u2014 e uma decodifica\u00e7\u00e3o "
               "por arco-seno no final."),
            u ("Os dois filtros compartilham o mesmo par de estados, alternando a cada amostra "
               "entre dois conjuntos intercalados (A/B). Essa altern\u00e2ncia \u00e9 parte do som "
               "original e \u00e9 reproduzida exatamente. A frequ\u00eancia do shelf de agudos sobe "
               "com o pr\u00f3prio ganho de agudos; a do shelf de graves desce conforme o controle "
               "de graves sobe. \u00c9 um acoplamento proposital."),
            u ("Lat\u00eancia: 0 amostras. O adaptador processa em double e aplica bypass com "
               "rampa de 10 ms.")
        }
    });

    info.sections.push_back ({
        u ("O ponto neutro"),
        {
            u ("O algoritmo s\u00f3 \u00e9 neutro quando Entrada, Agudos e Graves est\u00e3o todos em "
               "0,50. Isso n\u00e3o \u00e9 um defeito: a codifica\u00e7\u00e3o de Console e os ganhos "
               "quadr\u00e1ticos fazem com que qualquer desvio mude o tom e o n\u00edvel. Por isso o "
               "bot\u00e3o PLANO existe, devolvendo os tr\u00eas controles ao padr\u00e3o de uma vez."),
            u ("Evite tratar 0,49 ou 0,51 como \u201cquase neutro\u201d. Se a inten\u00e7\u00e3o \u00e9 comparar "
               "com o sinal seco, use o bypass ou o bot\u00e3o PLANO.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Os tr\u00eas knobs s\u00e3o cont\u00ednuos de 0 a 1; duplo clique devolve ao padr\u00e3o. "
               "ENTRADA controla o n\u00edvel e a cor da satura\u00e7\u00e3o interna, AGUDOS e GRAVES "
               "moldam as extremidades. O medidor de sa\u00edda mostra os picos e o LED CLIP pode "
               "ser clicado para limpar. BYPASS desvia o DSP sem clique.")
        }
    });

    info.controls.push_back ({ "Entrada", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho de entrada antes do est\u00e1gio de Console. Altera a "
                                  "intensidade e a cor do processamento.") });
    info.controls.push_back ({ "Agudos", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Shelf de agudos. Acima de 0,50 abre; abaixo escurece.") });
    info.controls.push_back ({ "Graves", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Shelf de graves. Acima de 0,50 engorda; abaixo enxuga.") });
    info.controls.push_back ({ "Plano", u ("a\u00e7\u00e3o \u00b7 sem par\u00e2metro"),
                               u ("Devolve Entrada, Agudos e Graves a 0,50, o \u00fanico ponto neutro "
                                  "do algoritmo.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows Baxandall3";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("Licen\u00e7a MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double, preserva a "
                               "altern\u00e2ncia A/B dos estados e o acoplamento entre ganho e "
                               "frequ\u00eancia. N\u00e3o h\u00e1 curva desenhada na UI porque o caminho "
                               "inclui satura\u00e7\u00e3o e n\u00e3o \u00e9 linear; bypass suavizado e "
                               "medidores s\u00e3o adi\u00e7\u00f5es Forja.");

    return info;
}

} // namespace forja::torno
