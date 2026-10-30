#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace parity
{

enum class Signal
{
    sine,
    sine60,
    square,
    noise,
    dcPos,
    dcNeg,
    silence,
    impulse,
    step,
    burst
};

struct Case
{
    double sampleRate = 48000.0;
    double seconds = 5.0;
    int blockSize = 512;
    Signal signal = Signal::sine;
    double frequency = 1000.0;
    double levelDb = -12.0;
    std::vector<float> params {};
};

struct Compare
{
    double maxAbs = 0.0;
    double rms = 0.0;
    bool finite = true;
    bool ok = true;

    double worstSample = 0.0;
    std::int64_t worstIndex = 0;
    double worstOriginal = 0.0;
    double worstAdapter = 0.0;
};

inline double dbToLinear (double db)
{
    return std::pow (10.0, db / 20.0);
}

inline void generateSignal (const Case& c,
                            std::vector<double>& left,
                            std::vector<double>& right)
{
    const int frames = (int) std::lround (c.seconds * c.sampleRate);
    left.assign ((std::size_t) frames, 0.0);
    right.assign ((std::size_t) frames, 0.0);

    const double amplitude = dbToLinear (c.levelDb);
    std::uint32_t noiseState = 0x12345678u;

    for (int i = 0; i < frames; ++i)
    {
        const double t = (double) i / c.sampleRate;
        const double phase = 2.0 * 3.14159265358979323846 * c.frequency * t;

        switch (c.signal)
        {
            case Signal::sine:
            case Signal::sine60:
                left[i] = amplitude * std::sin (phase);
                right[i] = amplitude * std::sin (phase + 0.37);
                break;

            case Signal::square:
                left[i] = amplitude * (std::sin (phase) >= 0.0 ? 1.0 : -1.0);
                right[i] = amplitude * (std::sin (phase + 0.37) >= 0.0 ? 1.0 : -1.0);
                break;

            case Signal::noise:
                noiseState = noiseState * 1664525u + 1013904223u;
                left[i] = amplitude * ((double) (noiseState >> 8) / 8388608.0 - 1.0);
                noiseState = noiseState * 1664525u + 1013904223u;
                right[i] = amplitude * ((double) (noiseState >> 8) / 8388608.0 - 1.0);
                break;

            case Signal::dcPos:
                left[i] = amplitude;
                right[i] = amplitude;
                break;

            case Signal::dcNeg:
                left[i] = -amplitude;
                right[i] = -amplitude;
                break;

            case Signal::silence:
                left[i] = 0.0;
                right[i] = 0.0;
                break;

            case Signal::impulse:
                left[i] = (i == 0) ? amplitude : 0.0;
                right[i] = (i == 0) ? amplitude : 0.0;
                break;

            case Signal::step:
                left[i] = (i < frames / 2 ? dbToLinear (-40.0) : amplitude) * std::sin (phase);
                right[i] = (i < frames / 2 ? dbToLinear (-40.0) : amplitude)
                         * std::sin (phase + 0.37);
                break;

            case Signal::burst:
            {
                const bool loud = i >= frames / 4 && i < (3 * frames) / 4;
                const double amp = loud ? amplitude : amplitude * 0.01;
                left[i] = amp * std::sin (phase);
                right[i] = amp * std::sin (phase + 0.37);
                break;
            }
        }
    }
}

template <typename Original, typename Adapter, typename SetParameters>
Compare runCase (Original& original,
                 Adapter& adapter,
                 const Case& c,
                 SetParameters&& setParameters)
{
    original.setSampleRate (c.sampleRate);
    adapter.setSampleRate (c.sampleRate);
    setParameters (original, adapter, c.params);

    std::vector<double> inL;
    std::vector<double> inR;
    generateSignal (c, inL, inR);

    const int frames = (int) inL.size();
    std::vector<double> origL ((std::size_t) c.blockSize);
    std::vector<double> origR ((std::size_t) c.blockSize);
    std::vector<double> adapterL ((std::size_t) c.blockSize);
    std::vector<double> adapterR ((std::size_t) c.blockSize);

    Compare result;
    double sumSquares = 0.0;
    std::int64_t compared = 0;

    for (int offset = 0; offset < frames; offset += c.blockSize)
    {
        const int count = std::min (c.blockSize, frames - offset);

        std::copy_n (inL.begin() + offset, count, origL.begin());
        std::copy_n (inR.begin() + offset, count, origR.begin());
        std::copy_n (inL.begin() + offset, count, adapterL.begin());
        std::copy_n (inR.begin() + offset, count, adapterR.begin());

        double* origPtrs[2] = { origL.data(), origR.data() };
        double* adapterPtrs[2] = { adapterL.data(), adapterR.data() };

        original.processDoubleReplacing (origPtrs, origPtrs, count);
        adapter.processDouble (adapterPtrs[0], adapterPtrs[1], count);

        for (int i = 0; i < count; ++i)
        {
            const double a = origL[(std::size_t) i];
            const double b = adapterL[(std::size_t) i];
            const double ar = origR[(std::size_t) i];
            const double br = adapterR[(std::size_t) i];

            if (! std::isfinite (a) || ! std::isfinite (b)
                || ! std::isfinite (ar) || ! std::isfinite (br))
            {
                result.finite = false;
                result.ok = false;
            }

            const double error = std::max (std::abs (a - b), std::abs (ar - br));
            if (error > result.maxAbs)
            {
                result.maxAbs = error;
                result.worstSample = (double) (offset + i);
                result.worstIndex = offset + i;
                result.worstOriginal = a;
                result.worstAdapter = b;
            }

            sumSquares += (a - b) * (a - b) + (ar - br) * (ar - br);
            compared += 2;
        }
    }

    result.rms = compared > 0 ? std::sqrt (sumSquares / (double) compared) : 0.0;
    result.ok = result.ok && result.finite
             && result.maxAbs <= 1.0e-9
             && result.rms <= 1.0e-7;
    return result;
}

// Measures the latency in samples of a prepared processor by feeding a single
// impulse and locating the first non-tiny sample. Returns -1 if nothing showed
// up inside maxLatency.
template <typename ProcessFn>
int measureLatency (ProcessFn&& process, int maxLatency = 8192)
{
    constexpr int lead = 64;
    const int total = lead + maxLatency;

    std::vector<double> left ((std::size_t) total, 0.0);
    std::vector<double> right ((std::size_t) total, 0.0);
    left[(std::size_t) lead] = 1.0;
    right[(std::size_t) lead] = 1.0;

    process (left.data(), right.data(), total);

    double peak = 0.0;
    for (double value : left)
        peak = std::max (peak, std::abs (value));

    if (peak <= 0.0)
        return -1;

    const double threshold = peak * 1.0e-6;
    for (int i = 0; i < total; ++i)
        if (std::abs (left[(std::size_t) i]) > threshold)
            return i - lead;

    return -1;
}

template <typename Dsp>
int measureDspLatency (Dsp& dsp, double sampleRate, int maxLatency = 8192)
{
    dsp.prepare (sampleRate);
    return measureLatency ([&dsp] (double* left, double* right, int count)
    {
        dsp.processDouble (left, right, count);
    }, maxLatency);
}

// A DSP that only adds dither seeds with fpd * 1.18e-17 leaves roughly 1e-8
// behind for silence input; anything near 1e-5 means a stuck filter state.
template <typename Dsp>
bool checkSilence (const char* name, Dsp& dsp)
{
    constexpr int frames = 48000;
    dsp.prepare (48000.0);
    std::vector<double> left ((std::size_t) frames, 0.0);
    std::vector<double> right ((std::size_t) frames, 0.0);
    dsp.processDouble (left.data(), right.data(), frames);

    for (int i = 8; i < frames; ++i)
    {
        if (std::abs (left[(std::size_t) i]) > 1.0e-5
            || std::abs (right[(std::size_t) i]) > 1.0e-5)
        {
            std::printf ("FAIL %s silence output: %.3e at %d\n", name,
                         std::max (std::abs (left[(std::size_t) i]),
                                   std::abs (right[(std::size_t) i])), i);
            return false;
        }
    }

    return true;
}

inline std::pair<std::uint32_t, std::uint32_t> simulateAirwindowsNoiseSeed (unsigned seed)
{
    const auto next = []()
    {
        std::uint32_t value = 1u;
        while (value < 16386u)
            value = (std::uint32_t) std::rand() * (std::uint32_t) 0xffffffffu;
        return value;
    };

    std::srand (seed);
    const auto left = next();
    const auto right = next();

    std::srand (seed);
    return { left, right };
}

inline void report (const char* model, int caseIndex, const Case& c, const Compare& result)
{
    if (result.ok)
        return;

    std::printf ("FAIL %s case %d (sr=%.0f signal=%d): max abs %.3e at sample %lld "
                 "(orig %.17g adapt %.17g), rms %.3e\n",
                 model, caseIndex, c.sampleRate, (int) c.signal,
                 result.maxAbs, (long long) result.worstIndex,
                 result.worstOriginal, result.worstAdapter, result.rms);
}

} // namespace parity
