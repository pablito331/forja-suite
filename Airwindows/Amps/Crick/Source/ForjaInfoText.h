#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::crick
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "CRICK";
    info.subtitle = u ("BAIXO DE DOIS CAPTADORES \u00b7 FORJA");
    info.tagline = u ("CRICK \u00e9 um amp de baixo com dois caminhos de captador: o canal "
                      "esquerdo e o direito passam por est\u00e1gios de satura\u00e7\u00e3o e "
                      "gabinetes diferentes, como um baixo com dois captadores no mesmo amp.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O CrickBass da Airwindows parte de um filtro ultrass\u00f4nico de seis biquads "
               "e aplica seis est\u00e1gios de satura\u00e7\u00e3o em cascata. Cada canal tem sua "
               "pr\u00f3pria tabela de gabinete: o esquerdo usa uma resposta mais fechada, o "
               "direito uma mais aberta. O resultado \u00e9 um baixo gordo, com ataque "
               "definido e sem aquele chiado de distor\u00e7\u00e3o de guitarra."),
            u ("Originalmente L e R s\u00e3o dois captadores, n\u00e3o um par est\u00e9reo. Em "
               "material mono, os dois lados produzem timbres diferentes; em est\u00e9reo, "
               "cada lado mant\u00e9m o seu car\u00e1ter.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("DRIVE controla o ganho de entrada dos est\u00e1gios (`(drive + 0,5)\u00b2`) e o "
               "TOM controla o corte de graves e a abertura do gabinete. Entre os est\u00e1gios, "
               "um circuito de \u201cskew\u201d usa a derivada do sinal para dar dire\u00e7\u00e3o ao "
               "ataque, e os filtros de estado vari\u00e1vel arredondam a resposta."),
            u ("Acima de 44,1 kHz o plugin entra em modo de subamostragem: processa um "
               "ciclo a cada N amostras e interpola a sa\u00edda. Isso explica a lat\u00eancia "
               "medida: 1 amostra em 44,1/48 kHz, 3 em 88,2/96 kHz e 5 em 192 kHz. O "
               "adaptador reporta exatamente esse valor para o PDC.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("DRIVE em 0,00 \u00e9 o baixo mais limpo; 0,50 d\u00e1 corpo e compress\u00e3o "
               "natural; acima de 0,75 o som fica denso e agressivo. TOM em 0,00 fecha o "
               "topo e refor\u00e7a o grave; 0,50 \u00e9 o ponto neutro; 1,00 abre o brilho "
               "dos captadores."),
            u ("N\u00e3o h\u00e1 controle de n\u00edvel: o drive aumenta a loudness como no "
               "original. Compense no canal ou no bus.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para baixo gravado direto: DRIVE 0,45, TOM 0,40. Para rock: DRIVE 0,70, "
               "TOM 0,60. Para slap: DRIVE 0,35, TOM 0,75. O plugin j\u00e1 inclui o "
               "gabinete, ent\u00e3o n\u00e3o empilhe outro simulador. BYPASS desvia com "
               "rampa de 10 ms; processamento em double e sem aloca\u00e7\u00e3o.")
        }
    });

    info.controls.push_back ({ "Drive", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho dos seis est\u00e1gios de satura\u00e7\u00e3o.") });
    info.controls.push_back ({ "Tone", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Corte de graves e abertura dos captadores.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows CrickBass";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do "
                               "CrickBass, incluindo os biquads ultrass\u00f4nicos, o skew, os "
                               "seis est\u00e1gios, as duas tabelas de gabinete e o ClipOnly "
                               "do canal direito. Acrescenta medidores de pico, bypass "
                               "suavizado e leitura de clipe; n\u00e3o h\u00e1 aloca\u00e7\u00e3o no "
                               "caminho de \u00e1udio.");

    return info;
}

} // namespace forja::crick
