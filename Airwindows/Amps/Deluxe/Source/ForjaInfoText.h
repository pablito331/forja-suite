#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::deluxe
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "DELUXE";
    info.subtitle = u ("DI COMPRIMIDO \u00b7 FORJA");
    info.tagline = u ("DELUXE \u00e9 o DI da fam\u00edlia: dez bandas fixas, dezesseis blocos de "
                      "filtro em cascata e um seguidor de envelope por banda. O sinal sai "
                      "comprimido, com ataque preservado e corpo no lugar.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O ChimeyDeluxe da Airwindows \u00e9 a vers\u00e3o de DI da linhagem ChimeyGuitar: "
               "em vez de simular o amp inteiro, ele condiciona o sinal direto da guitarra, "
               "com compress\u00e3o embutida em cada banda. Serve antes de um amp, de um "
               "simulador ou como timbre de base para limpo e arpejo."),
            u ("N\u00e3o h\u00e1 um controle de drive separado: as dez bandas tamb\u00e9m definem "
               "o ganho global. A banda mais alta manda no drive; a mais baixa define o "
               "pad. Por isso os controles s\u00e3o interdependentes \u00b7 mexer em uma banda "
               "muda o car\u00e1ter inteiro.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O sinal atravessa dezesseis blocos em cascata. Cada bloco divide o \u00e1udio "
               "em doze bandas com filtros acoplados; a primeira banda \u00e9 o extremo agudo e "
               "a d\u00e9cima segunda o extremo grave. As dez bandas do painel s\u00e3o os ganhos "
               "dessas faixas (Hell cobre as duas mais agudas, Rarely cobre as mais graves)."),
            u ("Em cada banda, um seguidor de envelope do tipo Bezier acumula o m\u00f3dulo do "
               "sinal e fecha o ganho por meio de uma curva de seno. O ataque escapa, o "
               "corpo \u00e9 comprimido \u00b7 e isso se repete dezesseis vezes, bloco a bloco. "
               "No fim, um grampeamento em \u00b1 1 garante que a sa\u00edda nunca estoure.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("HELL, FIZZ e PICK s\u00e3o o topo: brilho extremo, chiado e ataque da palheta. "
               "SATAN, DANGER e CRITICAL s\u00e3o os m\u00e9dios, de agudo a grave. H MEAT e "
               "L MEAT d\u00e3o corpo; SWING e RARELY s\u00e3o o peso dos graves. Todos partem "
               "de 0,50, que \u00e9 o ajuste neutro de f\u00e1brica."),
            u ("Como o drive vem da banda mais alta, subir HELL deixa o DI mais sujo. Baixar "
               "qualquer banda reduz o pad e muda o n\u00edvel geral \u00b7 n\u00e3o h\u00e1 sa\u00edda "
               "separada para compensar.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Limpo com ataque: HELL 0,40, FIZZ 0,45, PICK 0,60, H MEAT 0,60. Para levar "
               "um DI cru a um amp: HELL 0,65, PICK 0,55, L MEAT 0,45. Em baixo, reduza "
               "SWING e RARELY para 0,40. BYPASS desvia com rampa de 10 ms; processamento "
               "em double, sem aloca\u00e7\u00e3o e sem PDC.")
        }
    });

    info.controls.push_back ({ "Hell", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Agudo extremo; tamb\u00e9m abre o drive quando \u00e9 a banda mais alta.") });
    info.controls.push_back ({ "Fizz", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Chiado e brilho do topo.") });
    info.controls.push_back ({ "Pick", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ataque da palheta, entre agudos e m\u00e9dios.") });
    info.controls.push_back ({ "Satan", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("M\u00e9dio agudo.") });
    info.controls.push_back ({ "Danger", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("M\u00e9dio central.") });
    info.controls.push_back ({ "Critical", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("M\u00e9dio grave.") });
    info.controls.push_back ({ "H Meat", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Corpo alto, abaixo dos m\u00e9dios.") });
    info.controls.push_back ({ "L Meat", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Corpo baixo, acima dos graves.") });
    info.controls.push_back ({ "Swing", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Graves.") });
    info.controls.push_back ({ "Rarely", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Subgraves; cobre as duas bandas mais graves.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows ChimeyDeluxe";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do "
                               "ChimeyDeluxe, incluindo as dezesseis cascatas de doze "
                               "bandas, os seguidores de envelope Bezier e o grampeamento "
                               "final. Acrescenta medidores de pico, bypass suavizado e "
                               "leitura de clipe; n\u00e3o h\u00e1 aloca\u00e7\u00e3o no caminho de "
                               "\u00e1udio.");

    return info;
}

} // namespace forja::deluxe
