#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::granalha
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = u ("GRANALHA");
    info.subtitle = u ("REDUTOR DE BITS \u00b7 FORJA");
    info.tagline = u ("GRANALHA \u00e9 o gr\u00e3o digital controlado: reduz a resolu\u00e7\u00e3o "
                      "da onda, redesenha os degraus em B\u00e9zier e deixa o brilho e os "
                      "graves decidirem onde o lo-fi morde. \u00c9 o bitcrusher da su\u00edte, "
                      "feito para sujar sem perder o corpo.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O original trabalha com dois gestos ao mesmo tempo. O primeiro \u00e9 a "
               "redu\u00e7\u00e3o de taxa por dire\u00e7\u00e3o: o c\u00f3digo conta quantos "
               "samples seguidos a onda sobe ou desce e limita quantos pontos novos ela pode "
               "receber naquele trecho. O segundo \u00e9 a reconstru\u00e7\u00e3o da forma: em vez "
               "de segurar o valor antigo, o DeRez5 interpola entre dois pontos e desenha o "
               "degrau como uma curva."),
            u ("Por isso o resultado n\u00e3o \u00e9 o \u201cbitcrush\u201d quadrado comum. Com "
               "QUADRADO em 0 o degrau vira patamar; com QUADRADO em 1 a interpola\u00e7\u00e3o "
               "fica linear e o gr\u00e3o parece anal\u00f3gico. O car\u00e1ter est\u00e1 nessa "
               "mistura entre palavra curta e curva suave.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("DEREZ entra como 1 \u2212 D^(2 + taxa/44100). Em 1,00 o fator \u00e9 zero e o "
               "sinal passa intacto; descendo, o contador de dire\u00e7\u00e3o passa a liberar "
               "menos pontos por trecho. TRIM soma (1 \u2212 T) \u00b7 16 \u00b7 escala ao mesmo "
               "contador, o que deixa a redu\u00e7\u00e3o menos dependente da dire\u00e7\u00e3o "
               "\u2014 \u00e9 o ajuste fino do gr\u00e3o."),
            u ("BRILHO \u00e9 um subamostrador de B\u00e9zier em cima, aplicado antes e depois "
               "da redu\u00e7\u00e3o; em 1,00 ele fica desligado. GRAVES usa a quarta pot\u00eancia "
               "do controle para um B\u00e9zier de baixo que subtrai metade do sinal filtrado: "
               "em 0,00 fica desligado e subindo engrossa o corpo. SA\u00cdDA \u00e9 o ganho "
               "final, depois de todo o gr\u00e3o."),
            u ("O ru\u00eddo de dither do original est\u00e1 comentado no c\u00f3digo. O adaptador "
               "mant\u00e9m o RNG avan\u00e7ando (inclusive o passo extra assim\u00e9trico do "
               "canal direito) para que a paridade com o original seja exata, mas n\u00e3o "
               "injeta o dither na sa\u00edda.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("Os padr\u00f5es s\u00e3o os do original: DEREZ 1,00, TRIM 1,00, QUADRADO 0,00, "
               "BRILHO 1,00, GRAVES 0,00 e SA\u00cdDA 1,00. Nessa posi\u00e7\u00e3o o plugin \u00e9 "
               "praticamente transparente; o gr\u00e3o s\u00f3 aparece quando DEREZ desce ou "
               "TRIM sobe."),
            u ("QUADRADO em 0,00 d\u00e1 o degrau mais duro; em 1,00 a rampa fica cont\u00ednua. "
               "BRILHO abaixo de 1,00 suaviza os agudos reconstru\u00eddos; GRAVES acima de 0,00 "
               "devolve corpo. SA\u00cdDA compensa a perda de n\u00edvel que o gr\u00e3o costuma "
               "trazer.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para lo-fi de voz, DEREZ 0,55, TRIM 0,35, QUADRADO 0,15 e SA\u00cdDA 0,90. Para "
               "bateria com gr\u00e3o curto, DEREZ 0,40 e TRIM 0,60, com BRILHO 0,70. Para "
               "destrui\u00e7\u00e3o criativa, DEREZ 0,15, QUADRADO 0,00 e GRAVES 0,30."),
            u ("O processamento \u00e9 sample a sample, sem lat\u00eancia e sem PDC. BYPASS desvia "
               "o DSP com rampa de 10 ms e os medidores continuam ativos. O adaptador processa "
               "em double, n\u00e3o aloca no caminho de \u00e1udio e limpa o estado em "
               "prepareToPlay.")
        }
    });

    info.controls.push_back ({ "Derez", u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("Redu\u00e7\u00e3o de taxa por dire\u00e7\u00e3o; 1,00 = intacto.") });
    info.controls.push_back ({ "Trim", u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("Dose extra de gr\u00e3o, menos dependente da dire\u00e7\u00e3o.") });
    info.controls.push_back ({ "Quadrado", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("0 = degrau duro; 1 = interpola\u00e7\u00e3o linear.") });
    info.controls.push_back ({ "Brilho", u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("B\u00e9zier de agudos; 1,00 = desligado.") });
    info.controls.push_back ({ "Graves", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("B\u00e9zier de baixo; 0,00 = desligado.") });
    info.controls.push_back ({ u ("Sa\u00edda"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("Ganho final depois do gr\u00e3o.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows DeRez5";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do DeRez5: "
                               "o contador de dire\u00e7\u00e3o, os dois est\u00e1gios de "
                               "B\u00e9zier de brilho e de graves, a reconstru\u00e7\u00e3o por "
                               "pontos e o ganho de sa\u00edda. Acrescenta medidores de pico, "
                               "bypass suavizado e leitura de clipe. O dither de 64 bits do "
                               "original est\u00e1 comentado l\u00e1 e continua comentado; s\u00f3 "
                               "o RNG avan\u00e7a. Nenhuma aloca\u00e7\u00e3o dentro de process().");

    return info;
}

} // namespace forja::granalha
