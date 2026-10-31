#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::impeto
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = u ("\u00cdMPETO");
    info.subtitle = u ("COMPRESS\u00c3O R\u00c1PIDA \u00b7 FORJA");
    info.tagline = u ("\u00cdMPETO \u00e9 compress\u00e3o sem envelope: a quarta pot\u00eancia do "
                      "sinal vira a pr\u00f3pria curva, ent\u00e3o o pico \u00e9 dobrado para "
                      "dentro instantaneamente, sem ataque, release ou detector.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O Dyno parte do mesmo ganho de entrada do CORPO, mas troca a curva de raiz "
               "quarta por uma curva de quarta pot\u00eancia. Quanto maior o sample, mais forte "
               "a curvatura; samples pequenos passam quase intactos. O resultado \u00e9 uma "
               "compress\u00e3o de forma de onda, sample a sample."),
            u ("Como n\u00e3o existe detector nem envelope, n\u00e3o h\u00e1 bombeamento nem "
               "ataque para ajustar: o pico \u00e9 limitado no mesmo instante em que aparece. "
               "O pre\u00e7o \u00e9 distor\u00e7\u00e3o harm\u00f4nica nos picos altos, que \u00e9 "
               "justamente o car\u00e1ter do plugin.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O ganho \u00e9 `10^((controle \u00b7 24 \u2212 12) / 20)`. Em seguida o original "
               "calcula `dyno = |x|^4` e aplica `sin(x \u00b7 dyno) / dyno \u00b7 1,1654321`. "
               "O fator 1,1654321 rep\u00f5e parte do n\u00edvel que a curva consome."),
            u ("Sinais abaixo de aproximadamente 0,5 quase n\u00e3o sentem a curva; acima disso "
               "ela aperta r\u00e1pido. Em 1,0 o sample inteiro \u00e9 remodelado. N\u00e3o h\u00e1 "
               "estado al\u00e9m do RNG.")
        }
    });

    info.sections.push_back ({
        u ("O controle"),
        {
            u ("O padr\u00e3o 0,50 corresponde a 0,0 dB de ganho, com a curva atuando. Para "
               "segurar picos sem perder corpo, 0,55 a 0,65; para um efeito mais agressivo, "
               "0,75 ou mais. Abaixo de 0,45 a curva quase n\u00e3o aparece."),
            u ("N\u00e3o h\u00e1 ganho de compensa\u00e7\u00e3o nem tempo de release: o controle "
               "\u00e9 a quantidade de curva, n\u00e3o a velocidade.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Em voz, 0,55 para controlar picos de consoante. Em bateria, 0,60 a 0,70 para "
               "colar o transiente. Em baixo, 0,65. BYPASS desvia o DSP com rampa de 10 ms; o "
               "processamento \u00e9 em double, sem aloca\u00e7\u00e3o e sem PDC.")
        }
    });

    info.controls.push_back ({ u ("\u00cdmpeto"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho de entrada de \u221212 a +12 dB antes da curva.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows Dyno";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do Dyno: o "
                               "ganho de entrada, a quarta pot\u00eancia do m\u00f3dulo, a curva "
                               "de seno e o fator 1,1654321. Acrescenta medidores de pico, "
                               "bypass suavizado e leitura de clipe; nenhuma aloca\u00e7\u00e3o "
                               "dentro de process().");

    return info;
}

} // namespace forja::impeto
