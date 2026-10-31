#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::cadinho
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "CADINHO";
    info.subtitle = u ("STRIP DE BUS \u00b7 FORJA");
    info.tagline = u ("No cadinho o metal se mistura: quatro bandas entrela\u00e7adas, "
                      "compress\u00e3o de bandas e ClipOnly3 na sa\u00edda. Feito para bus e master.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Cadinho \u00e9 um strip de bus de quatro bandas: agudos, m\u00e9dio-alto, "
               "m\u00e9dio-baixo e graves, cada uma com ganho e frequ\u00eancia pr\u00f3prios. "
               "Depois do EQ existe um est\u00e1gio din\u00e2mico (SmoothEQ2/Dynamics2) que "
               "segura a soma das bandas, e um fader com ClipOnly3 no fim do caminho."),
            u ("\u00c9 o par de bus do BIGORNA: onde o Bigorna molda uma fonte, o Cadinho "
               "equilibra a mistura inteira. Em lat\u00eancia 1 amostra, ele serve para o bus "
               "principal ou para um grupo."),
            u ("O caminho passa por uma satura\u00e7\u00e3o Console na entrada, que arredonda os "
               "extremos com uma curva exponencial (raz\u00e3o \u00e1urea) antes de qualquer filtro. "
               "Ela \u00e9 parte do som e n\u00e3o foi removida.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do X2Buss da Airwindows. O sinal entra na Console, \u00e9 limitado "
               "a \u00b11, e segue para cruzamentos de tr\u00eas vias. Cada banda usa tr\u00eas "
               "biquadr\u00e1ticos em cascata, com resson\u00e2ncias escalonadas (2,247, 0,802 e "
               "0,555), o que d\u00e1 ao cruzamento uma resposta suave mesmo sendo raso."),
            u ("Depois dos cruzamentos, um est\u00e1gio de IIRs exponenciais completa a separa\u00e7\u00e3o "
               "e os quatro ganhos s\u00e3o aplicados com lei c\u00fabica em torno de 0,50: o desvio "
               "vira d, o ganho \u00e9 1 + d\u00b7|d|\u00b7|d|, o que d\u00e1 mais peso aos realces do que "
               "aos cortes."),
            u ("As quatro frequ\u00eancias (E a H) n\u00e3o mexem s\u00f3 nos cortes: elas combinam "
               "para formar tr\u00eas frequ\u00eancias de crossover e tr\u00eas fatores de Q. Subir "
               "a frequ\u00eancia de uma banda muda o foco e a largura das vizinhas \u2014 \u00e9 parte "
               "do desenho e n\u00e3o um efeito colateral."),
            u ("O est\u00e1gio SmoothEQ2/Dynamics2 mede a energia do sinal em duas janelas "
               "(r\u00e1pida e lenta) e aplica uma curva de B\u00e9zier para reduzir a soma quando "
               "ela passa do limiar. "
               "DIN\u00c2MICA controla o quanto e a velocidade dessa medi\u00e7\u00e3o. O fader "
               "multiplica por at\u00e9 2 e o ClipOnly3 limita a sa\u00edda com suaviza\u00e7\u00e3o de "
               "slew. Lat\u00eancia: 1 amostra.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("Os quatro ganhos t\u00eam 0,50 como neutro. Abaixo disso a banda \u00e9 atenuada; "
               "acima, real\u00e7ada. As quatro frequ\u00eancias tamb\u00e9m partem de 0,50 e movem o "
               "ponto de cruzamento e o Q das bandas vizinhas, ent\u00e3o peque\u00e1s mudan\u00e7as "
               "redesenham o equil\u00edbrio inteiro."),
            u ("DIN\u00c2MICA \u00e9 invertida em rela\u00e7\u00e3o ao nome: em 1,00 o est\u00e1gio "
               "din\u00e2mico fica desligado e o bus passa seco; perto de 0,00 a compress\u00e3o de "
               "bandas e a medi\u00e7\u00e3o ficam no m\u00e1ximo. O medidor de redu\u00e7\u00e3o mostra "
               "quanto est\u00e1 sendo segurado. FADER em 0,50 \u00e9 ganho unit\u00e1rio; acima disso "
               "o bus ganha at\u00e9 +6 dB, abaixo atenua.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para usar como bus neutro: ganhos e frequ\u00eancias em 0,50, DIN\u00c2MICA em "
               "1,00 e FADER em 0,50. Ajuste as bandas primeiro e s\u00f3 depois des\u00e7a a "
               "DIN\u00c2MICA. Duplo clique devolve o controle ao padr\u00e3o; as setas fazem ajuste "
               "fino. BYPASS desvia o strip com rampa de 10 ms e os medidores continuam ativos."),
            u ("Desvios do adaptador: medidores de pico e de redu\u00e7\u00e3o, bypass suavizado e "
               "buffers do ClipOnly3 maiores (33/65 posi\u00e7\u00f5es em vez de 18/34). O original "
               "fica fora dos limites acima de ~372 kHz; o port continua definido e a paridade "
               "\u00e9 verificada at\u00e9 352,8 kHz, onde o original ainda est\u00e1 dentro da "
               "mem\u00f3ria. As tens\u00f5es de B\u00e9zier come\u00e7am em 1,0, como no construtor do "
               "X2Buss, e a dither de 64 bits est\u00e1 comentada no original.")
        }
    });

    info.controls.push_back ({ "Agudos", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda alta com lei c\u00fabica; 0,50 \u00e9 neutro.") });
    info.controls.push_back ({ "M. alto", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda m\u00e9dio-alta.") });
    info.controls.push_back ({ "M. baixo", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda m\u00e9dio-baixa.") });
    info.controls.push_back ({ "Graves", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho da banda grave.") });
    info.controls.push_back ({ "Freq. alta", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ponto de cruzamento dos agudos; inclina o Q do m\u00e9dio-alto.") });
    info.controls.push_back ({ "Freq. m. alta", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Cruzamento m\u00e9dio-alto; interage com agudos e m\u00e9dio-baixo.") });
    info.controls.push_back ({ "Freq. m. baixa", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Cruzamento m\u00e9dio-baixo; interage com m\u00e9dio-alto e graves.") });
    info.controls.push_back ({ "Freq. grave", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ponto de cruzamento dos graves.") });
    info.controls.push_back ({ u ("Din\u00e2mica"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 1,00"),
                               u ("Compress\u00e3o de bandas: 1,00 desliga, perto de 0,00 \u00e9 o m\u00e1ximo.") });
    info.controls.push_back ({ "Fader", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho de sa\u00edda; 0,50 \u00e9 unit\u00e1rio.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows X2Buss";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do X2Buss. "
                               "Acrescenta medidores, bypass suavizado e buffers de ClipOnly3 "
                               "maiores; preserva a Console de entrada, a inicializa\u00e7\u00e3o das "
                               "tens\u00f5es de B\u00e9zier em 1,0 e a lat\u00eancia de 1 amostra.");

    return info;
}

} // namespace forja::cadinho
