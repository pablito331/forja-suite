#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::folego
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = u ("F\u00d4LEGO");
    info.subtitle = u ("AMBIENTE DE CLUBE \u00b7 FORJA");
    info.tagline = u ("F\u00d4LEGO \u00e9 o ambiente de um clube de 272 lugares: reflex\u00f5es "
                      "iniciais curtas, corpo de seis vias por canal e dois est\u00e1gios de "
                      "subamostragem por B\u00e9zier. \u00c9 o par pequeno do SAL\u00c3O. O atraso "
                      "interno \u00e9 musical, n\u00e3o compensa\u00e7\u00e3o de lat\u00eancia.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O sinal entra por uma rede 3\u00d73 de reflex\u00f5es iniciais por canal, "
               "com tempos tirados de uma tabela de 36 posi\u00e7\u00f5es de um clube de 272 "
               "lugares (de ~1 a ~93 ms). Essas reflex\u00f5es d\u00e3o o tamanho do ambiente "
               "sem empurrar a fonte para longe."),
            u ("O corpo \u00e9 uma matriz 6\u00d76 por canal repetida em cinco est\u00e1gios, "
               "trinta e seis linhas de atraso por canal. \u00c9 a mesma fam\u00edlia do "
               "SAL\u00c3O, com sala menor, coer\u00eancia mais curta e cauda mais controlada.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("REGENERA\u00c7\u00c3O entra como (1 \u2212 (1 \u2212 A)\u00b2) \u00b7 fdb6ck, "
               "com fdb6ck \u2248 0,001302. O fator pequeno \u00e9 compensado pela matriz, que "
               "devolve ganho a cada passagem; por isso 0,50 j\u00e1 sustenta uma cauda aud\u00edvel."),
            u ("DEREZ controla a resolu\u00e7\u00e3o do corpo. At\u00e9 0,50 a subamostragem \u00e9 "
               "em degraus exatos; acima de 0,50 o original muda para o modo cont\u00ednuo, que "
               "casa as curvas de B\u00e9zier com a taxa. O meio (0,50) \u00e9 a fronteira entre "
               "os dois modos, n\u00e3o um ponto neutro."),
            u ("FILTRO faz a mesma divis\u00e3o, mas dentro da subamostragem: h\u00e1 um segundo "
               "B\u00e9zier dedicado ao filtro, por isso o controle escurece a cauda sem "
               "engrossar o ataque. REFLEX\u00d5ES escala o peso das primeiras reflex\u00f5es."),
            u ("POSI\u00c7\u00c3O escolhe o in\u00edcio da janela de nove tempos na tabela: "
               "E \u00b7 27 posi\u00e7\u00f5es. MISTURA \u00e9 linear entre seco e molhado.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("A posi\u00e7\u00e3o padr\u00e3o \u00e9 0,90, como no original: um clube com a fonte "
               "levemente \u00e0 frente. DEREZ e FILTRO em 0,50 ficam exatamente na fronteira "
               "dos dois modos de B\u00e9zier."),
            u ("REFLEX\u00d5ES em 0,50 mant\u00e9m o equil\u00edbrio do original. Em 0,00 s\u00f3 "
               "o corpo da sala aparece; em 1,00 as reflex\u00f5es dominam o ataque. MISTURA em "
               "0,50 j\u00e1 \u00e9 metade molhado.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para um ambiente de voz, POSI\u00c7\u00c3O 0,70, REGEN 0,45, FILTRO 0,55 e "
               "MISTURA 0,28. Para um clube de bateria, REGEN 0,60 e REFLEX\u00d5ES 0,65. "
               "Para um ambiente lo-fi, DEREZ 0,30 e FILTRO 0,70."),
            u ("O tempo interno n\u00e3o entra no PDC: o host n\u00e3o desloca a faixa. O "
               "original injeta ru\u00eddo de dither na entrada de sil\u00eancio, por isso a "
               "sa\u00edda nunca \u00e9 zero digital. BYPASS desvia o DSP com rampa de 10 ms e os "
               "medidores continuam ativos. O adaptador processa em double e n\u00e3o aloca no "
               "caminho de \u00e1udio.")
        }
    });

    info.controls.push_back ({ u ("Regenera\u00e7\u00e3o"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("(1 \u2212 (1 \u2212 A)\u00b2) \u00b7 fdb6ck; sustenta a cauda.") });
    info.controls.push_back ({ "Derez", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Resolu\u00e7\u00e3o do corpo; 0,50 = fronteira dos modos.") });
    info.controls.push_back ({ "Filtro", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Escurece a cauda dentro da subamostragem.") });
    info.controls.push_back ({ u ("Reflex\u00f5es"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Peso das reflex\u00f5es iniciais.") });
    info.controls.push_back ({ u ("Posi\u00e7\u00e3o"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,90"),
                               u ("Janela de nove tempos na tabela do clube.") });
    info.controls.push_back ({ "Mistura", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Seco/molhado linear; 1,00 = s\u00f3 ambiente.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows kStation";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do kStation: "
                               "as reflex\u00f5es iniciais 3\u00d73, o corpo 6\u00d76 em cinco "
                               "est\u00e1gios, o B\u00e9zier externo do corpo e o interno do "
                               "filtro, e o ru\u00eddo de dither injetado na entrada. Acrescenta "
                               "medidores de pico, bypass suavizado e leitura de clipe. A dither "
                               "em si est\u00e1 comentada l\u00e1; s\u00f3 o RNG avan\u00e7a. As "
                               "linhas moram no heap porque o tipo tamb\u00e9m \u00e9 criado na "
                               "pilha pelo harness de paridade.");

    return info;
}

} // namespace forja::folego
