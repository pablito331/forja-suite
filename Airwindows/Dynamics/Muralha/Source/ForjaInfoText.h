#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::muralha
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "MURALHA";
    info.subtitle = u ("LIMITADOR ADAPTATIVO \u00b7 FORJA");
    info.tagline = u ("Um muro no fim da cadeia: o sinal pode entrar quente, mas o pico "
                      "sai contido. O ADClip9 adapta o clipe ao movimento do material em vez "
                      "de usar um teto fixo.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Muralha \u00e9 um limitador de clipe adaptativo. Ele observa a inclina\u00e7\u00e3o "
               "do sinal e aperta o teto quando o material se move r\u00e1pido, devolvendo o "
               "espa\u00e7o quando o som se acalma. \u00c9 indicado para bus de bateria, bus de "
               "mix e master, ou como \u00faltima prote\u00e7\u00e3o antes da sa\u00edda."),
            u ("N\u00e3o h\u00e1 lookahead: a resposta \u00e9 imediata e a lat\u00eancia \u00e9 de 1 amostra. "
               "O resultado preserva mais transiente do que um brickwall tradicional, ao custo "
               "de n\u00e3o segurar o pico com antecipa\u00e7\u00e3o.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do ADClip9 da Airwindows. O est\u00e1gio ClipOnly3 compara cada "
               "amostra com o n\u00edvel de ru\u00eddo e com o teto, mede a maior varia\u00e7\u00e3o recente "
               "e reduz o teto efetivo conforme essa inclina\u00e7\u00e3o. Quando o clipe acontece, "
               "as amostras seguintes s\u00e3o suavizadas pelo buffer interno antes de voltar "
               "ao sinal."),
            u ("Boost e Match aplicam at\u00e9 18 dB na entrada. No modo MATCH o ganho de entrada "
               "\u00e9 desfeito na sa\u00edda; no modo CLIP ONLY o boost serve s\u00f3 para empurrar o "
               "sinal contra o clipe e o overshoot \u00e9 removido depois."),
            u ("Lat\u00eancia: 1 amostra. O caminho seco do bypass recebe o mesmo atraso para o "
               "crossfade de 10 ms n\u00e3o virar filtro em peine.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Use o modo BOOST para colorir e limitar de uma vez, MATCH para ganho casado "
               "e CLIP ONLY para proteger sem alterar o n\u00edvel. O medidor GR mostra quantos "
               "dB o pico foi contido. O bot\u00e3o BYPASS desvia o DSP com rampa curta.")
        }
    });

    info.controls.push_back ({
        "Boost",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
        u ("Ganho de entrada de 0 a +18 dB. Empurra o sinal para dentro do clipe.")
    });
    info.controls.push_back ({
        "Match",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
        u ("Ganho de 0 a +18 dB aplicado no modo MATCH e desfeito na sa\u00edda, mantendo o "
           "n\u00edvel percebido.")
    });
    info.controls.push_back ({
        "Noise",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,70"),
        u ("N\u00edvel de ru\u00eddo usado na transi\u00e7\u00e3o do clipe. Valores altos suavizam o "
           "arredondamento e escurecem o ataque.")
    });
    info.controls.push_back ({
        "Ceiling",
        u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,75"),
        u ("Teto m\u00e1ximo do clipe. Em 1,00 o limite chega perto de 0 dBFS; valores baixos "
           "deixam mais headroom.")
    });
    info.controls.push_back ({
        u ("Modo"),
        u ("BOOST / MATCH / CLIP ONLY \u00b7 padr\u00e3o BOOST"),
        u ("Escolhe como o ganho de entrada e o overshoot s\u00e3o tratados na sa\u00edda.")
    });
    info.controls.push_back ({
        "Bypass",
        u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
        u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms e atraso de 1 amostra no caminho seco.")
    });

    info.provenance.source = "Airwindows ADClip9";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("Licen\u00e7a MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve processDoubleReplacing em double, "
                               "mant\u00e9m o est\u00e1gio ClipOnly3, os buffers intermediate[18] e "
                               "slew[34] e o RNG por inst\u00e2ncia. O modo discreto usa os mesmos "
                               "tr\u00eas pontos do original (0 / 0,5 / 1). Bypass, medi\u00e7\u00e3o e "
                               "APVTS s\u00e3o camadas Forja; a lat\u00eancia medida por impulso \u00e9 "
                               "1 amostra.");

    return info;
}

} // namespace forja::muralha
