#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::mel
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "MEL";
    info.subtitle = u ("BRILHO SUAVE \u00b7 FORJA");
    info.tagline = u ("MEL \u00e9 o ado\u00e7ador de topo: uma m\u00e9dia curta do sinal ao "
                      "quadrado \u00e9 subtra\u00edda da entrada, arredondando picos e "
                      "amaciar os agudos sem injetar harm\u00f4nicos de exciter.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O Sweeten pega o sinal, eleva ao quadrado, aplica uma pilha de m\u00e9dias "
               "curtas e subtrai esse valor do sinal seco. Como o quadrado cresce mais r\u00e1pido "
               "nos picos, a subtra\u00e7\u00e3o age principalmente onde o sinal j\u00e1 est\u00e1 "
               "alto, deixando as passagens baixas quase intactas."),
            u ("O efeito \u00e9 de amaciamento: o topo perde aspereza e o material ganha uma "
               "colora\u00e7\u00e3o doce. N\u00e3o \u00e9 um EQ, n\u00e3o tem frequ\u00eancia de "
               "corte e n\u00e3o gera aquele brilho sint\u00e9tico de exciter.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O controle escolhe quantos bits de atenua\u00e7\u00e3o entram na conta: "
               "2^(controle \u00b7 10 \u2212 10). Em 0,00 o fator \u00e9 2^\u221210, t\u00e3o "
               "pequeno que o sinal praticamente n\u00e3o muda; em 1,00 o fator \u00e9 1 e a "
               "subtra\u00e7\u00e3o \u00e9 m\u00e1xima."),
            u ("Antes e depois do quadrado, o original passa o sinal por m\u00e9dias de 2 a 8 "
               "amostras conforme a taxa (at\u00e9 quatro est\u00e1gios em 192 kHz), o que "
               "limita o efeito ao que \u00e9 cont\u00ednuo e evita chirrido. O estado \u00e9 "
               "uma tabela de dezenove m\u00e9dias por canal, sem atraso declarado.")
        }
    });

    info.sections.push_back ({
        u ("O controle"),
        {
            u ("MEL \u00e9 para ajuste fino. Em 0,20 a 0,40 j\u00e1 se ouve o topo mais macio "
               "em voz e viol\u00e3o; em 0,60 a 0,80 o arredondamento chega em bateria e "
               "metais. Acima de 0,90 o efeito vira colora\u00e7\u00e3o e pode escurecer "
               "demais."),
            u ("O original n\u00e3o tem ganho de compensa\u00e7\u00e3o. O adaptador tamb\u00e9m "
               "n\u00e3o inventa um.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Coloque no fim da cadeia de voz, depois do compressor, em 0,25. Em mix bus, "
               "0,15. Em material j\u00e1 processado, 0,10 s\u00f3 para assentar. BYPASS desvia "
               "o DSP com rampa de 10 ms; o processamento \u00e9 em double, sem aloca\u00e7\u00e3o "
               "e sem PDC.")
        }
    });

    info.controls.push_back ({ "Sweeten", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Quantidade da subtra\u00e7\u00e3o quadrada; 0 = quase intacto.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows Sweeten";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do Sweeten: "
                               "as dezenove m\u00e9dias por canal, a sele\u00e7\u00e3o de bits "
                               "por controle e o RNG do original. Acrescenta medidores de pico, "
                               "bypass suavizado e leitura de clipe; nenhuma aloca\u00e7\u00e3o "
                               "dentro de process().");

    return info;
}

} // namespace forja::mel
