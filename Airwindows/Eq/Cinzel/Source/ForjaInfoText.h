#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::cinzel
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = u ("CINZ\u00c9L");
    info.subtitle = u ("EQUALIZADOR PARAM\u00c9TRICO DIN\u00c2MICO \u00b7 FORJA");
    info.tagline = u ("Tr\u00eas bandas param\u00e9tricas com resson\u00e2ncia musical e uma camada "
                      "din\u00e2mica opcional que s\u00f3 existe na Forja: esculpe sem estourar.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Cinzel \u00e9 um equalizador param\u00e9trico de tr\u00eas bandas: alta, m\u00e9dia alta e "
               "m\u00e9dia baixa. Cada banda tem frequ\u00eancia, ganho e resson\u00e2ncia pr\u00f3prios, "
               "com curvas largas e musicais. \u00c9 um EQ de tom e de corre\u00e7\u00e3o: serve para "
               "dar corpo, remover dureza e desenhar a presen\u00e7a de uma fonte ou de um bus."),
            u ("As bandas trabalham sobre o sinal seco: o resultado \u00e9 o som original somado ao "
               "que cada filtro extrai da regi\u00e3o. Por isso os ganhos soam como realces e "
               "cortes suaves, e n\u00e3o como um crossover cir\u00fargico.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do Parametric da Airwindows. Cada banda aplica tr\u00eas filtros "
               "biquadr\u00e1ticos em cascata, com resson\u00e2ncias escalonadas (1,93, 0,71 e 0,52) "
               "para espalhar a a\u00e7\u00e3o da banda no espectro. A frequ\u00eancia \u00e9 mapeada "
               "por uma curva c\u00fabica: perto de 0 o controle desce at\u00e9 a regi\u00e3o m\u00ednima "
               "da banda e perto de 1 chega ao topo."),
            u ("Faixas aproximadas: alta de 1,5 kHz a 16 kHz, m\u00e9dia alta de 600 Hz a 7 kHz e "
               "m\u00e9dia baixa de 20 Hz a 2,2 kHz. O ganho usa a lei do original: em 0,5 a banda "
               "fica neutra, abaixo corta e acima real\u00e7a, com mais peso nos realces. A "
               "resson\u00e2ncia estreita ou alarga o foco de cada banda."),
            u ("Lat\u00eancia: 0 amostras. O adaptador processa em double, aplica bypass com rampa "
               "de 10 ms e mede a curva por FFT do pr\u00f3prio filtro em n\u00edvel baixo.")
        }
    });

    info.sections.push_back ({
        u ("A camada din\u00e2mica"),
        {
            u ("O controle DIN\u00c2MICA \u00e9 uma extens\u00e3o da Forja, n\u00e3o da Airwindows. Em 0 "
               "ele fica completamente desligado e o caminho \u00e9 id\u00eantico ao Parametric. "
               "Acima de 0 cada banda ganha um detector est\u00e9reo linkado que age apenas na "
               "pr\u00f3pria banda, com ataque de 10 ms e libera\u00e7\u00e3o de 150 ms."),
            u ("O limiar cai de \u22126 dBFS em 0 para \u221236 dBFS em 1. Quando a banda passa do "
               "limiar, a a\u00e7\u00e3o reduz o ganho em at\u00e9 12 dB: realces encolhem na dire\u00e7\u00e3o "
               "de 0 dB e cortes ficam mais profundos. Em m\u00fasica densa isso segura picos de "
               "agudo sem achatar o resto; em cortes, funciona como um de-esser ou controle de "
               "mudan\u00e7a de timbre dependente de n\u00edvel."),
            u ("O medidor de redu\u00e7\u00e3o mostra a maior redu\u00e7\u00e3o instant\u00e2nea entre as tr\u00eas "
               "bandas. A curva desenhada \u00e9 sempre a est\u00e1tica (dyn = 0); a a\u00e7\u00e3o "
               "din\u00e2mica aparece no medidor.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Cada banda tem tr\u00eas knobs em coluna: frequ\u00eancia, ganho e resson\u00e2ncia. "
               "Duplo clique devolve o controle ao padr\u00e3o; as setas fazem ajuste fino. "
               "MISTURA permite equilibrar o EQ processado com o seco. O bot\u00e3o BYPASS desvia "
               "o DSP sem clique e os medidores continuam ativos."),
            u ("Na curva, arraste os n\u00f3s A, MA e MB: na horizontal o n\u00f3 muda a frequ\u00eancia "
               "da banda e na vertical o ganho. Os knobs acompanham o arraste em tempo real.")
        }
    });

    info.controls.push_back ({ "Freq. alta", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Centro da banda alta, de 1,5 kHz a 16 kHz.") });
    info.controls.push_back ({ "Ganho alto", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Em 0,5 neutro; abaixo corta e acima real\u00e7a a banda alta.") });
    info.controls.push_back ({ u ("Resson\u00e2ncia alta"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Foco da banda alta. Valores baixos alargam, altos estreitam.") });
    info.controls.push_back ({ u ("Freq. m\u00e9dia alta"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Centro da banda m\u00e9dia alta, de 600 Hz a 7 kHz.") });
    info.controls.push_back ({ u ("Ganho m\u00e9dio alto"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Corta ou real\u00e7a a presen\u00e7a e o corpo m\u00e9dio-alto.") });
    info.controls.push_back ({ u ("Resson\u00e2ncia m\u00e9dia alta"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Foco da banda m\u00e9dia alta.") });
    info.controls.push_back ({ u ("Freq. m\u00e9dia baixa"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Centro da banda m\u00e9dia baixa, de 20 Hz a 2,2 kHz.") });
    info.controls.push_back ({ u ("Ganho m\u00e9dio baixo"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Corta ou real\u00e7a a regi\u00e3o de corpo e calor.") });
    info.controls.push_back ({ u ("Resson\u00e2ncia m\u00e9dia baixa"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Foco da banda m\u00e9dia baixa.") });
    info.controls.push_back ({ "Mistura", u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("Dry/wet entre o sinal seco e o equalizado.") });
    info.controls.push_back ({ u ("Din\u00e2mica"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Liga a camada din\u00e2mica por banda. Em 0 o caminho \u00e9 "
                                  "id\u00eantico ao Parametric.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = u ("Airwindows Parametric (base) + camada din\u00e2mica Forja");
    info.provenance.author = u ("Chris Johnson (Airwindows) \u00b7 extens\u00e3o Forja");
    info.provenance.license = u ("Base MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do Parametric. "
                               "Com DIN\u00c2MICA em 0 a paridade \u00e9 bit-exata; acima de 0 o "
                               "detector por banda e o ganho suavizado s\u00e3o uma extens\u00e3o "
                               "documentada, n\u00e3o comportamento Airwindows.");

    return info;
}

} // namespace forja::cinzel
