#pragma once

#include <algorithm>
#include <cmath>

namespace forja::pianoforge
{

// Compressor de um knob ORIGINAL da Forja (camada de molho, opt-in). Segue o MESMO
// desenho do compressor de canal do mixer do DRUMFORGE
// (drumforge/.../engine/Compressor.h): detector de pico estereo-linkado com decaimento
// lento, joelho suave de 6 dB e a REDUCION suavizada em dB por DOIS tempos (attack na
// subida, release na descida). E isso que da o caracter de "compressor musical de mesa",
// nao de limitador: a reducao ataca em ~3 ms (finito) e segura por ~120 ms, entao o
// ganho aplicado e praticamente constante ao longo de um ciclo audivel -- nada modula
// dentro da forma de onda, por isso NAO degrada (sem harmonicos, sem "areia").
//
// Diferenca para o canal do Drumforge: aqui e um knob so (0..1) com RATIO FIXA em 5:1
// (cada 1 dB acima do threshold vira 5 dB de reducao; o knob nao toca na razao, so
// aprofunda o threshold: mais squash = threshold mais baixo) e soma um makeup de laco
// fechado sobre a MEDIA da
// reducao para segurar o nivel percebido -- girar nao derruba o volume, ao contrario de um
// ganho direto (foi a reclamacao que motivou este redesenho). O makeup segue a media da
// reducao (que as vezes passa de 30 dB no talo), por isso seu teto e alto (~+30 dB); como o
// perseguidor e LENTO (200 ms), esse teto nao bombeia nem modula a forma de onda -- so
// recompoe o nivel na media, e os picos continuam sendo achados pela reducao rapida.
//
// amount 0..1 ja escalonado pelo engine. Em 0 (ou perto) o process passa a amostra ilesa
// -- bypass honesto, sem alocacao, sem clique. Nenhum static, nada aloca no caminho de
// audio; o prepare so re-acerta os coeficientes de tempo.
class WorshipCompressor
{
public:
    void prepare (double newSampleRate) noexcept
    {
        const double sr = newSampleRate > 0.0 ? newSampleRate : 48000.0;

        attackCoefficient  = smoothingCoefficient (3.0,   sr);  // subida da reducao (musical)
        releaseCoefficient = smoothingCoefficient (120.0, sr);  // descida: segura entre notas
        detectorRelease    = smoothingCoefficient (60.0,  sr);  // decaimento do detector de pico
        makeupSmoothing    = smoothingCoefficient (200.0, sr);  // media lenta p/ o makeup

        reset();
    }

    void reset() noexcept
    {
        detector       = 0.0;
        reductionDb    = 0.0;
        avgReductionDb = 0.0;
    }

    void setAmount (float a) noexcept
    {
        amount  = std::clamp (a, 0.0f, 1.0f);
        enabled = amount > 1e-4f;
        recomputeCurve();
    }

    void process (float* left, float* right, int numSamples) noexcept
    {
        if (! enabled)
        {
            // Bypass honesto: amostras intactas. Drena o seguidor p/ o neutro -- o
            // proximo ligar comeca com makeup 0 (nao bomba nem da salto de ganho).
            detector       = 0.0;
            reductionDb    = 0.0;
            avgReductionDb = 0.0;
            return;
        }

        const float slope = 1.0f - 1.0f / ratio;

        for (int i = 0; i < numSamples; ++i)
        {
            const float peak = std::max (std::abs (left[i]), std::abs (right[i]));

            // Detector de pico com decaimento: sobe na hora do pico, cai em escala de
            // dezenas de ms. Nada de salto de GANHO -- o que salta e o nivel detectado.
            if (peak > detector)
                detector = peak;
            else
                detector += (peak - detector) * detectorRelease;

            const float levelDb = detector > 1.0e-6 ? 20.0f * std::log10 (static_cast<float> (detector))
                                                    : -120.0f;
            const float over = levelDb - thresholdDb;

            // Joelho suave de 6 dB (igual ao canal do Drumforge): quadratico perto do
            // threshold, linear (razao) acima dele.
            float targetReduction = 0.0f;
            if (over > kneeDb * 0.5f)
                targetReduction = over * slope;
            else if (over > -kneeDb * 0.5f)
            {
                const float k = (over + kneeDb * 0.5f) / kneeDb;
                targetReduction = slope * k * k * kneeDb * 0.5f;
            }

            targetReduction = std::min (targetReduction, 48.0f);

            // A REDUCION (em dB) e suavizada em dois tempos -- attack na subida, release
            // na descida. E o coracao do "compressor musical": o ganho aplicado se move
            // em milissegundos, nao amostra a amostra, entao nao modula a forma de onda.
            const double coefficient = targetReduction > reductionDb ? attackCoefficient
                                                                      : releaseCoefficient;
            reductionDb += (static_cast<double> (targetReduction) - reductionDb) * coefficient;

            // Makeup de laco fechado sobre a MEDIA da reducao: recompoe o nivel percebido
            // sem anular o squash (os picos continuam reduzidos). Teto de ~+30 dB -- alto
            // o bastante para acompanhar a reducao do talo sem derrubar o volume, mas num
            // perseguidor lento (200 ms) que nao bombeia.
            avgReductionDb += (static_cast<double> (reductionDb) - avgReductionDb) * makeupSmoothing;
            const float makeupDb = std::clamp (static_cast<float> (avgReductionDb), 0.0f, 30.0f);

            const float gain = dbToGain (makeupDb - static_cast<float> (reductionDb));
            left[i]  *= gain;
            right[i] *= gain;
        }
    }

private:
    static double smoothingCoefficient (double timeMilliseconds, double sampleRate) noexcept
    {
        // Suavizador de um polo em funcao do tempo em ms: k = 1 - exp(-1 / (tau*sr)).
        const double samples = (timeMilliseconds * 0.001) * sampleRate;
        return samples > 1.0 ? 1.0 - std::exp (-1.0 / samples) : 1.0;
    }

    static float dbToGain (float decibels) noexcept
    {
        return std::pow (10.0f, decibels * 0.05f);
    }

    void recomputeCurve() noexcept
    {
        // EXATAMENTE o compressor de canal do Drumforge: mesma faixa de threshold do
        // "Thr" do canal (0..1 -> -40..0 dB, Mapping::thresholdDb) e a ratio pedida fixa
        // em 5:1. O knob (amount) e o threshold: 0 -> limiar em 0 dB (nao encosta em
        // nada; com bypass honesto em 0), 1 -> limiar em -40 dB (talo). Joelho 6 dB,
        // detector e teto de 48 dB de reducao sao os mesmos do canal.
        thresholdDb = -40.0f * amount;   // -40..0 dB, igual ao Thr do canal
        ratio       = 5.0f;              // fixa em 5:1 (pedido do usuario)
    }

    static constexpr float kneeDb = 6.0f;

    float amount = 0.0f;
    bool  enabled = false;

    float thresholdDb = -12.0f;
    float ratio = 2.0f;

    double detector = 0.0;
    double reductionDb = 0.0;
    double avgReductionDb = 0.0;

    double attackCoefficient = 0.01;
    double releaseCoefficient = 0.001;
    double detectorRelease = 0.001;
    double makeupSmoothing = 0.0005;
};

} // namespace forja::pianoforge
