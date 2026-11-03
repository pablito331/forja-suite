#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace forja::pianoforge
{

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

// ---- Lowpass de um polo (inline, sem estado externo) -----------------------
// Doma os agudos do halo espectral (o chiado que sobra da fase aleatoria nos
// bins altos). Um polo basta e nao aloca.
class OnePoleLowpass
{
public:
    void setCutoff (float sampleRate, float hz) noexcept
    {
        const float rc = 1.0f / (2.0f * static_cast<float> (M_PI) * hz);
        y = 0.0f;
        coef = std::exp (-1.0f / (sampleRate * rc));
    }

    inline float process (float in) noexcept
    {
        y = in * (1.0f - coef) + y * coef;
        return y;
    }

private:
    float coef = 0.0f;
    float y = 0.0f;
};

// ---- FFT radix-2 in-place (re/im paralelos), sem alocacao -------------------
// Iterativa, com twiddle rotacionado no laco interno (cos/sin uma vez por
// estagio). N precisa ser potencia de 2. Roda apenas a cada hop, nao por amostra.
inline void spectralFFT (float* re, float* im, int n, bool inverse) noexcept
{
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            std::swap (re[i], re[j]);
            std::swap (im[i], im[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * M_PI / static_cast<double> (len) * (inverse ? -1.0 : 1.0);
        const float wlRe = static_cast<float> (std::cos (ang));
        const float wlIm = static_cast<float> (std::sin (ang));
        const int half = len >> 1;

        for (int i = 0; i < n; i += len)
        {
            float wr = 1.0f, wi = 0.0f;
            for (int k = 0; k < half; ++k)
            {
                const int a = i + k;
                const int b = i + k + half;
                const float vre = re[b] * wr - im[b] * wi;
                const float vim = re[b] * wi + im[b] * wr;
                re[b] = re[a] - vre;
                im[b] = im[a] - vim;
                re[a] += vre;
                im[a] += vim;

                const float nwr = wr * wlRe - wi * wlIm;
                wi = wr * wlIm + wi * wlRe;
                wr = nwr;
            }
        }
    }

    if (inverse)
    {
        const float s = 1.0f / static_cast<float> (n);
        for (int i = 0; i < n; ++i)
        {
            re[i] *= s;
            im[i] *= s;
        }
    }
}

// ---- Shimmer espectral (tecnica do Atlantis Reverb, reimplementada) ---------
// Nao e pitch-shifter no tempo nem delay musical. E um resonador no dominio da
// frequencia: corta o sinal em graos (~150 ms, janela de Hann, sobreposicao 4x),
// guarda a ENERGIA (magnitude) por bin num buffer que decai exponencialmente (o
// "reverb" espectral -- a cauda e o tempo de decaimento) e reconstrui com FASE
// ALEATORIA. A fase aleatoria e o que mata a "voz robotizada": nao sobra
// formante nem transiente coerente para deslocar -- vira uma nuvem difusa.
//
// O SHIFT e de intervalo FIXO, nao uma escada que sobe sem fim. A cada hop uma
// copia da energia do core (ja difusa) e reposicionada em 2k (exatamente +1
// oitava) e, na dose da quinta, em 3k (+12a = oitava + 5a). A copia NAO e
// realimentada no core, entao nunca recompoe: nada de 3k->9k->27k (que dava 9a/
// 27a dissonantes) nem de subir oitava atras de oitava ate o agudo. Resposta do
// usuario: "quantas +12 eu pulo?" = exatamente uma (+ a 12a da Quinta). Mesma
// familia de ideia do Atlantis (Geraint Luff), reimplementada do zero, sem
// copiar o codigo dele.
//
// Zero alocacao no caminho de audio: todos os buffers nascem em prepare().
class SpectralShimmer
{
public:
    void prepare (float sampleRate) noexcept
    {
        sr = sampleRate > 1000.0f ? sampleRate : 44100.0f;

        int n = 2048;
        while (n < 8192 && static_cast<double> (n) < 0.15 * sr)
            n <<= 1;                                   // grao alvo ~150 ms
        fftSize = n;
        bins = fftSize / 2 + 1;
        hop = fftSize / 4;
        mask = 2 * fftSize - 1;                        // aneis tem tamanho 2*fftSize

        re.assign (static_cast<std::size_t> (fftSize), 0.0f);
        im.assign (static_cast<std::size_t> (fftSize), 0.0f);
        energyL.assign (static_cast<std::size_t> (bins), 0.0f);
        energyR.assign (static_cast<std::size_t> (bins), 0.0f);
        shiftL.assign (static_cast<std::size_t> (bins), 0.0f);
        shiftR.assign (static_cast<std::size_t> (bins), 0.0f);

        window.resize (static_cast<std::size_t> (fftSize));
        for (int i = 0; i < fftSize; ++i)
            window[static_cast<std::size_t> (i)] =
                static_cast<float> (0.5 * (1.0 - std::cos (2.0 * M_PI * (i + 0.5) / fftSize)));

        ringInL.assign (static_cast<std::size_t> (2 * fftSize), 0.0f);
        ringInR.assign (static_cast<std::size_t> (2 * fftSize), 0.0f);
        olaL.assign (static_cast<std::size_t> (2 * fftSize), 0.0f);
        olaR.assign (static_cast<std::size_t> (2 * fftSize), 0.0f);

        writePos = 0;
        hopCtr = 0;
        rng = 1u;

        lpL.setCutoff (sr, kShiftLpHz);
        lpR.setCutoff (sr, kShiftLpHz);

        // A FFT direta NAO divide por N e o IFFT divide: a ida-e-volta e a
        // identidade. Como reconstruimos pela magnitude |X| com fase aleatoria,
        // o nivel do halo sai da ordem da entrada; o shimmerLevel do Molho dosa.
        magScale = 1.0f;

        applyDecay();
    }

    void setTone (float t) noexcept
    {
        tone = std::fmax (0.0f, std::fmin (1.0f, t));
    }

    void setDecay (float d) noexcept
    {
        decayKnob = std::fmax (0.0f, std::fmin (1.0f, d));
        applyDecay();
    }

    // Escreve APENAS o halo na saida (consome a entrada L/R). A mistura com o
    // seco acontece no Molho.
    void process (float* left, float* right, int frames) noexcept
    {
        for (int s = 0; s < frames; ++s)
        {
            const float inL = left[s];
            const float inR = right[s];

            ringInL[static_cast<std::size_t> (writePos & mask)] = inL;
            ringInR[static_cast<std::size_t> (writePos & mask)] = inR;
            ++writePos;

            if (++hopCtr >= hop)
            {
                hopCtr = 0;
                doBlock();
            }

            // A saida tem latencia de fftSize amostras (janela de analise). O
            // slot lido aqui fica UMA amostra antes do inicio do grao que o
            // synthAdd acabou de escrever -- senao zerariamos a amostra k=0 recem
            // somada. O anel de 2*fftSize da folga para o overlap-add nao colidir.
            const int oIdx = (writePos - fftSize - 1) & mask;
            left[s]  = lpL.process (olaL[static_cast<std::size_t> (oIdx)]);
            right[s] = lpR.process (olaR[static_cast<std::size_t> (oIdx)]);
            olaL[static_cast<std::size_t> (oIdx)] = 0.0f;   // libera o slot p/ o proximo grao
            olaR[static_cast<std::size_t> (oIdx)] = 0.0f;
        }
    }

private:
    void applyDecay() noexcept
    {
        const double rt = 0.15 + static_cast<double> (decayKnob) * (1.6 - 0.15);
        baseDecay = static_cast<float> (std::exp (-1.0 * hop / (sr * rt)));
    }

    float rand01() noexcept
    {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float> ((rng >> 8) & 0xFFFFFF) / static_cast<float> (0x1000000);
    }

    void doBlock() noexcept
    {
        analyseAdd (ringInL, energyL);
        analyseAdd (ringInR, energyR);
        buildOverlay (energyL, shiftL);
        buildOverlay (energyR, shiftR);
        synthAdd (shiftL, olaL);
        synthAdd (shiftR, olaR);
    }

    // Monta a camada de intervalo FIXO a partir do core (nao realimenta): uma
    // copia do espectro difuso vai para 2k (+1 oitava) sempre, e uma fracao
    // proporcional a `tone` vai para 3k (+12a = oitava + 5a). Como le do core e
    // escreve num buffer separado, a energia nunca recompoe -- so os intervalos
    // consonantes 2x e 3x, e o quanto de cada. O kernel 0,25/0,5/0,25 alisa a
    // copia de oitava (evita pitos de bin unico); a 12a entra direta.
    void buildOverlay (const std::vector<float>& core, std::vector<float>& shift) noexcept
    {
        for (int k = 0; k < bins; ++k)
            shift[static_cast<std::size_t> (k)] = 0.0f;

        const float wTw = tone;

        for (int k = 1; k < bins; ++k)
        {
            const float e = core[static_cast<std::size_t> (k)];
            if (e <= 0.0f)
                continue;

            const int octave = k * 2;
            if (octave < bins)
            {
                const int lo = octave - 1 >= 1 ? octave - 1 : 1;
                const int hi = octave + 1 < bins ? octave + 1 : bins - 1;
                shift[static_cast<std::size_t> (lo)] += 0.25f * e;
                shift[static_cast<std::size_t> (octave)] += 0.5f * e;
                shift[static_cast<std::size_t> (hi)] += 0.25f * e;
            }
            if (wTw > 0.0f)
            {
                const int twelfth = k * 3;
                if (twelfth < bins)
                    shift[static_cast<std::size_t> (twelfth)] += wTw * e;
            }
        }
    }

    // Le a janela dos ultimos fftSize amostras (fftSize atras de writePos, pois
    // writePos ja aponta para a proxima) e acumula |X[k]|^2 com decaimento+damping.
    void analyseAdd (const std::vector<float>& ring, std::vector<float>& energy) noexcept
    {
        const int start = (writePos - fftSize) & mask;

        for (int k = 0; k < fftSize; ++k)
        {
            const float x = ring[static_cast<std::size_t> ((start + k) & mask)]
                              * window[static_cast<std::size_t> (k)];
            re[static_cast<std::size_t> (k)] = x;
            im[static_cast<std::size_t> (k)] = 0.0f;
        }

        spectralFFT (re.data(), im.data(), fftSize, false);

        const double denom = static_cast<double> (bins - 1);
        for (int k = 0; k < bins; ++k)
        {
            const float rr = re[static_cast<std::size_t> (k)];
            const float ii = im[static_cast<std::size_t> (k)];
            const float mag2 = rr * rr + ii * ii;

            const double norm = static_cast<double> (k) / denom;
            const float damp = static_cast<float> (
                std::pow (static_cast<double> (baseDecay), 1.0 + kDampSlope * norm * norm));

            energy[static_cast<std::size_t> (k)] =
                energy[static_cast<std::size_t> (k)] * damp + mag2 * (1.0f - damp);   // EMA
        }
    }

    // Monta o espectro (magnitude da energia + fase aleatoria, simetria
    // Hermitiana para saida real), IFFT e soma com janela no overlap-add.
    void synthAdd (std::vector<float>& energy, std::vector<float>& ola) noexcept
    {
        for (int k = 0; k < bins; ++k)
        {
            const float mag = std::sqrt (std::max (0.0f, energy[static_cast<std::size_t> (k)])) * magScale;
            const float ph = rand01() * 2.0f * static_cast<float> (M_PI);
            const float a = mag * std::cos (ph);
            const float b = mag * std::sin (ph);
            re[static_cast<std::size_t> (k)] = a;
            im[static_cast<std::size_t> (k)] = b;

            const int mirror = fftSize - k;
            if (mirror < fftSize && mirror != k)
            {
                re[static_cast<std::size_t> (mirror)] = a;
                im[static_cast<std::size_t> (mirror)] = -b;
            }
        }

        spectralFFT (re.data(), im.data(), fftSize, true);

        const int start = (writePos - fftSize) & mask;
        for (int k = 0; k < fftSize; ++k)
        {
            const int idx = (start + k) & mask;
            ola[static_cast<std::size_t> (idx)] += re[static_cast<std::size_t> (k)]
                                                   * window[static_cast<std::size_t> (k)];
        }
    }

    static constexpr float kShiftLpHz = 5400.0f;   // doma o chiado dos bins altos
    static constexpr double kDampSlope = 3.0;       // inclinacao do damping dos agudos

    float sr = 44100.0f;
    int fftSize = 2048;
    int bins = 1025;
    int hop = 512;
    int mask = 0;
    int writePos = 0;
    int hopCtr = 0;
    unsigned int rng = 1u;

    float tone = 0.0f;
    float decayKnob = 0.5f;
    float baseDecay = 0.99f;
    float magScale = 1.0f;

    std::vector<float> re, im, energyL, energyR, shiftL, shiftR, window, ringInL, ringInR, olaL, olaR;

    OnePoleLowpass lpL;
    OnePoleLowpass lpR;
};

// ---- Molho: shimmer espectral estilo Atlantis ------------------------------
// Camada de "molho" de worship: halo difuso que sobe em oitava (e 12a) por cima
// do sinal ja espacializado. Nao e delay nem pitch-shifter no tempo -- e o
// resonador de espectro (SpectralShimmer) acima.
//
// Tres controles (a cauda mora no MOTOR):
//   molho (0..1)  = nivel da camada de shimmer (+12 fixo, com a 12a da quinta).
//                   0 = off. No talo dosamos a 30% do ganho antigo (pedido do
//                   usuario: o max anterior era alto demais).
//   quinta (0..1) = dose da 12a (oitava+5a) somada por cima da oitava. 0 = so +12.
//   cauda (0..1)  = tempo da cauda (RT) do resonador. = decay_seconds.
//
// O intervalo e FIXO (nao sobe escada): o halo e exatamente +1 oitava e, com a
// quinta, +12a -- sempre harmonico, sem 9a/27a de recomposicao.
//
// Knob do molho em 0 = bypass honesto (early return; nao encosta no seco).
// Zero alocacao no caminho de audio.
class Molho
{
public:
    void prepare (float sampleRate) noexcept
    {
        shimmer.prepare (sampleRate);
        sr = sampleRate > 1000.0f ? sampleRate : 44100.0f;
        panPhase = 0.0f;
        panInc = 2.0f * static_cast<float> (M_PI) * kPanHz / sr;
        applyMix();
    }

    void setAmount (float amount) noexcept
    {
        const float k = std::fmax (0.0f, std::fmin (1.0f, amount));
        active = k >= 0.001f;
        knob = k;
        applyMix();
    }

    void setTone (float tone) noexcept
    {
        shimmer.setTone (std::fmax (0.0f, std::fmin (1.0f, tone)));
    }

    void setTail (float tail) noexcept
    {
        shimmer.setDecay (std::fmax (0.0f, std::fmin (1.0f, tail)));
    }

    bool isActive() const noexcept { return active; }

    void process (float* left, float* right, std::size_t numFrames) noexcept
    {
        if (! active)
            return;

        const std::size_t n = std::min (numFrames, kMaxFrames);

        for (std::size_t i = 0; i < n; ++i)
        {
            dryL[i] = left[i];
            dryR[i] = right[i];
        }

        // O shimmer consome a entrada e devolve APENAS o halo (com latencia do
        // grao). Misturamos o halo sobre o seco preservado acima.
        shimmer.process (left, right, static_cast<int> (n));

        for (std::size_t i = 0; i < n; ++i)
        {
            const float pan = std::sin (panPhase);
            panPhase += panInc;
            if (panPhase >= 2.0f * static_cast<float> (M_PI))
                panPhase -= 2.0f * static_cast<float> (M_PI);

            const float panAmt = panDepth * pan;
            left[i]  = dryL[i] + left[i] * shimmerLevel * (1.0f - panAmt);
            right[i] = dryR[i] + right[i] * shimmerLevel * (1.0f + panAmt);
        }
    }

private:
    void applyMix() noexcept
    {
        const float k = active ? knob : 0.0f;
        shimmerLevel = k * 0.42f;                       // nivel do halo (30% do max antigo 1,4)
        const float pan = std::fmax (0.0f, std::fmin (1.0f, (k - 0.25f) / 0.75f));
        panDepth = pan * 0.25f;
    }

    static constexpr float kPanHz = 0.20f;
    static constexpr std::size_t kMaxFrames = 4096;   // cobre o maior bloco de host

    SpectralShimmer shimmer;
    std::array<float, kMaxFrames> dryL{};
    std::array<float, kMaxFrames> dryR{};

    float sr = 44100.0f;
    float panPhase = 0.0f;
    float panInc = 0.0f;

    bool active = false;
    float knob = 0.0f;
    float shimmerLevel = 0.0f;
    float panDepth = 0.0f;
};

} // namespace forja::pianoforge
