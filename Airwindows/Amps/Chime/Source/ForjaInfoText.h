#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::chime
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "CHIME";
    info.subtitle = u ("AMP CHIME \u00b7 FORJA");
    info.tagline = u ("CHIME \u00e9 o amp comprimido da fam\u00edlia: cada banda tem seu "
                      "pr\u00f3prio seguidor de envelope, ent\u00e3o o ataque abre o som e o "
                      "corpo sustenta. Serve para guitarras que precisam brilhar sem gritar.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O ChimeyGuitar2 da Airwindows empilha at\u00e9 vinte blocos de filtro, "
               "cada um com nove bandas. Em cada banda, um compressor no estilo mu-law "
               "segura o n\u00edvel antes da soma. O resultado \u00e9 uma compress\u00e3o multibanda "
               "embutida na distor\u00e7\u00e3o: o ataque passa, o resto \u00e9 achatado com m\u00fasica."),
            u ("No fim, um limitador de seno garante que a sa\u00edda nunca passe de \u03c0/2, e "
               "os dois bancos de alto-falante moldam o topo e o corpo. \u00c9 um amp mais "
               "controlado e menos \u201csujo\u201d que o PONTA.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("A COMPRESS\u00c3O define o n\u00famero de polos (at\u00e9 20) e o limiar: "
               "`1 \u2212 0,81 \u00b7 compres\u00b2`. Cada polo percorre as nove bandas, aplica o "
               "ganho do envelope e atualiza a velocidade do compressor. VELOCIDADE "
               "define qu\u00e3o r\u00e1pido esse envelope se move, de lento e vocal a r\u00e1pido "
               "e percussivo."),
            u ("O limite de seno no final \u00e9 parte do car\u00e1ter: picos extremos s\u00e3o "
               "dobrados para dentro da curva, n\u00e3o cortados. Em n\u00edveis normais ele "
               "quase n\u00e3o age.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("COMPRESS\u00c3O em 0,40 entrega o amp limpo e din\u00e2mico; 0,60 a 0,80 \u00e9 o "
               "chime cl\u00e1ssico; acima de 0,90 o som fica denso e nivelado. VELOCIDADE "
               "baixa preserva o ataque; alta comprime at\u00e9 as varia\u00e7\u00f5es r\u00e1pidas. "
               "PRESEN\u00c7A, AGUDOS, M\u00c9DIOS, GRAVES e SUB s\u00e3o o tom. HSPEAKR e LSPEAKR "
               "s\u00e3o o gabinete. SA\u00cdDA compensa o n\u00edvel."),
            u ("N\u00e3o h\u00e1 gate: o compressor j\u00e1 mant\u00e9m o ru\u00eddo sob controle.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para arpejos e guitarras limpas com brilho: COMPRESS\u00c3O 0,55, "
               "VELOCIDADE 0,35, HSPEAKR 0,85. Para r\u00edtmica densa: COMPRESS\u00c3O 0,80, "
               "VELOCIDADE 0,70, SA\u00cdDA 0,65. Em baixo, baixe SUB para 0,30. BYPASS "
               "desvia com rampa de 10 ms; processamento em double, sem aloca\u00e7\u00e3o e "
               "sem PDC.")
        }
    });

    info.controls.push_back ({ "Compres", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,60"),
                               u ("N\u00famero de polos e limiar da compress\u00e3o por banda.") });
    info.controls.push_back ({ "Speed", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,80"),
                               u ("Velocidade do envelope de cada banda.") });
    info.controls.push_back ({ "Presence", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,80"),
                               u ("Ataque e defini\u00e7\u00e3o do pr\u00e9.") });
    info.controls.push_back ({ "High / Mid / Low / Sub", u ("0,00 a 1,00 \u00b7 padr\u00f5es 0,70 / 0,50 / 0,70 / 0,40"),
                               u ("Equaliza\u00e7\u00e3o de quatro faixas do amp.") });
    info.controls.push_back ({ "Highspeaker / Lowspeaker", u ("0,00 a 1,00 \u00b7 padr\u00f5es 0,80 / 0,40"),
                               u ("Corte de topo e de corpo do gabinete.") });
    info.controls.push_back ({ "Output", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,80"),
                               u ("N\u00edvel de sa\u00edda do amp.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows ChimeyGuitar2";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do "
                               "ChimeyGuitar2, incluindo o compressor mu-law por banda, o "
                               "limitador de seno e os bancos de alto-falante. Acrescenta "
                               "medidores de pico, bypass suavizado e leitura de clipe; "
                               "n\u00e3o h\u00e1 aloca\u00e7\u00e3o no caminho de \u00e1udio.");

    return info;
}

} // namespace forja::chime
