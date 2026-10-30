#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::prensa
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "PRENSA";
    info.subtitle = u ("COMPRESSOR VARI-MU \u00b7 FORJA");
    info.tagline = u ("Densidade e car\u00e1ter vari-mu com detector est\u00e9reo linkado. "
                      "Apressa o sinal sem perder o corpo.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Prensa \u00e9 um compressor de densidade. Ele aperta o sinal de forma musical, "
               "escurece picos e aumenta a sensa\u00e7\u00e3o de corpo. \u00c9 indicado para vocais, "
               "baixo, guitarra e bus onde se busca car\u00e1ter, n\u00e3o transpar\u00eancia.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O algoritmo vem do Pressure5 da Airwindows. Um \u00fanico detector est\u00e9reo "
               "linkado compara o maior m\u00f3dulo entre os canais e alimenta dois envelopes "
               "alternados, A e B. A cada amostra o plugin troca de envelope, o que suaviza o "
               "movimento e distribui a compress\u00e3o de forma cont\u00ednua."),
            u ("O par\u00e2metro Miado modula a curva de redu\u00e7\u00e3o de ganho e a Garra controla "
               "a velocidade com que essa modula\u00e7\u00e3o reage \u00e0 inclina\u00e7\u00e3o do sinal. "
               "Acima de aproximadamente 48 kHz um filtro ultrass\u00f4nico fixo entra no caminho "
               "para manter o comportamento do original. Um est\u00e1gio ClipOnly2 arredonda os "
               "picos no fim da cadeia."),
            u ("Lat\u00eancia: 1 amostra, por causa do buffer do ClipOnly2. O adaptador Forja "
               "atrasa o caminho seco do bypass na mesma amostra para o crossfade de 10 ms n\u00e3o "
               "virar filtro comb.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Todos os knobs s\u00e3o cont\u00ednuos de 0 a 1. O medidor GR e o gr\u00e1fico de "
               "movimento mostram a redu\u00e7\u00e3o de ganho em dB. O bot\u00e3o BYPASS desvia o DSP "
               "com rampa curta.")
        }
    });

    info.controls.push_back ({
        u ("Press\u00e3o"),
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
        u ("Define o limiar de compress\u00e3o. Em 0 o compressor quase n\u00e3o age; em 1 "
           "praticamente todo o sinal \u00e9 comprimido.")
    });
    info.controls.push_back ({
        "Velocidade",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,25"),
        u ("Controla a libera\u00e7\u00e3o do envelope. Valores baixos seguram mais o sinal; "
           "valores altos deixam o movimento mais r\u00e1pido e aberto.")
    });
    info.controls.push_back ({
        "Miado",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
        u ("Modula a curva de compress\u00e3o. \u00c9 o principal controle de car\u00e1ter: "
           "altera como a redu\u00e7\u00e3o responde ao n\u00edvel e ao movimento.")
    });
    info.controls.push_back ({
        "Garra",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
        u ("Controla a influ\u00eancia da inclina\u00e7\u00e3o do sinal sobre o Miado. Valores "
           "altos deixam a resposta mais agressiva em transientes.")
    });
    info.controls.push_back ({
        u ("Sa\u00edda"),
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
        u ("Ganho de sa\u00edda. Compensa o volume perdido na compress\u00e3o para comparar "
           "com o bypass.")
    });
    info.controls.push_back ({
        "Mistura",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
        u ("Mistura dry/wet. O caminho seco j\u00e1 sai alinhado com a lat\u00eancia do wet.")
    });
    info.controls.push_back ({
        "Bypass",
        u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
        u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms e atraso de 1 amostra no caminho seco.")
    });

    info.provenance.source = "Airwindows Pressure5";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("Licen\u00e7a MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double, inicializa "
                               "muNewSpeed e muAttack em zero (o original os deixa indefinidos), "
                               "dimensiona o buffer do ClipOnly2 com 17 posi\u00e7\u00f5es para "
                               "eliminar o acesso fora dos limites em sample rates altos e reporta "
                               "a lat\u00eancia real de 1 amostra.");

    return info;
}

} // namespace forja::prensa
