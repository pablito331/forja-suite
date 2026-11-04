#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::bigorna
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "BIGORNA";
    info.subtitle = u ("CHANNEL STRIP COMPLETO \u00b7 FORJA");
    info.tagline = u ("Bigorna \u00e9 o channel strip completo: EQ de tr\u00eas bandas com "
                      "crossover ajust\u00e1vel, filtros passa-baixo e passa-alto, compressor "
                      "estilo ButterComp e sa\u00edda com ganho e satura\u00e7\u00e3o Spiral. Molda "
                      "uma fonte de ponta a ponta em um s\u00f3 insert; o compressor s\u00f3 liga "
                      "quando COMPRESS\u00c3O passa de 0.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Bigorna \u00e9 um channel strip completo: equalizador de tr\u00eas bandas com "
               "crossover ajust\u00e1vel, filtros passa-baixos e passa-altos, compressor "
               "estilo ButterComp e um est\u00e1gio de sa\u00edda com ganho e satura\u00e7\u00e3o Spiral. "
               "Serve para moldar uma fonte de ponta a ponta, sem precisar de v\u00e1rios inserts."),
            u ("O compressor s\u00f3 liga quando COMPRESS\u00c3O passa de 0; abaixo disso o strip \u00e9 "
               "s\u00f3 EQ, filtros e sa\u00edda. Os filtros tamb\u00e9m s\u00f3 entram em a\u00e7\u00e3o quando "
               "saem do padr\u00e3o (passa-baixos em 1,00 e passa-altos em 0,00 est\u00e3o abertos).")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do CStrip2 da Airwindows. A entrada passa por um trecho de "
               "capacitor: tr\u00eas polos de passa-altos e tr\u00eas de passa-baixos, revezados em "
               "seis conjuntos de estados (A a F) por um \u201cgearbox\u201d que troca o conjunto a "
               "cada amostra. Isso espalha o trabalho dos filtros e reduz artefatos sem "
               "exigir mais polos por amostra."),
            u ("Depois vem o ButterComp: um detector de picos e vales com mistura de raz\u00e3o "
               "\u00e1urea que atualiza dois lados em altern\u00e2ncia. O ganho de entrada do "
               "compressor \u00e9 (compres\u2074\u00b735)+1, e a velocidade define o divisor do "
               "envelope, escalado pela taxa de amostragem."),
            u ("O EQ de tr\u00eas bandas separa agudos, m\u00e9dios e graves com dois IIRs de um polo "
               "(frequ\u00eancias de split) e converte cada controle em um ganho linear de \u22126 a "
               "+6 dB. Cada banda ainda tem um trecho de drive com seno/cosseno que aquece "
               "conforme o ganho. O bloco \u00e9 aplicado uma vez por amostra, mas alterna entre "
               "tr\u00eas conjuntos de estados de filtro; cada conjunto atualiza em um ter\u00e7o da "
               "taxa, o que suaviza a resposta sem custar mais c\u00e1lculo."),
            u ("Na sa\u00edda, OUTPUT controla um ganho de 0 a 3 e tamb\u00e9m o Spiral: acima de "
               "0,33 aproximadamente, o sinal come\u00e7a a ser comprimido e depois moldado por "
               "sen(x\u00b7|x|), com uma camada extra de \u201cphat\u201d mais adiante. Em 0,33 o ganho "
               "fica em 0,99, praticamente unit\u00e1rio. Lat\u00eancia: 0 amostras.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("AGUDOS, M\u00c9DIOS e GRAVES t\u00eam 0,50 como neutro. A lei interna vai de \u22126 a "
               "+6 dB; o mostrador do original anuncia \u221212 a +12 dB, mas esse r\u00f3tulo n\u00e3o "
               "corresponde \u00e0 conta (A\u00b712\u22126). Mantivemos a lei e n\u00e3o o r\u00f3tulo: a "
               "faixa real \u00e9 de \u22126 a +6 dB por banda."),
            u ("FREQ. ALTA e FREQ. GRAVE movem o ponto de separa\u00e7\u00e3o do crossover do EQ. "
               "PASSA-BAIXOS corta os agudos a partir de 1,00 para baixo; PASSA-ALTOS corta os "
               "graves a partir de 0,00 para cima. Os dois s\u00e3o filtros de v\u00e1rios polos: "
               "quanto mais longe do padr\u00e3o, mais \u00edngreme o corte."),
            u ("COMPRESS\u00c3O define o ganho de entrada do ButterComp; em 0 o compressor nem "
               "processa. VELOCIDADE escolhe qu\u00e3o r\u00e1pido o envelope segue o sinal \u2014 em 0 o "
               "divisor \u00e9 m\u00ednimo (mais r\u00e1pido). SA\u00cdDA acima de 0,33 entra no Spiral e "
               "aquece o resultado.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para um ponto de partida neutro: AGUDOS, M\u00c9DIOS e GRAVES em 0,50, "
               "PASSA-BAIXOS em 1,00, PASSA-ALTOS em 0,00, COMPRESS\u00c3O e VELOCIDADE em 0,00 e "
               "SA\u00cdDA em 0,33. Duplo clique devolve cada controle ao padr\u00e3o; as setas fazem "
               "ajuste fino. O medidor de redu\u00e7\u00e3o mostra quanto o ButterComp est\u00e1 "
               "segurando. BYPASS desvia o strip com rampa de 10 ms e os medidores continuam "
               "ativos."),
            u ("Desvios do adaptador: s\u00f3 medidores, leitura de redu\u00e7\u00e3o de ganho, bypass "
               "suavizado e a dither de 64 bits comentada (o RNG continua avan\u00e7ando). As duas "
               "escalas do original \u2014 taxa de amostragem bruta nos IIRs e SR/44100 no "
               "compressor \u2014 foram preservadas exatamente como est\u00e3o.")
        }
    });

    info.controls.push_back ({ "Agudos", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda alta, de \u22126 a +6 dB; 0,50 \u00e9 neutro.") });
    info.controls.push_back ({ u ("M\u00e9dios"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda m\u00e9dia, de \u22126 a +6 dB.") });
    info.controls.push_back ({ "Graves", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda grave, de \u22126 a +6 dB.") });
    info.controls.push_back ({ "Freq. agudos", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ponto de separa\u00e7\u00e3o entre agudos e m\u00e9dios no crossover.") });
    info.controls.push_back ({ "Freq. graves", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ponto de separa\u00e7\u00e3o entre m\u00e9dios e graves.") });
    info.controls.push_back ({ "Passa-baixos", u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("Corta os agudos; 1,00 deixa o filtro aberto.") });
    info.controls.push_back ({ "Passa-altos", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Corta os graves; 0,00 deixa o filtro aberto.") });
    info.controls.push_back ({ u ("Compress\u00e3o"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Ganho de entrada do ButterComp. Em 0 o compressor n\u00e3o age.") });
    info.controls.push_back ({ "Velocidade", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Tempo do envelope do compressor; 0 \u00e9 o mais r\u00e1pido.") });
    info.controls.push_back ({ u ("Sa\u00edda"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,33"),
                               u ("Ganho de sa\u00edda; acima de 0,33 ativa o Spiral.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows CStrip2";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do CStrip2 "
                               "est\u00e1gio por est\u00e1gio. Acrescenta medidores de pico, leitura de "
                               "redu\u00e7\u00e3o de ganho, bypass suavizado e mant\u00e9m as duas escalas "
                               "do original. A dither de 64 bits est\u00e1 comentada no pr\u00f3prio "
                               "original; s\u00f3 o RNG avan\u00e7a.");

    return info;
}

} // namespace forja::bigorna
