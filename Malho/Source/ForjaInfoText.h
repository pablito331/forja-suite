#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::malho
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "MALHO";
    info.subtitle = u ("COMPRESSOR DE IMPACTO \u00b7 FORJA");
    info.tagline = u ("Ataque e soltura separados, envelopes independentes por canal e um drive "
                      "que deixou o sinal na frente. Feito para bateria e bus de impacto.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Malho \u00e9 um compressor de impacto. Ele segura o pico e devolve o sinal com "
               "presen\u00e7a, mantendo o ataque definido. \u00c9 indicado para bateria, percuss\u00e3o, "
               "bus de rock e qualquer material em que o transiente precisa continuar existindo.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do Pop2 da Airwindows. Cada canal tem dois envelopes pr\u00f3prios, "
               "A e B, alternados a cada amostra. O tempo de ataque define a velocidade com que o "
               "ganho desce; a soltura define a velocidade com que ele volta. O drive funciona "
               "como pr\u00e9-ganho e tamb\u00e9m como makeup, ent\u00e3o aumentar o drive empurra o "
               "sinal para dentro do compressor e compensa o n\u00edvel na sa\u00edda."),
            u ("Um est\u00e1gio ClipOnly2 arredonda os picos antes da mistura dry/wet. Ele usa um "
               "pequeno buffer para suavizar a transi\u00e7\u00e3o do clipping e \u00e9 a origem da "
               "lat\u00eancia de 1 amostra do plugin. O adaptador Forja atrasa o caminho seco do "
               "bypass na mesma amostra para manter o crossfade de 10 ms alinhado."),
            u ("Lat\u00eancia: 1 amostra.")
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
        u ("Compress\u00e3o"),
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
        u ("Define o limiar. Em 0 o compressor quase n\u00e3o age; em 1 o sinal inteiro entra "
           "na curva e o ataque fica mais evidente.")
    });
    info.controls.push_back ({
        "Ataque",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
        u ("Velocidade com que o ganho desce. Valores baixos seguram o transiente; valores "
           "altos deixam o pico passar e comprimem o corpo.")
    });
    info.controls.push_back ({
        "Soltura",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
        u ("Velocidade com que o ganho volta. Valores baixos mant\u00eam o sinal comprimido por "
           "mais tempo; valores altos devolvem o n\u00edvel r\u00e1pido.")
    });
    info.controls.push_back ({
        "Drive",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
        u ("Pr\u00e9-ganho do compressor e makeup de sa\u00edda. Mais drive significa mais "
           "compress\u00e3o e mais presen\u00e7a; menos drive deixa o sinal mais aberto.")
    });
    info.controls.push_back ({
        "Mistura",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
        u ("Mistura dry/wet aplicada depois do ClipOnly2. O caminho seco j\u00e1 sai alinhado "
           "com a lat\u00eancia do wet.")
    });
    info.controls.push_back ({
        "Bypass",
        u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
        u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms e atraso de 1 amostra no caminho seco.")
    });

    info.provenance.source = "Airwindows Pop2";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("Licen\u00e7a MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double, mant\u00e9m os "
                               "envelopes independentes L/R e a altern\u00e2ncia A/B, dimensiona o "
                               "buffer do ClipOnly2 com 17 posi\u00e7\u00f5es para eliminar o acesso "
                               "fora dos limites e reporta a lat\u00eancia real de 1 amostra.");

    return info;
}

} // namespace forja::malho
