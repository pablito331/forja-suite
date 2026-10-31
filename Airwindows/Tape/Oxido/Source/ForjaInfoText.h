#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::oxido
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = u ("\u00d3XIDO");
    info.subtitle = u ("FITA E LO-FI \u00b7 FORJA");
    info.tagline = u ("\u00d3xido de ferro \u00e9 o que d\u00e1 cor \u00e0 fita. Aqui, ele \u00e9 o gr\u00e3o: "
                      "velocidade, flutter e ru\u00eddo constroem a textura, e a mistura pode ir de "
                      "s\u00f3 seco a s\u00f3 fita, inclusive invertida.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("\u00d3xido \u00e9 uma emula\u00e7\u00e3o de fita de \u00f3xido de ferro com foco em "
               "textura. Ele faz o que a fita faz de errado de prop\u00f3sito: perde agudo "
               "conforme a velocidade, ganha flutter, acrescenta ru\u00eddo e amacia os picos. "
               "\u00c9 uma m\u00e1quina de lo-fi, n\u00e3o uma fita de mastering limpa."),
            u ("Tr\u00eas recortes de velocidade: em IPS baixo o som vira cassete, com pouco agudo e "
               "muito gr\u00e3o; em IPS alto a fita fica aberta e quase limpa. Flutter e ru\u00eddo "
               "s\u00e3o independentes e podem ser desligados por completo.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do IronOxide5 da Airwindows. A primeira coisa que acontece \u00e9 o "
               "flutter: um anel de 100 amostras com at\u00e9 30 amostras de deslocamento, lido com "
               "interpola\u00e7\u00e3o. A profundidade cresce com o quadrado de FLUTTER e a varredura "
               "muda de forma aleat\u00f3ria a cada volta, sorteando um novo per\u00edodo entre 0,02 e "
               "1,00."),
            u ("Depois entra um passa-altas de um polo que alterna entre dois estados a cada "
               "amostra. A frequ\u00eancia dele vem de FITA GRAVE: quanto mais devagar a fita, mais "
               "grave ela perde. Em seguida, o ganho de ENTRADA e um limitador suave em seno "
               "(teto 1,57079633), que arredonda os picos antes da distor\u00e7\u00e3o."),
            u ("O miolo \u00e9 o modelador de \u00f3xido: um anel de 264 amostras alimenta dois "
               "integradores em paralelo, um r\u00e1pido e um lento, com taps esparsos que formam um "
               "filtro de resposta longa. A diferen\u00e7a entre eles, escalada pela velocidade, "
               "\u00e9 o que apaga o agudo e junta o grave. O original alterna dois ramos (flip "
               "A/B) a cada amostra para dobrar a resolu\u00e7\u00e3o interna."),
            u ("Um segundo limitador em seno fecha a distor\u00e7\u00e3o. O ru\u00eddo de fita n\u00e3o \u00e9 "
               "um gerador \u00e0 parte: cada amostra mistura o sinal processado com a amostra seca "
               "anterior numa propor\u00e7\u00e3o aleat\u00f3ria que depende de RU\u00cdDO e da velocidade "
               "(via RNG do original). \u00c9 isso que d\u00e1 o chiado granulado em vez de um hiss "
               "constante."),
            u ("No fim v\u00eam o ganho de SA\u00cdDA e a mistura Inv/Dry/Wet, que funciona de um jeito "
               "particular descrito abaixo. Lat\u00eancia: 0 amostras; a fita n\u00e3o tem linha de "
               "atraso fixa, s\u00f3 o flutter, que \u00e9 musical.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("ENTRADA e SA\u00cdDA s\u00e3o trims de \u221218 a +18 dB, lineares em dB, com 0,50 no "
               "centro. FITA AGUDO e FITA GRAVE definem a velocidade em polegadas por segundo "
               "(IPS): o valor exibido vai de 1,5 a 150, e o DSP aplica uma corre\u00e7\u00e3o interna de "
               "1,1 para calibrar. Em cerca de 0,30 a fita fica com cara de cassete; no padr\u00e3o "
               "(~16 IPS) ela soa aberta."),
            u ("FLUTTER controla a profundidade da modula\u00e7\u00e3o de velocidade; em 0 o anel de "
               "flutter n\u00e3o \u00e9 aplicado e o sinal segue direto. RU\u00cdDO em 0 desliga a "
               "mistura aleat\u00f3ria de fita."),
            u ("MISTURA \u00e9 o Inv/Dry/Wet do original: 0,50 deixa s\u00f3 o sinal seco, 1,00 deixa "
               "s\u00f3 a fita, e abaixo de 0,50 a fita entra com fase invertida somada ao seco. "
               "N\u00e3o \u00e9 um crossfade comum; o comportamento foi preservado como parte do som.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para textura de cassete, comece com FITA AGUDO e GRAVE em 0,30, FLUTTER em 0,25 e "
               "RU\u00cdDO em 0,30. Para uma fita mais limpa de insert, mantenha a velocidade alta e "
               "o ru\u00eddo baixo. Use MISTURA para dosar: em 1,00 voc\u00ea ouve s\u00f3 a fita, em 0,50 "
               "s\u00f3 o seco."),
            u ("Duplo clique devolve o controle ao padr\u00e3o; as setas fazem ajuste fino. BYPASS "
               "desvia o DSP com rampa de 10 ms e os medidores continuam ativos. O adaptador "
               "processa em double e n\u00e3o aloca no caminho de \u00e1udio.")
        }
    });

    info.controls.push_back ({ "Entrada", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Trim de entrada de \u221218 a +18 dB.") });
    info.controls.push_back ({ u ("Fita agudo"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,5623"),
                               u ("Velocidade da fita no lado agudo, em IPS.") });
    info.controls.push_back ({ u ("Fita grave"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,5623"),
                               u ("Velocidade no lado grave; define o corte do passa-altas.") });
    info.controls.push_back ({ "Flutter", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Profundidade da modula\u00e7\u00e3o de velocidade.") });
    info.controls.push_back ({ u ("Ru\u00eddo"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Quantidade de gr\u00e3o de fita; em 0 desliga.") });
    info.controls.push_back ({ u ("Sa\u00edda"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Trim de sa\u00edda de \u221218 a +18 dB.") });
    info.controls.push_back ({ "Mistura", u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("Inv/Dry/Wet: 0,50 s\u00f3 seco, 1,00 s\u00f3 fita, abaixo de 0,50 invertido.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows IronOxide5";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do IronOxide5: "
                               "flutter aleat\u00f3rio, passa-altas alternado, modelador com taps "
                               "esparsos, limitadores em seno, gr\u00e3o de fita e Inv/Dry/Wet. "
                               "Acrescenta medidores de pico, bypass suavizado e leitura de clipe; "
                               "a quarta pot\u00eancia dos controles de velocidade \u00e9 feita em float "
                               "como no original. A dither de 64 bits est\u00e1 comentada l\u00e1; s\u00f3 o "
                               "RNG avan\u00e7a.");

    return info;
}

} // namespace forja::oxido
