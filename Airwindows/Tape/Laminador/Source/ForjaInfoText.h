#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::laminador
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "LAMINADOR";
    info.subtitle = u ("FITA DE EST\u00daDIO \u00b7 FORJA");
    info.tagline = u ("Laminar \u00e9 passar o metal entre cilindros at\u00e9 ele ficar do jeito "
                      "certo. Aqui, o cilindro \u00e9 a fita: a entrada ganha corpo, o cabe\u00e7ote "
                      "refor\u00e7a o grave e o flutter move a velocidade como uma m\u00e1quina real.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Laminador \u00e9 uma emula\u00e7\u00e3o de fita de est\u00fadio de duas pistas. Ele "
               "n\u00e3o \u00e9 um saturador simples: junta compress\u00e3o de agudos estilo Dubly, "
               "histerese, refor\u00e7o de cabe\u00e7ote, modula\u00e7\u00e3o de velocidade (flutter) e "
               "um limitador ClipOnly3 na sa\u00edda."),
            u ("O resultado \u00e9 o som de uma m\u00e1quina passando por todos os est\u00e1gios. Em "
               "INPUT 0,50, TILT 0,50 e OUTPUT 0,50 o caminho \u00e9 praticamente unit\u00e1rio, mas "
               "a fita nunca \u00e9 transparente por completo: cabe\u00e7ote, histerese e ClipOnly3 "
               "continuam agindo.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O n\u00facleo vem do ToTape9 da Airwindows. A entrada passa por um ganho "
               "(INPUT dobrado ao quadrado). Em seguida vem a codifica\u00e7\u00e3o Dubly: um filtro "
               "de um polo separa a parte aguda, ela \u00e9 comprimida por uma curva logar\u00edtmica "
               "e devolvida ao sinal, com quantidade definida por TILT. No fim da cadeia a "
               "decodifica\u00e7\u00e3o Dubly subtrai essa mesma parcela, invertendo o processo."),
            u ("SHAPE divide o filtro de codifica\u00e7\u00e3o e o de decodifica\u00e7\u00e3o: ele "
               "escolhe quanto de cada lado fica com a parte aguda. O flutter \u00e9 um atraso "
               "modulado de at\u00e9 1000 amostras, com o per\u00edodo variando de forma aleat\u00f3ria "
               "a cada ciclo; a profundidade cresce com a sexta pot\u00eancia de FLUTTER e a "
               "velocidade com o cubo de VEL. FLUTTER."),
            u ("BIAS monta uma escada de nove est\u00e1gios de slew, cada um 1,618 vezes maior que "
               "o anterior. A amostra \u00e9 presa e solta por essa escada, o que produz a "
               "histerese da fita. Em BIAS 0,50 a escada fica inativa; acima e abaixo dela o "
               "material satura de formas diferentes. Depois vem uma histerese fina de 0,0114."),
            u ("O TapeHack2 faz m\u00e9dias m\u00f3veis de 2 a 32 amostras conforme a taxa, "
               "misturadas por um fator de slew: \u00e9 a perda de agudos que a fita imp\u00f5e. O "
               "TapeHack seguinte clampa o sinal em \u00b12,305929 e aplica uma s\u00e9rie de "
               "Taylor \u00edmpar at\u00e9 a 11\u00aa pot\u00eancia, uma aproxima\u00e7\u00e3o suave de "
               "seno. O cabe\u00e7ote entra com um oscilador c\u00fabico e dois biquads em cascata "
               "na frequ\u00eancia escolhida e em 93,75% dela, Q 0,618."),
            u ("Por \u00faltimo, a sa\u00edda (OUTPUT dobrado) e o ClipOnly3, que limita em 0,9085 "
               "com suaviza\u00e7\u00e3o dependente do slew. Lat\u00eancia: 1 amostra, do anel do "
               "ClipOnly3; o flutter \u00e9 modula\u00e7\u00e3o musical e n\u00e3o entra na "
               "compensa\u00e7\u00e3o de atraso da DAW.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("ENTRADA e SA\u00cdDA s\u00e3o ganhos, mas n\u00e3o lineares: o valor \u00e9 dobrado e "
               "elevado ao quadrado (entrada) ou dobrado (sa\u00edda), ent\u00e3o 0,50 \u00e9 o "
               "ponto unit\u00e1rio e os extremos pesam mais do que parecem. INCLINA\u00c7\u00c3O "
               "\u00e9 o TILT: controla quanto agudo o Dubly devolve e quanto a decodifica\u00e7\u00e3o "
               "retira; em 0,50 o par \u00e9 sim\u00e9trico."),
            u ("FORMA (SHAPE) escolhe o reparto do filtro Dubly entre codifica\u00e7\u00e3o e "
               "decodifica\u00e7\u00e3o. BIAS em 0,50 desliga a escada de histerese; abaixo e acima "
               "dela o car\u00e1ter muda. CABE\u00c7OTE define a for\u00e7a do refor\u00e7o de grave "
               "(drive e mistura sobem juntos) e FREQ. CABE\u00c7OTE escolhe o ponto, de 25 a 200 Hz; "
               "0,50 fica em 87,5 Hz."),
            u ("FLUTTER em 0 desliga a modula\u00e7\u00e3o; valores baixos j\u00e1 bastam, porque a "
               "profundidade cresce com a sexta pot\u00eancia. VEL. FLUTTER em 0 congela a "
               "velocidade da varredura. O original preserva uma troca entre canais no c\u00e1lculo "
               "do pr\u00f3ximo per\u00edodo de flutter (L usa R e vice-versa); ela faz parte do som e "
               "n\u00e3o foi corrigida.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para master, comece com tudo em 0,50, suba ENTRADA at\u00e9 o medidor mostrar "
               "trabalho e ajuste SA\u00cdDA para compensar. CABE\u00c7OTE entre 0,25 e 0,50 d\u00e1 "
               "corpo sem embolar; acima de 0,70 o grave fica grande r\u00e1pido. FLUTTER em 0,15 a "
               "0,30 \u00e9 fita de est\u00fadio; acima disso vira efeito."),
            u ("Duplo clique devolve o controle ao padr\u00e3o; as setas fazem ajuste fino. BYPASS "
               "desvia o DSP com rampa de 10 ms e os medidores continuam ativos. O adaptador "
               "processa em double e n\u00e3o aloca no caminho de \u00e1udio.")
        }
    });

    info.controls.push_back ({ "Entrada", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho de entrada, dobrado ao quadrado. 0,50 \u00e9 unit\u00e1rio.") });
    info.controls.push_back ({ u ("Inclina\u00e7\u00e3o"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("TILT: quantidade de Dubly na codifica\u00e7\u00e3o e na decodifica\u00e7\u00e3o.") });
    info.controls.push_back ({ "Forma", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("SHAPE: reparte o filtro Dubly entre codificar e decodificar.") });
    info.controls.push_back ({ "Flutter", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Profundidade da modula\u00e7\u00e3o de velocidade (sexta pot\u00eancia).") });
    info.controls.push_back ({ u ("Vel. flutter"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Velocidade da varredura de flutter (cubo do valor).") });
    info.controls.push_back ({ "Bias", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Offsets da fita. 0,50 desliga a escada de histerese.") });
    info.controls.push_back ({ u ("Cabe\u00e7ote"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Drive e mistura do refor\u00e7o de grave.") });
    info.controls.push_back ({ u ("Freq. cabe\u00e7ote"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Frequ\u00eancia do refor\u00e7o, de 25 a 200 Hz.") });
    info.controls.push_back ({ u ("Sa\u00edda"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho de sa\u00edda, dobrado. 0,50 \u00e9 unit\u00e1rio.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = "Airwindows ToTape9";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O adaptador Forja transcreve o la\u00e7o em double do ToTape9, "
                               "inclusive a troca entre canais no pr\u00f3ximo per\u00edodo de flutter "
                               "e a ordem exata dos est\u00e1gios. Acrescenta medidores de pico, "
                               "bypass suavizado e leitura de clipe; o ClipOnly3 usa o bloco "
                               "compartilhado da su\u00edte. A dither de 64 bits est\u00e1 comentada no "
                               "pr\u00f3prio original; s\u00f3 o gerador pseudoaleat\u00f3rio avan\u00e7a.");

    return info;
}

} // namespace forja::laminador
