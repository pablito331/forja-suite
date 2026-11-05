#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::traco
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "TRACO";
    info.subtitle = u ("EQ GRÁFICO \u00b7 FORJA");
    info.tagline = u ("TRACO é o EQ de bancada: cinco bandas que se desenham "
                      "no gráfico, com cortes de esquina e um de-esser de "
                      "verdade na sexta - e o espectro da entrada e da saída "
                      "vivo atrás da curva.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Equalizador paramétrico de controle gráfico: passa-alto e "
               "passa-baixa nas pontas e cinco bandas flexíveis (campainha, "
               "prateleira baixa, prateleira alta, passa-alta, passa-baixa e "
               "notch). Clicar numa banda abre o painel de controles dela no "
               "centro embaixo do gráfico, como no Pro-Q."),
            u ("A sexta banda é um de-esser dedicado: banda-passa 4-12 kHz com "
               "detector de excesso, redução até -24 dB só na faixa sibilante e "
               "escuta isolada da banda tratada.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O DSP é original da Forja: cadeia série de biquadros com "
               "coeficientes do Audio EQ Cookbook de Robert Bristow-Johnson "
               "(fórmulas públicas), processamento em double e troca de "
               "coeficientes com rampa de 10 ms. Nada vem do código Airwindows; "
               "por isso o TRACO não tem teste de paridade MIT - o que valida é "
               "o forja_traco: resposta medida por tom, cascata, inclinações, "
               "de-esser e zero alocação."),
            u ("O de-esser compara o envelope rápido da banda com a média lenta "
               "da própria banda: sibilo é excesso relativo, não nível. É o "
               "conceito de diferença filtrada já documentado no ESCÓRIA, em "
               "implementação própria, sem lookahead - latência zero medida por "
               "impulso."),
            u ("Atrás da curva, dois RTAs (a camada de medição Forja "
               "ForjaSpectrum, a mesma do CINZÉL e da LIMA) desenham o espectro "
               "da entrada e o da saída. Medição não altera o som e segue ativa "
               "no bypass."),
            u ("Por enquanto as cinco bandas são estáticas. A evolução natural "
               "é deixá-las dinâmicas: cada banda podendo reagir ao nível do "
               "sinal (EQ dinâmico por banda), seguindo a mesma filosofia do "
               "de-esser. É um caminho de atualização futura, não uma promessa "
               "de release.")
        }
    });

    info.sections.push_back ({
        u ("Operação"),
        {
            u ("Arraste o nó para mudar frequência e ganho; o nó selecionado "
               "abre o painel de baixo com tipo, Q, liga/desliga e a escuta da "
               "banda. Duplo-clique no nó zera a banda."),
            u ("OUVIR toca a diferença entre a saída e a entrada da banda: o "
               "que ela está mudando no sinal, isolado. É monitoramento da "
               "camada Forja e afeta a saída enquanto estiver ativo."),
            u ("BYPASS crossfada com rampa de 10 ms; o estado do filtro nunca é "
               "resetado no bypass.")
        }
    });

    info.controls.push_back ({ u ("Bandas 1-5"), u ("0 a 1 por parâmetro"),
                               u ("Tipo (6 posições, passo 0,2), frequência "
                                  "20 Hz-20 kHz log, ganho ±18 dB, Q 0,1-10 log, "
                                  "liga e ouvir.") });
    info.controls.push_back ({ "HP / LP", u ("fc e inclinação"),
                               u ("Cortes globais com 12 ou 24 dB por oitava, "
                                  "Q fixo de Butterworth.") });
    info.controls.push_back ({ u ("De-esser"), u ("centro, largura, alcance"),
                               u ("Banda-passa 4-12 kHz, redução até 24 dB e "
                                  "escuta da banda tratada.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado · padrão desligado"),
                               u ("Sinal seco com rampa de 10 ms.") });

    info.provenance.source = "Audio EQ Cookbook (RBJ) - formulas";
    info.provenance.author = "Robert Bristow-Johnson (matematica); Forja (código)";
    info.provenance.license = u ("Fórmulas de domínio público; implementação e "
                                 "interface originais da Forja (MIT). Nenhum "
                                 "código Airwindows neste plugin.");
    info.provenance.commit = "";
    info.provenance.notes = u ("O conceito de detecção por diferença filtrada "
                               "do de-esser é inspirado no comportamento do "
                               "ESCÓRIA (Airwindows DeBess, MIT) e na resposta "
                               "do compressor da ESA Digital / Soundcraft, "
                               "reimplementados do zero como camada Forja "
                               "original - inspiração de comportamento, nunca "
                               "de código. A medição "
                               "RTA reusa o ForjaSpectrum da suíte. O teste "
                               "forja_traco cobre resposta, cascata, de-esser, "
                               "latência e alocação.");

    return info;
}

} // namespace forja::traco
