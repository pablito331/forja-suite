#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::lixa
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "LIXA";
    info.subtitle = u ("DRIVE LIMPO \u00b7 FORJA");
    info.tagline = u ("LIXA \u00e9 o drive de um s\u00f3 controle que n\u00e3o vira distor\u00e7\u00e3o: "
                      "uma curva de seno arredonda os picos e a quantidade aplicada acompanha o "
                      "pr\u00f3prio sinal. Serve para limpar transientes e dar cola sem chiado.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O PurestDrive troca a linha reta por uma curva de seno. Em vez de cortar o "
               "pico, ele dobra a onda para dentro com suavidade. Quanto mais o sinal se "
               "aproxima de 1,0, mais a curva age; passagens baixas quase n\u00e3o s\u00e3o "
               "tocadas."),
            u ("O nome \u00e9 honesto: n\u00e3o h\u00e1 satura\u00e7\u00e3o de fita, n\u00e3o h\u00e1 "
               "harm\u00f4nicos empilhados por gera\u00e7\u00e3o. \u00c9 um amaciador de picos.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("Para cada sample, o original calcula o seno do sinal e mistura o valor seco "
               "com o seno usando |anterior + seno| / 2 \u00b7 drive. O termo \u201canterior\u201d "
               "faz a quantidade reagir \u00e0 hist\u00f3ria recente, e o drive escala tudo. O "
               "resultado fica entre o sinal original e a curva, nunca fora dela."),
            u ("A curva \u00e9 aplicada por canal, sem liga\u00e7\u00e3o est\u00e9reo, e o estado "
               "\u00e9 apenas o sample anterior do seno. N\u00e3o h\u00e1 filtro, buffer ou "
               "lat\u00eancia.")
        }
    });

    info.sections.push_back ({
        u ("O controle"),
        {
            u ("DRIVE em 0,00 deixa o sinal intacto. Em 0,25 o amaciamento j\u00e1 aparece em "
               "material quente; em 0,50 \u00e9 o ponto de uso geral; acima de 0,75 a curva "
               "come\u00e7a a arredondar ataques de bateria e a colar a voz."),
            u ("O original n\u00e3o tem ganho de compensa\u00e7\u00e3o e o adaptador n\u00e3o "
               "inventa um. Se subir o drive baixar o n\u00edvel, a sa\u00edda da cadeia "
               "compensa.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Em vocais, 0,20 a 0,35 antes do compressor. Em bateria, 0,40 para arredondar "
               "sem apagar o ataque. Em mix bus, 0,15 s\u00f3 para colar. BYPASS desvia o DSP "
               "com rampa de 10 ms; o processamento \u00e9 em double, sem aloca\u00e7\u00e3o e "
               "sem PDC.")
        }
    });

    info.controls.push_back ({ "Drive", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Quantidade da curva de seno; 0 = sinal intacto.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows PurestDrive";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do "
                               "PurestDrive: o seno por canal, a mistura com o sample anterior "
                               "e o RNG do original. Acrescenta medidores de pico, bypass "
                               "suavizado e leitura de clipe; n\u00e3o h\u00e1 aloca\u00e7\u00e3o "
                               "no caminho de \u00e1udio.");

    return info;
}

} // namespace forja::lixa
