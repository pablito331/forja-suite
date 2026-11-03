#pragma once

#include <algorithm>
#include <cmath>

namespace forja::pianoforge
{

// Os 23 parametros do PIANOFORGE. Os 17 primeiros seguem a ordem historica do
// projeto-irmao (pianoforge/src/core/Parameters.hpp); essa ordem e contratada: os
// testes de mapa bit-exato e o estado gravado dependem dela, e renumerar quebraria
// presets salvos. Os dois seguintes (body, ring) sao a camada de corpo acustico da
// Forja, e os quatro ultimos (compressor, molho, cauda, quinta) sao a camada de
// "molho" de worship, todos acrescentados por append -- nada antes de
// Param_ReleaseDamp se move. Nada aqui aloca.
enum ParamId : int
{
    Param_Volume = 0,
    Param_Hammer = 1,
    Param_VelCurve = 2,
    Param_SympRes = 3,
    Param_Lid = 4,
    Param_BassEQ = 5,
    Param_MidEQ = 6,
    Param_HighEQ = 7,
    Param_Chorus = 8,
    Param_Reverb = 9,

    // Ajuste fino: moram na aba MOTOR, mas seguem no APVTS como os outros.
    Param_ReverbType = 10,
    Param_TrimDb = 11,
    Param_Width = 12,
    Param_ReleaseDamp = 13,
    Param_HammerTone = 14,
    Param_SympTune = 15,
    Param_EngineTune = 16,

    // Camada de corpo acustico da Forja (append-only, nao renumera os antigos).
    Param_Body = 17,   // resposta do tampo (curva de radiacao do corpo)
    Param_Ring = 18,   // modos do tampo (a madeira vibrando depois da nota)

    // Camada de "molho" de worship da Forja (append-only). O compressor e o
    // molho sao opt-in (default 0,0 = bypass honesto). O Molho virou um shimmer
    // ESPECTRAL (tecnica do Atlantis, reimplementada) de INTERVALO FIXO: o knob
    // molho dosa o NIVEL/intensidade do halo (0 = off; no talo dosado a 30% do
    // ganho antigo), a quinta dosa a 12a (oitava + 5a) somada por cima da oitava
    // (0 = so oitava), e a cauda dosa o tempo (RT) do resonador espectral. O
    // shift nao realimenta, entao nao compoe nem sobe escada -- so +12 e +12a.
    // Os tres agem so com o molho ligado.
    // Compressor ORIGINAL da Forja (engine/dsp/Compressor.h), NAO o Dyno/Impeto: e o
    // MESMO compressor de canal do mixer do Drumforge (mesmo algoritmo) -- detector de
    // pico estereo-linkado com decaimento, joelho 6 dB e a reducao suavizada em dB por
    // DOIS tempos (attack ~3 ms / release ~120 ms), entao o ganho nao modula dentro do
    // ciclo e NAO degrada o audio. Ratio FIXA em 5:1 (pedido do usuario: cada 1 dB acima
    // do threshold vira 5 dB de reducao); o knob so aprofunda o threshold (0..1 -> -40..0 dB, como o "Thr"
    // do canal). Makeup de laco fechado (camada Forja, teto ~+30 dB) segura o nivel
    // percebido (girar nao derruba o volume -- so esmaga a dinamica); no canal do
    // Drumforge quem recompoe o nivel e o fader/trim. O valor guardado mantem a
    // invariante da suite: 0 = bypass honesto. A INVERSAO e so do gesto no editor: a
    // esquerda achata mais, a direita e o off (o knob mostra 1 - valor do parametro).
    Param_Compressor = 19,   // um knob: achata picos segurando o nivel (makeup)

    Param_Molho = 20,        // nivel/intensidade do halo do shimmer espectral (+12)
    Param_Tail = 21,         // tempo da cauda (RT) do resonador espectral
    Param_Quinta = 22,       // dose da 12a/quinta acima da oitava (0 = so oitava)

