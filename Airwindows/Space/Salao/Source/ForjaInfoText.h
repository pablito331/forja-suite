#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::salao
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = u ("SAL\u00c3O");
    info.subtitle = u ("SALA GRANDE \u00b7 FORJA");
    info.tagline = u ("SAL\u00c3O \u00e9 uma sala de concerto, n\u00e3o um reverb de placa. O "
                      "kRockstar2 junta reflex\u00f5es iniciais curtas, um corpo de "
                      "realimenta\u00e7\u00e3o de seis vias por canal e um piso de ru\u00eddo "
                      "de sala. O atraso interno \u00e9 musical: n\u00e3o h\u00e1 compensa\u00e7\u00e3o "
                      "de lat\u00eancia.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O sinal entra por uma rede de reflex\u00f5es iniciais 3\u00d73 por canal: "
               "nove linhas curtas com tempos tirados de uma tabela de posi\u00e7\u00f5es "
               "da sala. Essa rede d\u00e1 a primeira impress\u00e3o de tamanho e de "
               "dist\u00e2ncia antes de o corpo da sala abrir."),
            u ("O corpo \u00e9 uma matriz de realimenta\u00e7\u00e3o 6\u00d76 por canal, "
               "repetida em cinco est\u00e1gios: trinta e seis linhas de atraso por canal. "
               "Cada est\u00e1gio soma as seis sa\u00eddas com pesos cruzados, o que espalha "
               "a energia no tempo e no espa\u00e7o sem precisar de um algoritmo de "
               "convolu\u00e7\u00e3o.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("REGENERA\u00c7\u00c3O entra como (1 \u2212 (1 \u2212 A)\u00b3) \u00b7 0,0078125. "
               "O cubo faz o controle come\u00e7ar devagar e terminar r\u00e1pido; o fator "
               "1/128 \u00e9 compensado pela matriz, que devolve ganho a cada passagem."),
            u ("POSI\u00c7\u00c3O escolhe um \u00edndice na tabela de reflex\u00f5es: "
               "E \u00b7 27 posi\u00e7\u00f5es, das mais curtas (~2 ms) \u00e0s mais longas "
               "(~193 ms). Trocar a posi\u00e7\u00e3o muda a geometria sentida da sala."),
            u ("DOWNREZ reduz a resolu\u00e7\u00e3o interna do corpo com uma subamostragem "
               "por B\u00e9zier; abaixo de 1,00 a cauda ganha degraus e textura lo-fi. "
               "DIST\u00c2NCIA empilha at\u00e9 25 est\u00e1gios de m\u00e9dia de um polo, "
               "escurecendo e afastando a fonte conforme o valor sobe."),
            u ("O original gera um piso de ru\u00eddo de sala muito baixo em mono est\u00e9reo "
               "assim\u00e9trico, alimentado pelo mesmo RNG do dither. Ele mant\u00e9m a "
               "cauda \u201cviva\u201d e evita que a sala desapare\u00e7a em sil\u00eancio "
               "digital.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("REGEN em 0,50 j\u00e1 sustenta uma cauda longa; acima de 0,85 a sala fica "
               "quase infinita. POSITIN em 0,75 \u00e9 o padr\u00e3o do original, uma sala "
               "grande e equilibrada."),
            u ("DOWNREZ em 1,00 mant\u00e9m a resolu\u00e7\u00e3o plena. DIST\u00c2NCIA em "
               "0,25 j\u00e1 afasta a fonte do ouvinte; em 0,00 a fonte fica \u00e0 frente "
               "da sala. MISTURA em 0,50 \u00e9 metade molhado; em 1,00 s\u00f3 a sala sai.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para voz, POSI\u00c7\u00c3O 0,60, REGEN 0,45, DIST\u00c2NCIA 0,20 e MISTURA "
               "0,30. Para bateria, POSI\u00c7\u00c3O 0,80, REGEN 0,55 e MISTURA 0,35. Para "
               "uma sala lo-fi, DOWNREZ 0,45 e DIST\u00c2NCIA 0,60."),
            u ("O tempo interno n\u00e3o entra no PDC: o host n\u00e3o desloca a faixa. "
               "BYPASS desvia o DSP com rampa de 10 ms e os medidores continuam ativos. O "
               "adaptador processa em double e n\u00e3o aloca no caminho de \u00e1udio; as "
               "noventa linhas de atraso s\u00e3o reservadas na constru\u00e7\u00e3o.")
        }
    });

    info.controls.push_back ({ "Regenera\u00e7\u00e3o", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("(1 \u2212 (1 \u2212 A)\u00b3) \u00b7 0,0078125; sustenta a cauda.") });
    info.controls.push_back ({ u ("Posi\u00e7\u00e3o"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,75"),
                               u ("Escolhe a geometria de reflex\u00f5es da sala.") });
    info.controls.push_back ({ "DownRez", u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("Resolu\u00e7\u00e3o interna do corpo da sala.") });
    info.controls.push_back ({ u ("Dist\u00e2ncia"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,25"),
                               u ("At\u00e9 25 est\u00e1gios de filtro de dist\u00e2ncia.") });
    info.controls.push_back ({ "Mistura", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Seco/molhado linear; 1,00 = s\u00f3 sala.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows kRockstar2";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do "
                               "kRockstar2: a rede de reflex\u00f5es iniciais, o corpo de "
                               "seis vias em cinco est\u00e1gios, os filtros de dist\u00e2ncia "
                               "de 25 est\u00e1gios, a reconstru\u00e7\u00e3o por B\u00e9zier e "
                               "o piso de ru\u00eddo de sala. Acrescenta medidores de pico, "
                               "bypass suavizado e leitura de clipe. A dither em si est\u00e1 "
                               "comentada l\u00e1; s\u00f3 o RNG avan\u00e7a. As linhas moram no "
                               "heap porque o tipo tamb\u00e9m \u00e9 criado na pilha pelo "
                               "harness de paridade.");

    return info;
}

} // namespace forja::salao
