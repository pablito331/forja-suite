#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::lima
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "LIMA";
    info.subtitle = u ("EQUALIZADOR DE BUS \u00b7 FORJA");
    info.tagline = u ("Quatro bandas largas separadas por crossovers suaves, sem satura\u00e7\u00e3o: "
                      "o ajuste que o bus inteiro aceita sem soar mexido.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Lima \u00e9 um equalizador de quatro bandas pensado para bus e masteriza\u00e7\u00e3o. "
               "As divis\u00f5es entre as bandas s\u00e3o suaves e os ganhos s\u00e3o aplicados depois "
               "do crossover, ent\u00e3o uma corre\u00e7\u00e3o de graves ou de agudos mexe no "
               "equil\u00edbrio geral sem criar artefatos de fase agressivos."),
            u ("N\u00e3o h\u00e1 satura\u00e7\u00e3o, compressor ou colora\u00e7\u00e3o escondida: \u00e9 um "
               "equalizador limpo, feito para corrigir e finalizar.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do SmoothEQ2 da Airwindows. Tr\u00eas est\u00e1gios de crossover, "
               "cada um com tr\u00eas filtros biquadr\u00e1ticos, dividem o sinal em alta, m\u00e9dia alta, "
               "m\u00e9dia baixa e graves. Um est\u00e1gio exponencial (um polo) completa a divis\u00e3o. "
               "As quatro partes s\u00e3o somadas de volta j\u00e1 com os ganhos aplicados."),
            u ("Cada banda tem um ganho e um controle de frequ\u00eancia, e os controles se "
               "influenciam: a frequ\u00eancia de uma banda tamb\u00e9m ajusta a largura e a "
               "separa\u00e7\u00e3o das vizinhas. \u00c9 por isso que o EQ soa sempre suave, mesmo com "
               "ajustes grandes."),
            u ("A lei de ganho do original \u00e9 usada tal como est\u00e1: em 0,5 a banda \u00e9 neutra, "
               "abaixo disso o ganho cai rapidamente e acima sobe at\u00e9 cerca de +6 dB. "
               "Lat\u00eancia: 0 amostras. Bypass com rampa de 10 ms e curva medida por FFT do "
               "pr\u00f3prio filtro em n\u00edvel baixo.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("O card GANHOS re\u00fane os quatro n\u00edveis de banda e o card FREQU\u00caNCIAS os "
               "quatro centros. Comece movendo apenas os ganhos; use as frequ\u00eancias para "
               "reposicionar o problema, n\u00e3o para \u201cdesenhar\u201d um EQ cir\u00fargico. "
               "Duplo clique devolve ao padr\u00e3o. BYPASS compara com o sinal seco sem clique."),
            u ("Na curva, arraste os n\u00f3s A, M e G na horizontal para mover os tr\u00eas "
               "crossovers. Como cada parametro de frequ\u00eancia participa de mais de um "
               "crossover, o arraste resolve a combina\u00e7\u00e3o e os knobs acompanham.")
        }
    });

    info.controls.push_back ({ "Banda alta", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda aguda. Em 0,5 neutro; acima abre at\u00e9 +6 dB.") });
    info.controls.push_back ({ u ("Banda m\u00e9dia alta"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda m\u00e9dia alta, a regi\u00e3o de presen\u00e7a.") });
    info.controls.push_back ({ u ("Banda m\u00e9dia baixa"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda m\u00e9dia baixa, a regi\u00e3o de corpo.") });
    info.controls.push_back ({ "Banda grave", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda de graves.") });
    info.controls.push_back ({ u ("Frequ\u00eancia alta"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Centro da banda alta; tamb\u00e9m ajusta a largura das vizinhas.") });
    info.controls.push_back ({ u ("Frequ\u00eancia m\u00e9dia alta"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Centro da banda m\u00e9dia alta.") });
    info.controls.push_back ({ u ("Frequ\u00eancia m\u00e9dia baixa"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Centro da banda m\u00e9dia baixa.") });
    info.controls.push_back ({ u ("Frequ\u00eancia grave"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Centro da banda de graves.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows SmoothEQ2";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("Licen\u00e7a MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double, incluindo a lei "
                               "de ganho 1 + x\u00b7|x|\u00b7|x| e os tr\u00eas est\u00e1gios de crossover. "
                               "Bypass suavizado, medidores, curva por FFT e a UI s\u00e3o "
                               "adi\u00e7\u00f5es Forja; o caminho de \u00e1udio n\u00e3o tem satura\u00e7\u00e3o.");

    return info;
}

} // namespace forja::lima
