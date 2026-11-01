#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::minisat
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "MINISAT";
    info.subtitle = u ("SATURADOR DE VÁLVULAS \u00b7 FORJA");
    info.tagline = u ("MINISAT é a válvula do BRASA (Airwindows Tube2) em formato de "
                      "gaveta: um único knob DRIVE leva o sinal do pad documentado "
                      "ao sinal cheio, sem makeup gain e sem mesa de três knobs.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Satura o áudio da DAW com o modelador assimétrico de válvula do "
               "Tube2, na mesma configuração validada do BRASA. O knob DRIVE vai de "
               "0 a 1: no 0 o plugin está exatamente no default documentado do "
               "BRASA (pad de entrada 0,50); no 1 o sinal chega à válvula sem pad. "
               "Mais drive = mais amostras no clamp da válvula = som mais grosso."),
            u ("Não há compensação de nível: a camada Forja não adiciona makeup "
               "gain, normalização ou oversampling. O medidor mostra o que a válvula "
               "entrega de verdade.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("Por dentro roda o mesmo adaptador forja::BrasaDSP do BRASA, porta "
               "fiel de processDoubleReplacing do Tube2 de Chris Johnson (MIT): pad "
               "de entrada, modelador assimétrico, iterações de potência, histerese, "
               "clamp em ±0,52 e dither de 32 bits. Nada do DSP foi alterado."),
            u ("O DRIVE é uma camada Forja documentada que mapeia um knob sobre "
               "dois parâmetros originais: entrada = 0,5 + 0,5 × drive e válvula "
               "fixa em 0,50 (default do BRASA), com trim de saída em 0 dB. Onde a "
               "UI do BRASA mostra três controles, o mini expõe só essa curva.")
        }
    });

    info.sections.push_back ({
        u ("Operação"),
        {
            u ("Suba o DRIVE até o ponto em que a cor do timbre te agrada; o "
               "cartão de curva mostra o joelho da válvula no mapa atual. CLIP no "
               "badge significa saída acima de 0 dBFS: baixe o DRIVE ou controle o "
               "nivel na DAW. BYPASS crossfada com rampa de 10 ms.")
        }
    });

    info.controls.push_back ({ "Drive", u ("0 a 1 · padrão 0"),
                               u ("Camada Forja: mapeia o pad de entrada original "
                                  "(Entrada de 0,50 a 1,00) com Válvula 0,50 e Saída 0 dB "
                                  "fixos nos defaults do BRASA.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado · padrão desligado"),
                               u ("Sinal seco com rampa de 10 ms.") });

    info.provenance.source = "Airwindows Tube2 (BRASA)";
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT · Chris Johnson (Airwindows).");
    info.provenance.commit = "b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O DSP é o adaptador forja::BrasaDSP compartilhado "
                               "com o BRASA (Forja/Source/forja/dsp/ForjaBrasa.h), "
                               "validado por paridade no BRASA. A camada Forja deste "
                               "mini é só o mapeamento do knob DRIVE; os nomes "
                               "originais Entrada, Válvula e Saída ficam acima. Sem "
                               "teste de paridade próprio: a paridade mora no BRASA e "
                               "os testes do mini cobrem o mapeamento, o bypass, a "
                               "ausência de alocação e a gaveta do rack.");

    return info;
}

} // namespace forja::minisat
