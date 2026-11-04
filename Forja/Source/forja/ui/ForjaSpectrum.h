#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace forja::ui
{

/* RTA de entrada para os cards de curva.
 *
 * Camada de medicao, nao de DSP: consome uma copia do sinal que entra e
 * publica bandas suavizadas em atomicos POD de 4 bytes, sem tocar no caminho
 * de audio. Todas as tabelas (janela, twiddles, permutacao) nascem no
 * construtor, entao prepareToPlay e processBlock seguem sem alocacao. O FFT e
 * propria (radix-2 in-place) para nao arrastar o modulo juce_dsp para o
 * SharedCode de quem so liga juce_audio_processors.
 */
class ForjaSpectrum
{
public:
    static constexpr int windowSize = 2048;
    static constexpr int bandCount = 32;
    static constexpr float floorDb = -90.0f;

    ForjaSpectrum() noexcept
    {
        constexpr double kPi = 3.14159265358979323846;

        double hannSum = 0.0;

        for (int i = 0; i < windowSize; ++i)
        {
            const double w = 0.5 * (1.0 - std::cos (2.0 * kPi * static_cast<double> (i)
                                                     / static_cast<double> (windowSize - 1)));
            hann[static_cast<std::size_t> (i)] = static_cast<float> (w);
            hannSum += w;
        }

        sumHann = static_cast<float> (hannSum);

        for (int t = 0; t < windowSize / 2; ++t)
        {
            const double angle = 2.0 * kPi * static_cast<double> (t)
                                 / static_cast<double> (windowSize);
            cosTable[static_cast<std::size_t> (t)] = static_cast<float> (std::cos (angle));
            sinTable[static_cast<std::size_t> (t)] = static_cast<float> (-std::sin (angle));
        }

        constexpr int order = 11;

        for (int i = 0; i < windowSize; ++i)
        {
            int reversed = 0;

            for (int bit = 0; bit < order; ++bit)
                reversed = (reversed << 1) | ((i >> bit) & 1);

            permutation[static_cast<std::size_t> (i)] = reversed;
        }

        for (int i = 0; i < bandCount; ++i)
            bands[static_cast<std::size_t> (i)].store (floorDb, std::memory_order_relaxed);
    }

    void prepare (double sampleRate) noexcept
    {
        const double rate = sampleRate > 0.0 ? sampleRate : 48000.0;

        for (int i = 0; i < bandCount; ++i)
        {
            const double norm0 = static_cast<double> (i) / static_cast<double> (bandCount);
            const double norm1 = static_cast<double> (i + 1) / static_cast<double> (bandCount);
            const double f0 = 20.0 * std::pow (1000.0, norm0);
            const double f1 = 20.0 * std::pow (1000.0, norm1);

            int lo = std::max (1, static_cast<int> (f0 * windowSize / rate));
            int hi = std::max (lo + 1, static_cast<int> (std::ceil (f1 * windowSize / rate)));

            lo = std::min (lo, windowSize / 2 - 1);
            hi = std::min (hi, windowSize / 2);

            bandLo[static_cast<std::size_t> (i)] = lo;
            bandHi[static_cast<std::size_t> (i)] = hi;
            smoothed[static_cast<std::size_t> (i)] = floorDb;
            bands[static_cast<std::size_t> (i)].store (floorDb, std::memory_order_relaxed);
        }

        writePos = 0;
        ring.fill (0.0f);
    }

    void pushSamples (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (numChannels <= 0 || numSamples <= 0)
            return;

        const bool stereo = numChannels >= 2;

        for (int s = 0; s < numSamples; ++s)
        {
            const float mid = stereo
                ? 0.5f * (channels[0][s] + channels[1][s])
                : channels[0][s];

            ring[static_cast<std::size_t> (writePos)] = mid;
            writePos = (writePos + 1) % windowSize;
        }

        analyze();
    }

    void readBands (float* out, int count) const noexcept
    {
        const int n = std::min (count, bandCount);

        for (int i = 0; i < n; ++i)
            out[i] = bands[static_cast<std::size_t> (i)].load (std::memory_order_relaxed);
    }

private:
    void analyze() noexcept
    {
        for (int i = 0; i < windowSize; ++i)
        {
            const int pos = (writePos + i) % windowSize;
            work[static_cast<std::size_t> (2 * i)] =
                ring[static_cast<std::size_t> (pos)] * hann[static_cast<std::size_t> (i)];
            work[static_cast<std::size_t> (2 * i + 1)] = 0.0f;
        }

        fftInPlace();

        for (int i = 0; i < bandCount; ++i)
        {
            const int lo = bandLo[static_cast<std::size_t> (i)];
            const int hi = bandHi[static_cast<std::size_t> (i)];

            double power = 0.0;

            for (int bin = lo; bin < hi; ++bin)
            {
                const double re = work[static_cast<std::size_t> (2 * bin)];
                const double im = work[static_cast<std::size_t> (2 * bin + 1)];
                power += re * re + im * im;
            }

            const double amplitude =
                2.0 * std::sqrt (power / static_cast<double> (std::max (1, hi - lo)))
                / static_cast<double> (sumHann);

            const float instant = amplitude > 0.0
                ? std::log10 (static_cast<float> (amplitude)) * 20.0f
                : floorDb;

            float& previous = smoothed[static_cast<std::size_t> (i)];

            // Ataque instantaneo, release fixo por atualizacao: a leitura sobe
            // na hora e cai suave, sem piscar entre blocos de tamanhos diferentes.
            previous = instant >= previous ? instant
                                           : std::max (floorDb, previous - 1.6f);

            bands[static_cast<std::size_t> (i)].store (std::min (0.0f, previous),
                                                       std::memory_order_relaxed);
        }
    }

    void fftInPlace() noexcept
    {
        for (int i = 0; i < windowSize; ++i)
        {
            const int j = permutation[static_cast<std::size_t> (i)];

            if (j > i)
            {
                std::swap (work[static_cast<std::size_t> (2 * i)],
                           work[static_cast<std::size_t> (2 * j)]);
                std::swap (work[static_cast<std::size_t> (2 * i + 1)],
                           work[static_cast<std::size_t> (2 * j + 1)]);
            }
        }

        for (int length = 2; length <= windowSize; length *= 2)
        {
            const int half = length / 2;
            const int step = windowSize / length;

            for (int start = 0; start < windowSize; start += length)
            {
                for (int k = 0; k < half; ++k)
                {
                    const float cosv = cosTable[static_cast<std::size_t> (k * step)];
                    const float sinv = sinTable[static_cast<std::size_t> (k * step)];

                    const std::size_t a = static_cast<std::size_t> (2 * (start + k));
                    const std::size_t b = static_cast<std::size_t> (2 * (start + k + half));

                    const float tr = cosv * work[b] - sinv * work[b + 1];
                    const float ti = cosv * work[b + 1] + sinv * work[b];

                    work[b] = work[a] - tr;
                    work[b + 1] = work[a + 1] - ti;
                    work[a] += tr;
                    work[a + 1] += ti;
                }
            }
        }
    }

    std::array<float, windowSize> ring {};
    std::array<float, windowSize> hann {};
    std::array<float, windowSize * 2> work {};
    std::array<float, windowSize / 2> cosTable {};
    std::array<float, windowSize / 2> sinTable {};
    std::array<int, windowSize> permutation {};
    std::array<int, bandCount> bandLo {};
    std::array<int, bandCount> bandHi {};
    std::array<float, bandCount> smoothed {};
    std::array<std::atomic<float>, bandCount> bands {};
    int writePos = 0;
    float sumHann = 1.0f;
};

} // namespace forja::ui
