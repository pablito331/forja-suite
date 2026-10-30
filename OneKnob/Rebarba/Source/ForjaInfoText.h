#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::rebarba
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "REBARBA";
    info.subtitle = u ("SUAVIZA\u00c7\u00c3O DE SLEW \u00b7 FORJA");
    info.tagline = u ("REBARBA lima as arestas da onda: um preditor de tr\u00eas amostras "
                      "estima o pr\u00f3ximo passo e o limitador corta o que passa do "
                      "limite. \u00c9 o removedor de rebarba do sinal digital.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O Slew3 olha as tr\u00eas \u00faltimas amostras e estima para onde a onda ia. "
               "Se o pr\u00f3ximo sample pular mais do que o limite permitido, o valor \u00e9 "
               "puxado para perto do anterior. O resultado \u00e9 uma onda com os cantos "
               "arredondados, sem mudar o n\u00edvel m\u00e9dio."),
            u ("Serve para tirar aspereza de material digital, amaciar transientes de "
               "programa\u00e7\u00e3o e reduzir aquele chiado de aresta em sintetizadores. "
               "N\u00e3o \u00e9 filtro de agudos: o limite \u00e9 de passo, n\u00e3o de "
               "frequ\u00eancia.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O limite \u00e9 `(1 \u2212 SUAVIDADE)^4 / escala`, ent\u00e3o em 0,00 ele fica "
               "em 1,0 (quase nenhum clamp) e em 1,00 cai a zero (clamp m\u00e1ximo). A "
               "predi\u00e7\u00e3o usa os coeficientes 0,381966 e 0,618033 \u2014 a propor\u00e7\u00e3o "
               "\u00e1urea \u2014 para estimar o pr\u00f3ximo ponto a partir de tr\u00eas "
               "amostras."),
            u ("Depois do clamp, o sinal ainda passa por uma m\u00e9dia com os mesmos pesos, o "
               "que alisa a transi\u00e7\u00e3o. N\u00e3o h\u00e1 buffer nem atraso declarado: tr\u00eas "
               "amostras de estado por canal.")
        }
    });

    info.sections.push_back ({
        u ("O controle"),
        {
            u ("Em 0,00 o plugin \u00e9 praticamente transparente. Em 0,25 j\u00e1 limpa "
               "arestas de sintetizador; em 0,50 amacia bateria e voz; acima de 0,75 vira "
               "efeito e pode apagar transientes."),
            u ("O FUNDAMENTOS chama o par\u00e2metro de CLAMPING, o nome do original.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Use no fim da cadeia, depois da satura\u00e7\u00e3o, para tirar a rebarba que "
               "o drive deixou. Em voz, 0,25; em mix bus, 0,15; em efeito criativo, 0,70. "
               "BYPASS desvia o DSP com rampa de 10 ms; o processamento \u00e9 em double, sem "
               "aloca\u00e7\u00e3o e sem PDC.")
        }
    });

    info.controls.push_back ({ u ("Suavidade"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Limite de passo; 0 = quase transparente, 1 = m\u00e1ximo.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows Slew3";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do Slew3: "
                               "o preditor de tr\u00eas amostras com pesos \u00e1ureos, o "
                               "clamp de passo e a m\u00e9dia final. Acrescenta medidores de "
                               "pico, bypass suavizado e leitura de clipe; nenhuma "
                               "aloca\u00e7\u00e3o dentro de process().");

    return info;
}

} // namespace forja::rebarba
