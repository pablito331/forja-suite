#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::escoria
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = u ("ESC\u00d3RIA");
    info.subtitle = u ("DE-ESSER DIN\u00c2MICO \u00b7 FORJA");
    info.tagline = u ("Esc\u00f3ria \u00e9 o res\u00edduo que sobra do metal. Aqui, \u00e9 o agudo "
                      "que incomoda: o detector acha os transientes e devolve o resto intacto.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Esc\u00f3ria \u00e9 um de-esser de transientes: reduz sibil\u00e2ncia, estalos de "
               "boca, chiado de pratos e aquela dureza de agudo que aparece nos picos. Ele n\u00e3o "
               "comprime a faixa inteira: age sobre a diferen\u00e7a entre o sinal e uma vers\u00e3o "
               "filtrada dele, ou seja, sobre a parcela aguda que passa pelo filtro interno."),
            u ("O resultado \u00e9 um controle de agudo dependente de n\u00edvel e de velocidade. "
               "Funciona em voz, bateria, guitarra e em qualquer fonte com picos de agudo "
               "irregulares. Em intensidade 0 o sinal passa intacto.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do DeBess da Airwindows. Para cada canal, o original guarda uma "
               "janela de derivadas (a diferen\u00e7a entre amostras consecutivas) e calcula "
               "derivadas das derivadas. O produto dessas medidas \u00e9 o detector: ele reage a "
               "mudan\u00e7as r\u00e1pidas e a picos de agudo, e n\u00e3o a graves sustentados."),
            u ("A janela tem de 2 a 40 amostras e \u00e9 controlada por VELOCIDADE. O detector "
               "alimenta uma raz\u00e3o de redu\u00e7\u00e3o que sobe e desce com suaviza\u00e7\u00e3o "
               "proporcional \u00e0 janela. O teto dessa raz\u00e3o vem de INTENSIDADE (A elevado \u00e0 "
               "quinta, escalado pela taxa de amostragem) e PROFUNDIDADE limita o fundo da "
               "redu\u00e7\u00e3o."),
            u ("O filtro interno \u00e9 um passa-baixos de um polo: o processamento reduz a "
               "diferen\u00e7a entre a amostra e essa vers\u00e3o filtrada. FILTRO define o ponto de "
               "separa\u00e7\u00e3o, portanto o quanto do espectro conta como agudo. O original "
               "alterna dois conjuntos de estados a cada amostra (flip A/B), o que dobra a "
               "resolu\u00e7\u00e3o interna do filtro e da raz\u00e3o."),
            u ("Lat\u00eancia: 0 amostras. O adaptador processa em double, aplica bypass com rampa "
               "de 10 ms e mede entrada e sa\u00edda. A dither de 64 bits do original est\u00e1 "
               "comentada l\u00e1 tamb\u00e9m; s\u00f3 o gerador pseudoaleat\u00f3rio avan\u00e7a.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("INTENSIDADE \u00e9 o quanto o detector pode puxar a raz\u00e3o de redu\u00e7\u00e3o. Em 0 "
               "nada acontece. VELOCIDADE define a janela do detector: valores baixos reagem a "
               "mudan\u00e7as muito r\u00e1pidas e curtam menos; valores altos olham mais amostras e "
               "suavizam o trabalho."),
            u ("PROFUNDIDADE \u00e9 invertida em rela\u00e7\u00e3o ao nome: em 0 a redu\u00e7\u00e3o pode ir "
               "ao m\u00e1ximo e em 1 ela \u00e9 praticamente nula. FILTRO escolhe o ponto de "
               "separa\u00e7\u00e3o dos agudos; acima de 0,50 o de-esser se concentra no topo do "
               "espectro. MONITOR acima de 0,50 troca a sa\u00edda pelo que foi removido, \u00fatil "
               "para achar a frequ\u00eancia que incomoda."),
            u ("O detector \u00e9 independente por canal, como no original. Em material est\u00e9reo "
               "muito correlacionado isso praticamente n\u00e3o se nota; em fontes assim\u00e9tricas, "
               "cada lado pode receber redu\u00e7\u00f5es diferentes.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Comece com INTENSIDADE em 0,25 e PROFUNDIDADE em 0,50; suba a intensidade at\u00e9 "
               "a sibil\u00e2ncia ceder. Use MONITOR para conferir o que est\u00e1 sendo retirado e "
               "volte para a posi\u00e7\u00e3o normal. Duplo clique devolve o controle ao padr\u00e3o; "
               "as setas fazem ajuste fino. BYPASS desvia o DSP com rampa de 10 ms e os "
               "medidores continuam ativos."),
            u ("O original tem uma particularidade preservada de prop\u00f3sito: no ramo B do flip, "
               "a condi\u00e7\u00e3o que decide aplicar a redu\u00e7\u00e3o testa a raz\u00e3o do ramo A. Como os "
               "dois ramos seguem o mesmo detector, a diferen\u00e7a \u00e9 pequena, mas faz parte do "
               "som e n\u00e3o foi corrigida.")
        }
    });

    info.controls.push_back ({ "Intensidade", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Quanto o detector pode reduzir. Em 0 o sinal passa intacto.") });
    info.controls.push_back ({ "Velocidade", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Janela do detector, de 2 a 40 amostras.") });
    info.controls.push_back ({ u ("Profundidade"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Fundo da redu\u00e7\u00e3o: 0 = m\u00e1ximo, 1 = praticamente nenhum.") });
    info.controls.push_back ({ "Filtro", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ponto de separa\u00e7\u00e3o dos agudos processados.") });
    info.controls.push_back ({ "Monitor", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Acima de 0,50 a sa\u00edda toca s\u00f3 o que foi removido.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows DeBess";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do DeBess, "
                               "inclusive a condi\u00e7\u00e3o do ramo B que testa a raz\u00e3o A. "
                               "Acrescenta apenas medidores de pico, bypass suavizado e leitura de "
                               "clipe; a dither de 64 bits est\u00e1 comentada no pr\u00f3prio original.");

    return info;
}

} // namespace forja::escoria
