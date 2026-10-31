#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::repique
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "REPIQUE";
    info.subtitle = u ("DELAY DE FITA \u00b7 FORJA");
    info.tagline = u ("Repique \u00e9 o eco de uma m\u00e1quina de fita: a c\u00f3pia volta com o "
                      "filtro do cabe\u00e7ote, com a velocidade oscilando e com uma regenera\u00e7\u00e3o "
                      "que vira resson\u00e2ncia quando o filtro aperta. O atraso \u00e9 musical, "
                      "n\u00e3o compensa\u00e7\u00e3o de lat\u00eancia.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Repique \u00e9 um delay cujo cora\u00e7\u00e3o \u00e9 o TapeDelay2 da Airwindows. Ele "
               "n\u00e3o \u00e9 um delay digital limpo: cada repeti\u00e7\u00e3o passa por um filtro "
               "ressonante, e o tempo pode ser modulado por flutter. Com regenera\u00e7\u00e3o alta e "
               "filtro fechado, o eco vira um tom; com regenera\u00e7\u00e3o baixa, \u00e9 um slapback "
               "de fita."),
            u ("O controle de tempo age ao contr\u00e1rio do que se espera: ele define a velocidade "
               "da cabe\u00e7a de leitura, ent\u00e3o 1,00 \u00e9 o atraso mais curto (~70 ms) e 0,00 o "
               "mais longo (~1,8 s em 48 kHz).")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O delay \u00e9 um anel de 88200 amostras. A cada amostra a cabe\u00e7a recua "
               "speed posi\u00e7\u00f5es, e o trecho percorrido \u00e9 preenchido com uma interpola\u00e7\u00e3o "
               "linear entre o valor anterior e o novo. O tempo efetivo \u00e9 88200 dividido por "
               "speed. speed vem de TEMPO na quarta pot\u00eancia: 25\u00b7TEMPO\u2074 + 1, de 1 a 26."),
            u ("A amostra lida volta para a entrada somada ao sinal novo, escalada por REGEN ao "
               "quadrado. No caminho h\u00e1 um filtro de estado vari\u00e1vel: FREQU\u00caNCIA define o "
               "corte (0,0001 a 0,4 do Nyquist, via cubo) e RESSON\u00c2NCIA o amortecimento (0,01 a "
               "1,01, via quadrado). Com pouca resson\u00e2ncia o eco escurece; com muita, ele "
               "apita."),
            u ("A sa\u00edda passa por um segundo filtro com o mesmo corte, mas resson\u00e2ncia 1,618 "
               "vezes maior (\u03c6), o que deixa o eco mais aberto do que o caminho de "
               "realimenta\u00e7\u00e3o."),
            u ("FLUTTER \u00e9 uma modula\u00e7\u00e3o de velocidade. A profundidade cresce com a quinta "
               "pot\u00eancia do controle e com o tempo; a fase da senoide avan\u00e7a com o quadrado "
               "do sinal de entrada, ent\u00e3o material mais denso acelera o vibrato. Em 0 a "
               "oscila\u00e7\u00e3o some e o delay fica est\u00e1vel."),
            u ("Acima de 44,1 kHz o original processa o n\u00facleo em ciclos (2\u00d7 em 88,2/96 kHz, "
               "4\u00d7 em 176/192 kHz) e interpola as amostras intermedi\u00e1rias com m\u00e9dias "
               "sucessivas. O adaptador reproduz exatamente esses ciclos.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("TEMPO \u00e9 a velocidade da cabe\u00e7a: 1,00 deixa o eco em ~70 ms e 0,00 em ~1,8 s "
               "(em 48 kHz). Girar o controle durante o som muda o tom do eco, como numa fita "
               "sendo esticada; n\u00e3o h\u00e1 suaviza\u00e7\u00e3o de tempo porque o original tamb\u00e9m "
               "n\u00e3o tem."),
            u ("SYNC faz o Tempo seguir o BPM do host e, quando o host informa um BPM "
               "v\u00e1lido, desativa o knob. DIVIS\u00c3O escolhe "
               "1/16, 1/8, 1/4, 1/2, 1/1 ou 2/1; o adaptador converte a divis\u00e3o em segundos e "
               "resolve a velocidade necess\u00e1ria. Divis\u00f5es mais longas que o anel permitem "
               "(\u2248 2 s em 44,1 kHz) aparecem com \"m\u00e1x\" na leitura. Sem BPM do host, o "
               "SYNC n\u00e3o interfere e o Tempo manual continua valendo."),
            u ("REGEN \u00e9 o quanto do eco volta para dentro do filtro, elevado ao quadrado. Em 0 "
               "h\u00e1 uma repeti\u00e7\u00e3o s\u00f3; perto de 1,00 o eco se sustenta. FREQU\u00caNCIA e "
               "RESSON\u00c2NCIA moldam esse caminho: feche a frequ\u00eancia e suba a resson\u00e2ncia "
               "para o eco virar nota."),
            u ("FLUTTER em 0 desliga a modula\u00e7\u00e3o. MISTURA \u00e9 o dry/wet do original: 1,00 "
               "deixa s\u00f3 o eco, 0,50 deixa o seco cheio somado ao eco cheio (submix, n\u00e3o "
               "crossfade) e abaixo de 0,50 o seco domina.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para slapback de fita, TEMPO perto de 0,8, REGEN em 0,2, FLUTTER em 0,1 e MISTURA "
               "em 0,5. Para um eco ressonante, baixe FREQU\u00caNCIA para 0,2 para 0,35, suba "
               "RESSON\u00c2NCIA e mantenha REGEN abaixo de 0,85. Para dub, REGEN alto com "
               "FLUTTER m\u00e9dio e filtro fechado; monitore o medidor para n\u00e3o estourar."),
            u ("O atraso interno n\u00e3o entra no PDC: o host n\u00e3o desloca a faixa. BYPASS desvia "
               "o DSP com rampa de 10 ms e os medidores continuam ativos. O adaptador processa "
               "em double e n\u00e3o aloca no caminho de \u00e1udio; o anel de 88200 amostras \u00e9 "
               "reservado na constru\u00e7\u00e3o."),
            u ("Com SYNC ligado, a divis\u00e3o \u00e9 medida pela batida do host: 1/4 em 120 BPM "
               "d\u00e1 500 ms de eco. O adaptador mant\u00e9m o anel fixo, ent\u00e3o o tempo m\u00e1ximo "
               "depende da taxa de amostragem; o host continua sem compensa\u00e7\u00e3o de lat\u00eancia.")
        }
    });

    info.controls.push_back ({ "Tempo", u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("Velocidade da cabe\u00e7a: 1,00 \u2248 70 ms, 0,00 \u2248 1,8 s.") });
    info.controls.push_back ({ u ("Regenera\u00e7\u00e3o"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Retorno do eco, ao quadrado; em 0 h\u00e1 s\u00f3 uma repeti\u00e7\u00e3o.") });
    info.controls.push_back ({ u ("Frequ\u00eancia"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Corte do filtro no caminho de realimenta\u00e7\u00e3o.") });
    info.controls.push_back ({ u ("Resson\u00e2ncia"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Amortecimento do filtro; alto vira tom.") });
    info.controls.push_back ({ "Flutter", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Profundidade da modula\u00e7\u00e3o de velocidade.") });
    info.controls.push_back ({ "Mistura", u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("1,00 s\u00f3 eco, 0,50 seco + eco cheio, abaixo de 0,50 s\u00f3 seco.") });
    info.controls.push_back ({ "Sync", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Tempo segue o BPM do host; o knob de Tempo fica inativo.") });
    info.controls.push_back ({ "Divis\u00e3o", u ("6 posi\u00e7\u00f5es \u00b7 padr\u00e3o 1/4"),
                               u ("1/16, 1/8, 1/4, 1/2, 1/1 ou 2/1; s\u00f3 vale com Sync ligado.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows TapeDelay2";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do TapeDelay2: "
                               "anel de 88200 amostras com preenchimento interpolado, filtros de "
                               "realimenta\u00e7\u00e3o e sa\u00edda, flutter por sinal e ciclos de "
                               "sobreamostragem. Acrescenta medidores de pico, bypass suavizado, "
                               "leitura de clipe e o Sync de BPM do host (com o Sync desligado o "
                               "DSP \u00e9 id\u00eantico ao original); a dither de 64 bits est\u00e1 comentada "
                               "l\u00e1, s\u00f3 o RNG avan\u00e7a. O anel mora no heap porque o tipo tamb\u00e9m \u00e9 "
                               "criado na pilha pelo harness de paridade.");

    return info;
}

} // namespace forja::repique
