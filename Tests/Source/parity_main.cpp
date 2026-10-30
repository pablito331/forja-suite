#include "forja/dsp/ForjaMalho.h"
#include "forja/dsp/ForjaPrensa.h"
#include "forja/dsp/ForjaTempera.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

bool runTemperaParity();
bool runPrensaParity();
bool runMalhoParity();

namespace
{

bool checkLatency (const char* name, int measured, int expected)
{
    if (measured != expected)
    {
        std::printf ("FAIL %s latency: expected %d, measured %d\n", name, expected, measured);
        return false;
    }

    return true;
}

template <typename Dsp>
int impulseLatency (Dsp& dsp, double sampleRate)
{
    dsp.prepare (sampleRate);

    constexpr int frames = 16;
    float left[frames] {};
    float right[frames] {};
    left[0] = 0.5f;
    right[0] = 0.5f;

    dsp.process (left, right, frames);

    for (int i = 0; i < frames; ++i)
    {
        if (std::abs (left[i]) > 1.0e-9f || std::abs (right[i]) > 1.0e-9f)
            return i;
    }

    return -1;
}

template <typename Dsp>
bool checkHighRate (const char* name, Dsp& dsp)
{
    constexpr double sampleRate = 705600.0;
    constexpr int frames = 4096;
    dsp.prepare (sampleRate);

    std::vector<float> left ((std::size_t) frames);
    std::vector<float> right ((std::size_t) frames);

    for (int i = 0; i < frames; ++i)
    {
        const double sample = 0.5 * std::sin (2.0 * 3.14159265358979323846 * 1000.0
                                              * (double) i / sampleRate);
        left[(std::size_t) i] = (float) sample;
        right[(std::size_t) i] = (float) sample;
    }

    dsp.process (left.data(), right.data(), frames);

    for (int i = 0; i < frames; ++i)
    {
        if (! std::isfinite (left[(std::size_t) i]) || ! std::isfinite (right[(std::size_t) i]))
        {
            std::printf ("FAIL %s at 705.6 kHz: non-finite output at %d\n", name, i);
            return false;
        }
    }

    return true;
}

template <typename Dsp, typename SetParams>
bool checkMetersAndSilence (const char* name, Dsp& dsp, SetParams&& setParams)
{
    dsp.prepare (48000.0);
    setParams (dsp);

    constexpr int frames = 48000;
    std::vector<float> left ((std::size_t) frames, 0.0f);
    std::vector<float> right ((std::size_t) frames, 0.0f);
    for (int i = 0; i < frames; ++i)
    {
        left[(std::size_t) i] = 0.5f * (float) std::sin (2.0 * 3.14159265358979323846
                                                         * 1000.0 * (double) i / 48000.0);
        right[(std::size_t) i] = left[(std::size_t) i];
    }

    dsp.process (left.data(), right.data(), frames);
    const auto meters = dsp.readMeters (true);

    if (! std::isfinite (meters.gainReductionDb) || meters.gainReductionDb > 0.0f)
    {
        std::printf ("FAIL %s GR meter invalid: %f dB\n", name, (double) meters.gainReductionDb);
        return false;
    }

    std::fill (left.begin(), left.end(), 0.0f);
    std::fill (right.begin(), right.end(), 0.0f);
    dsp.process (left.data(), right.data(), frames);

    // Prensa/Malho keep the previous signal in the ClipOnly2 delay; give the
    // one-sample latency pipeline a few samples to flush before checking.
    constexpr int settle = 8;
    for (int i = settle; i < frames; ++i)
    {
        if (std::abs (left[(std::size_t) i]) > 1.0e-5f
            || std::abs (right[(std::size_t) i]) > 1.0e-5f)
        {
            std::printf ("FAIL %s silence output: %.3e at %d\n",
                         name, (double) std::max (std::abs (left[(std::size_t) i]),
                                                  std::abs (right[(std::size_t) i])), i);
            return false;
        }
    }

    return true;
}

} // namespace

int main()
{
    bool ok = true;

    ok = runTemperaParity() && ok;
    ok = runPrensaParity() && ok;
    ok = runMalhoParity() && ok;

    {
        forja::dsp::TemperaDSP dsp;
        ok = checkLatency ("Tempera", impulseLatency (dsp, 44100.0), 0) && ok;
        ok = checkHighRate ("Tempera", dsp) && ok;
        ok = checkMetersAndSilence ("Tempera", dsp, [] (forja::dsp::TemperaDSP& d)
        {
            forja::dsp::TemperaDSP::Parameters p;
            p.compress = 1.0f;
            d.setParameters (p);
        }) && ok;
    }

    {
        forja::dsp::PrensaDSP dsp;
        ok = checkLatency ("Prensa", impulseLatency (dsp, 44100.0), 1) && ok;
        ok = checkHighRate ("Prensa", dsp) && ok;
        ok = checkMetersAndSilence ("Prensa", dsp, [] (forja::dsp::PrensaDSP& d)
        {
            forja::dsp::PrensaDSP::Parameters p;
            p.pressure = 1.0f;
            d.setParameters (p);
        }) && ok;
    }

    {
        forja::dsp::MalhoDSP dsp;
        ok = checkLatency ("Malho", impulseLatency (dsp, 44100.0), 1) && ok;
        ok = checkHighRate ("Malho", dsp) && ok;
        ok = checkMetersAndSilence ("Malho", dsp, [] (forja::dsp::MalhoDSP& d)
        {
            forja::dsp::MalhoDSP::Parameters p;
            p.compress = 1.0f;
            d.setParameters (p);
        }) && ok;
    }

    std::printf (ok ? "Parity suite: PASS\n" : "Parity suite: FAIL\n");
    return ok ? 0 : 1;
}