    Param_Count = 23
};

// Dominio de cada parametro no DSP. A regra da suite manda o APVTS viver em
// 0..1 (passo 0,01; reverbType com excecao de passo 0,25 no estilo da
// `division`); o engine recebe o valor ja escalonado para este dominio, e o
// DSP que interpreta (la no projeto antigo o knob vivia direto no dominio
// cru; aqui o escalonamento acontece uma so vez, na fronteira).
struct ParamDef
{
    const char* id;      // chave APVTS (ascii)
    const char* label;   // nome curto PT para o disco (utf-8)
    float min;           // minimo no dominio do engine
    float max;           // maximo no dominio do engine
    float default01;     // default ja normalizado em 0..1 (o APVTS ve 0..1)
};

// Defaults de fabrica: alinhados aos dois paineis que o usuario definiu como
// "o padrao" do plugin (aba PIANO + aba MOTOR). O corpo acustico continua
// ligado (body 0,62 / ring 0,50) porque a promessa do Pianoforge e "qualquer
// soundfont soar melhor ao abrir aqui", mas agora com a tampa meia-aberta
// (lid 0,52) e o piano mais domado: volume 0,82, martelo leve (0,16), medios
// realcados (midEQ 0,74), sala Prato, trim -1,9 dB, largura 1,54, abafar 0,53,
// brilho 0,0, afin. res. 0,55. O compressor e o molho entram em 0,0 -- camada
// opt-in, nao mudam o timbre de fabrica ate o usuario girar. A quinta entra em
// 0,0 (so oitava quando o shimmer ligar) e a cauda em 0,50 (RT do meio): so agem
// com o molho ligado. EQs graves/agudos neutros, sem chorus, ressonancia 0,45.
inline constexpr ParamDef kParamDefs[Param_Count] = {
    { "volume",      "Volume",       0.0f,   1.0f,   0.82f },
    { "hammer",      "Martelo",      0.0f,   1.0f,   0.16f },
    { "velCurve",    "Din\u00e2mica", 0.0f,  1.0f,   0.5f  },
    { "sympRes",     "Resson\u00e2ncia", 0.0f, 1.0f, 0.45f },
    { "lid",         "Tampa",        0.0f,   1.0f,   0.52f },
    { "bassEQ",      "Graves",       0.0f,   1.0f,   0.5f  },
    { "midEQ",       "M\u00e9dios",  0.0f,   1.0f,   0.74f },
    { "highEQ",      "Agudos",       0.0f,   1.0f,   0.5f  },
    { "chorus",      "Chorus",       0.0f,   1.0f,   0.0f  },
    { "reverb",      "Reverb",       0.0f,   1.0f,   0.12f },

    { "reverbType",  "Sala",         0.0f,   3.0f,   2.0f / 3.0f   },
    { "trimDb",      "Trim",        -12.0f,  12.0f,  0.420833f },
    { "width",       "Largura",      0.0f,   2.0f,   0.77f  },
    { "releaseDamp", "Abafar",       0.0f,   1.0f,   0.53f  },
    { "hammerTone",  "Brilho",       0.0f,   1.0f,   0.0f   },
    { "sympTune",    "Afin. res.",   0.0f,   1.0f,   0.55f  },
    { "engineTune",  "Afin. geral", -100.0f, 100.0f, 0.5f  },
    { "body",        "Corpo",        0.0f,   1.0f,   0.62f },
    { "ring",        "Madeira",      0.0f,   1.0f,   0.5f  },

    { "compressor",  "Compressor",   0.0f,   1.0f,   0.0f  },
    { "molho",       "Molho",        0.0f,   1.0f,   0.0f  },
    { "tail",        "Cauda",        0.0f,   1.0f,   0.5f  },
    { "quinta",      "Quinta",       0.0f,   1.0f,   0.0f  }
};

// Normalizado (0..1, o que o APVTS guarda) -> dominio do engine. Linear no
// dominio inteiro; reverbType quantiza nos 4 tipos (o antigo recebia o valor
// continuo 0..3 e escolhia por truncar; aqui o passo 0,25 do range garante
// que so chegam posicoes validas, e o arredondamento mantem a tabela lida).
inline float toEngineDomain (int paramId, float value01) noexcept
{
    if (paramId < 0 || paramId >= Param_Count)
        return 0.0f;

    const auto& def = kParamDefs[paramId];

    // Descartar NaN e o unico comportamento seguro: fmin/fmax devolvem o
    // operando NAO-NaN, entao automacao quebrada viraria maxValue (no volume,
    // um glitch do host mandaria o piano a fundo). Heranca do conserto antigo.
    if (! std::isfinite (value01))
        return def.min + def.default01 * (def.max - def.min);

    const auto t = std::clamp (value01, 0.0f, 1.0f);
    auto value = def.min + t * (def.max - def.min);

    if (paramId == Param_ReverbType)
        value = std::floor (std::clamp (value, 0.0f, 3.0f) + 0.5f);

    return value;
}

// Dominio do engine -> normalizado. Exigido pelo estado do ForjaRack e pelos
// testes de mapa: toda ida e volta num valor de grade (passo 0,01, ou 0,25 no
// reverbType) e bit-exata.
inline float fromEngineDomain (int paramId, float engineValue) noexcept
{
    if (paramId < 0 || paramId >= Param_Count)
        return 0.0f;

    if (! std::isfinite (engineValue))
        return kParamDefs[paramId].default01;

    const auto& def = kParamDefs[paramId];
    const auto span = def.max - def.min;

    if (span <= 0.0f)
        return 0.0f;

    return std::clamp ((engineValue - def.min) / span, 0.0f, 1.0f);
}

// Escala do knob do martelo aplicada no note-on. No projeto-irmao o dominio
// 0..35 vivia multiplicando por 0,10 longe do DSP, o que praticamente zerava o
// transiente (martelo a 100% ainda era inaudivel). Aqui o hammer 0..1 chega na
// escala certa e 0,50 da ao knob autoridade real: o default (0,50) injeta um
// drive 0,25 -- cerca de 7x o antigo default -- e o transiente de feltro sai
// audivel desde o primeiro toque. Registrado para o teste poder cita-lo.
inline constexpr float kHammerEscala = 0.50f;

} // namespace forja::pianoforge
