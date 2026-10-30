#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::eco
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "ECO";
    info.subtitle = u ("ESPA\u00c7O DE ECO \u00b7 FORJA");
    info.tagline = u ("Eco n\u00e3o \u00e9 um delay com um tempo: \u00e9 uma sala inteira. O "
                      "Echojam desenha dezessete espa\u00e7os fixos, cada um com seu "
                      "pr\u00f3prio conjunto de tempos de parede, e o Forja os exp\u00f5e num "
                      "s\u00f3 seletor. O atraso \u00e9 musical, n\u00e3o compensa\u00e7\u00e3o "
                      "de lat\u00eancia.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("ECO \u00e9 o retorno de uma sala: dezesseis taps por canal, cada um com "
               "atraso pr\u00f3prio, somados por uma matriz de realimenta\u00e7\u00e3o que "
               "espalha a energia entre os canais. Nada aqui \u00e9 um delay \u00fanico; o "
               "resultado \u00e9 uma cauda densa de espa\u00e7o, com car\u00e1ter de sala "
               "grande e piso de ru\u00eddo de fita."),
            u ("Os dezessete espa\u00e7os s\u00e3o fixos no original: cada um troca os "
               "dezesseis tempos de atraso de uma vez. Selecionar outro espa\u00e7o limpa "
               "as linhas, como mudar de sala.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo s\u00e3o dezesseis linhas de atraso por canal (A a P). A cada "
               "passagem do ciclo, as oito linhas \u00e0 esquerda recebem a soma dos retornos "
               "da direita e vice-versa; a matriz usa o dobro de cada tap menos a soma dos "
               "quatro vizinhos, o que coloca a energia fora de fase e abre o espa\u00e7o."),
            u ("REGENERA\u00c7\u00c3O entra como (1 \u2212 (1 \u2212 B)\u00b2) \u00b7 0,0625. "
               "O fator 1/16 \u00e9 proposital: o ganho aparente vem da matriz de "
               "realimenta\u00e7\u00e3o, n\u00e3o do controle. Mesmo em 1,00 o espa\u00e7o "
               "sustenta sem explodir."),
            u ("VELOCIDADE \u00e9 uma redu\u00e7\u00e3o de taxa dentro do algoritmo: "
               "derez = C elevado a (taxa / 44100). O ciclo vira uma fra\u00e7\u00e3o inteira "
               "de amostra e a entrada \u00e9 acumulada no ciclo. Na sa\u00edda, o original "
               "reconstr\u00f3i o sinal com uma curva de B\u00e9zier entre as \u00faltimas "
               "amostras, por isso em velocidades baixas o eco fica mais escuro e "
               "granulado, mas sem degraus."),
            u ("MISTURA \u00e9 wet = 1 \u2212 (1 \u2212 D)\u00b2 aplicado sobre seco e "
               "molhado juntos: em 0,50 j\u00e1 h\u00e1 75 % de espa\u00e7o. N\u00e3o \u00e9 "
               "meio a meio.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("ESPA\u00c7O escolhe um dos dezessete conjuntos de tempos. O tap mais curto "
               "vai de ~1 a ~3 ms e o mais longo de ~419 a ~519 ms, conforme a sala. A "
               "legenda do seletor mostra a faixa de tempo e o n\u00famero de lugares, como "
               "no original."),
            u ("REGENERA\u00c7\u00c3O em 0 deixa uma passagem \u00fanica; perto de 1,00 a "
               "cauda dura muito. VELOCIDADE reduz a resolu\u00e7\u00e3o interna; abaixo de "
               "0,25 o espa\u00e7o vira um lo-fi de sala. MISTURA em 1,00 deixa s\u00f3 o "
               "espa\u00e7o, em 0,00 s\u00f3 o seco."),
            u ("SYNC faz a Velocidade seguir o BPM do host e, quando o host informa um BPM "
               "v\u00e1lido, desativa o knob. A "
               "DIVIS\u00c3O alinha a parede mais longa do espa\u00e7o selecionado \u00e0 "
               "divis\u00e3o escolhida: em 120 BPM, 1/4 pede 500 ms. Quando a parede n\u00e3o "
               "alcan\u00e7a o valor pedido, a leitura mostra o tempo efetivo com \"m\u00e1x\". "
               "Sem BPM do host, o SYNC n\u00e3o interfere e a Velocidade manual continua "
               "valendo."),
            u ("Trocar de espa\u00e7o corta a cauda atual, porque as linhas s\u00e3o "
               "zeradas. \u00c9 o comportamento do original; n\u00e3o h\u00e1 crossfade "
               "entre salas.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para um ambiente de voz, ESPA\u00c7O 07 ou 09, REGEN em 0,35, VELOCIDADE "
               "em 0,75 e MISTURA em 0,35. Para uma sala grande de bateria, ESPA\u00c7O 14 "
               "com REGEN em 0,5. Para um eco distintamente lo-fi, VELOCIDADE em 0,4 e "
               "MISTURA em 0,4."),
            u ("O atraso interno n\u00e3o entra no PDC: o host n\u00e3o desloca a faixa. "
               "BYPASS desvia o DSP com rampa de 10 ms e os medidores continuam ativos. O "
               "adaptador processa em double e n\u00e3o aloca no caminho de \u00e1udio; as "
               "trinta e duas linhas de atraso s\u00e3o reservadas na constru\u00e7\u00e3o.")
        }
    });

    info.controls.push_back ({ u ("Espa\u00e7o"), u ("17 posi\u00e7\u00f5es \u00b7 padr\u00e3o 09"),
                               u ("Conjunto fixo de tempos de parede e lugares da sala.") });
    info.controls.push_back ({ u ("Regenera\u00e7\u00e3o"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("(1 \u2212 (1 \u2212 B)\u00b2) \u00b7 0,0625; sustenta a cauda.") });
    info.controls.push_back ({ "Velocidade", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Redu\u00e7\u00e3o de taxa interna: C^(taxa/44100).") });
    info.controls.push_back ({ "Mistura", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("1 \u2212 (1 \u2212 D)\u00b2; 0,50 j\u00e1 \u00e9 75 % molhado.") });
    info.controls.push_back ({ "Sync", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Velocidade segue o BPM do host; o knob fica inativo.") });
    info.controls.push_back ({ "Divis\u00e3o", u ("6 posi\u00e7\u00f5es \u00b7 padr\u00e3o 1/4"),
                               u ("Alinha a parede mais longa do espa\u00e7o \u00e0 divis\u00e3o.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows Echojam";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do "
                               "Echojam: dezessete espa\u00e7os fixos, dezesseis taps por "
                               "canal, matriz de realimenta\u00e7\u00e3o trocada entre canais, "
                               "reconstru\u00e7\u00e3o por B\u00e9zier e o RNG assim\u00e9trico "
                               "do dither. Acrescenta medidores de pico, bypass suavizado, "
                               "leitura de clipe e o Sync de BPM do host (com o Sync desligado "
                               "o DSP \u00e9 id\u00eantico ao original); a dither em si est\u00e1 comentada "
                               "l\u00e1, s\u00f3 o RNG avan\u00e7a. As linhas moram no heap porque o tipo "
                               "tamb\u00e9m \u00e9 criado na pilha pelo harness de paridade.");

    return info;
}

} // namespace forja::eco
