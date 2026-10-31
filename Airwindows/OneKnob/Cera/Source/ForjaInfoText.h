#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::cera
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "CERA";
    info.subtitle = u ("CALOR ASSIM\u00c9TRICO \u00b7 FORJA");
    info.tagline = u ("CERA derrete o sinal com uma curva assim\u00e9trica e devolve a "
                      "diferen\u00e7a em derivadas: o resultado \u00e9 calor de valvula sem "
                      "perda de ataque. O ponto neutro \u00e9 exatamente 0,50.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O PurestWarm3 n\u00e3o usa uma curva de satura\u00e7\u00e3o comum. Ele desloca o "
               "sinal de um lado, aplica uma curva suave, desfaz o deslocamento e mede a "
               "diferen\u00e7a entre a entrada e a sa\u00edda. Essa diferen\u00e7a \u00e9 "
               "guardada em uma cadeia de derivadas e devolvida como um pequeno refor\u00e7o "
               "de harm\u00f4nicos."),
            u ("Como a curva \u00e9 assim\u00e9trica, os dois lados da onda n\u00e3o recebem o "
               "mesmo tratamento: \u00e9 da\u00ed que vem o calor par. E como o refor\u00e7o "
               "\u00e9 feito por diferen\u00e7a de derivadas, o ataque continua presente.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O controle vira `asym = (CALOR \u2212 0,50) \u00b7 8`. Com 0,50 o fator \u00e9 "
               "zero e o bloco inteiro \u00e9 pulado \u2014 o plugin fica transparente. Abaixo "
               "de 0,50 a curva pende para um lado; acima, para o outro. Os dois lados s\u00e3o "
               "\u00fatil e nenhum \u00e9 \u201cmais correto\u201d que o outro."),
            u ("A diferen\u00e7a passa por quatro derivadas e o ganho final usa tr\u00eas "
               "coeficientes fixos (0,0618, \u22120,05982 e 0,0206). \u00c9 esse jogo que d\u00e1 "
               "o timbre de cera: pouca distor\u00e7\u00e3o residual, muita sensa\u00e7\u00e3o de "
               "corpo.")
        }
    });

    info.sections.push_back ({
        u ("O controle"),
        {
            u ("CALOR em 0,50 \u00e9 neutro. 0,35 e 0,65 s\u00e3o os ajustes de uso, cada um "
               "com um lado do calor; 0,20 e 0,80 j\u00e1 s\u00e3o colora\u00e7\u00e3o forte. "
               "N\u00e3o existe ganho de compensa\u00e7\u00e3o no original nem no adaptador."),
            u ("O FUNDAMENTOS chama o par\u00e2metro de ASIM (Asym), o nome do original.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Em voz, 0,42 ou 0,58, sempre depois do compressor. Em mix bus, 0,45 s\u00f3 "
               "para colar. Em baixo, 0,60 para engrossar o lado positivo. BYPASS desvia o DSP "
               "com rampa de 10 ms; o processamento \u00e9 em double, sem aloca\u00e7\u00e3o e "
               "sem PDC.")
        }
    });

    info.controls.push_back ({ "Calor", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Asym = (CALOR \u2212 0,50) \u00b7 8; 0,50 = neutro.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows PurestWarm3";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do "
                               "PurestWarm3: o deslocamento assim\u00e9trico, a curva "
                               "c\u00fabica, a cadeia de quatro derivadas e o refor\u00e7o "
                               "final. Acrescenta medidores de pico, bypass suavizado e "
                               "leitura de clipe; a dither do original n\u00e3o \u00e9 "
                               "injetada, s\u00f3 o RNG avan\u00e7a. Nenhuma aloca\u00e7\u00e3o "
                               "dentro de process().");

    return info;
}

} // namespace forja::cera
