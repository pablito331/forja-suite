#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::ponta
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "PONTA";
    info.subtitle = u ("AMP DE GUITARRA \u00b7 FORJA");
    info.tagline = u ("PONTA \u00e9 um amp de guitarra completo em um s\u00f3 plugin: "
                      "pr\u00e9 com distor\u00e7\u00e3o, equaliza\u00e7\u00e3o de tr\u00eas bandas, "
                      "gabinete e gate. Do sinal de entrada ao cone, sem pedais.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O PointyGuitar da Airwindows empilha blocos de filtro para criar a "
               "distor\u00e7\u00e3o no dom\u00ednio da frequ\u00eancia. O DRIVE define quantos est\u00e1gios "
               "entram: cada est\u00e1gio passa o sinal por nove bandas em cascata e depois "
               "por um dobrador assim\u00e9trico suave. Quanto mais est\u00e1gios, mais o sinal "
               "se dobra e mais harm\u00f4nicos aparecem."),
            u ("Depois do pr\u00e9 v\u00eam o gate e o alto-falante. O gate n\u00e3o \u00e9 um expander "
               "comum: ele espera uma passagem de zero para abrir e fecha por tempo, o "
               "que soa como um amp de verdade. O alto-falante tem um passa-baixos e um "
               "passa-altos em cascata, ambos controlados por knobs pr\u00f3prios.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("Para cada amostra, o la\u00e7o externo percorre o n\u00famero de polos derivado "
               "do drive e o interno percorre nove bandas Bessel-ish entrela\u00e7adas. A "
               "soma das diferen\u00e7as entre entrada e sa\u00edda de cada banda forma o sinal "
               "distorcido, que \u00e9 ent\u00e3o dobrado por x\u00b7|x|\u00b70,654. O resultado \u00e9 "
               "uma distor\u00e7\u00e3o que muda de car\u00e1ter com o conte\u00fado, n\u00e3o s\u00f3 de "
               "quantidade."),
            u ("O gate calcula um limiar a partir de GATE ao cubo e usa a contagem de "
               "amostras desde a \u00faltima passagem por zero como tempo de abertura. Em "
               "GATE 0,00 o limiar \u00e9 t\u00e3o baixo que ele s\u00f3 age em sil\u00eancio absoluto.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("DRIVE \u00e9 o ganho e a quantidade de est\u00e1gios; 0,40 a 0,60 entrega "
               "crunch, 0,70 a 1,00 entrega distor\u00e7\u00e3o densa. PRESEN\u00c7A, AGUDOS, "
               "M\u00c9DIOS, GRAVES e SUB s\u00e3o o tom do pr\u00e9. HSPEAKR e LSPEAKR s\u00e3o o "
               "gabinete: o primeiro corta o topo, o segundo corta o corpo. SA\u00cdDA "
               "compensa o n\u00edvel final."),
            u ("O adaptador n\u00e3o inventa ganho de compensa\u00e7\u00e3o nem normaliza o drive: "
               "o amp fica mais alto quando distorce, como no original. O gate \u00e9 o "
               "\u00fanico processamento din\u00e2mico.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Guitarra limpa: DRIVE 0,30, HSPEAKR 0,75, LSPEAKR 0,30, SA\u00cdDA 0,70. "
               "Crunch: DRIVE 0,55, PRESEN\u00c7A 0,60. Distor\u00e7\u00e3o: DRIVE 0,85 e SA\u00cdDA "
               "0,55. Para gravar com menos ru\u00eddo, GATE 0,20 a 0,35. BYPASS desvia com "
               "rampa de 10 ms; o processamento \u00e9 em double, sem aloca\u00e7\u00e3o e sem PDC.")
        }
    });

    info.controls.push_back ({ "Drive", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,60"),
                               u ("Ganho e n\u00famero de est\u00e1gios de distor\u00e7\u00e3o.") });
    info.controls.push_back ({ "Presence", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ataque e defini\u00e7\u00e3o do pr\u00e9.") });
    info.controls.push_back ({ "High / Mid / Low / Sub", u ("0,00 a 1,00 \u00b7 padr\u00f5es 0,80 / 0,60 / 0,40 / 0,50"),
                               u ("Equaliza\u00e7\u00e3o de quatro faixas do amp.") });
    info.controls.push_back ({ "Highspeaker / Lowspeaker", u ("0,00 a 1,00 \u00b7 padr\u00f5es 0,70 / 0,40"),
                               u ("Corte de topo e de corpo do gabinete.") });
    info.controls.push_back ({ "Gate", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Limiar do gate; 0,00 = praticamente desligado.") });
    info.controls.push_back ({ "Output", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,70"),
                               u ("N\u00edvel de sa\u00edda do amp.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows PointyGuitar";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do "
                               "PointyGuitar, incluindo o gate e os dois bancos de "
                               "alto-falante. Acrescenta medidores de pico, bypass suavizado "
                               "e leitura de clipe; n\u00e3o h\u00e1 aloca\u00e7\u00e3o no caminho de "
                               "\u00e1udio.");

    return info;
}

} // namespace forja::ponta
