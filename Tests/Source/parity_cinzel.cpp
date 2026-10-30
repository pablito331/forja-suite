#include "parity_common.h"

#include "Parametric.h"
#include "forja/dsp/ForjaCinzel.h"

#include <cstdio>

namespace
{

parity::Case makeCase (parity::Signal signal, double levelDb, double frequency,
                       double sampleRate, const std::vector<float>& params)
{
    parity::Case c;
    c.signal = signal;
    c.levelDb = levelDb;
    c.frequency = frequency;
    c.sampleRate = sampleRate;
    c.params = params;
    return c;
}

struct BandProbe
{
    float highFreq = 0.5f;
    float highGain = 0.5f;
    float highReso = 0.5f;
    float highMidFreq = 0.5f;
    float highMidGain = 0.5f;
    float highMidReso = 0.5f;
    float lowMidFreq = 0.5f;
    float lowMidGain = 0.5f;
    float lowMidReso = 0.5f;
};

// RMS of (out - in) over the loud plateau of a 1 kHz burst, in dBFS.
double bandEnergyDb (const BandProbe& probe, float dyn)
{
    forja::dsp::CinzelDSP dsp;
    dsp.prepare (48000.0);

    forja::dsp::CinzelDSP::Parameters params;
    params.highFreq = probe.highFreq;
    params.highGain = probe.highGain;
    params.highReso = probe.highReso;
    params.highMidFreq = probe.highMidFreq;
    params.highMidGain = probe.highMidGain;
    params.highMidReso = probe.highMidReso;
    params.lowMidFreq = probe.lowMidFreq;
    params.lowMidGain = probe.lowMidGain;
    params.lowMidReso = probe.lowMidReso;
    params.mix = 1.0f;
    params.dyn = dyn;
    params.bypass = false;
    dsp.setParameters (params);

    parity::Case c;
    c.signal = parity::Signal::burst;
    c.levelDb = 0.0;
    c.frequency = 1000.0;
    c.sampleRate = 48000.0;
    c.seconds = 5.0;
    c.blockSize = 512;

    std::vector<double> inL, inR;
    parity::generateSignal (c, inL, inR);

    const int frames = (int) inL.size();
    const int windowStart = (int) (1.5 * c.sampleRate);
    const int windowEnd = (int) (3.5 * c.sampleRate);

    std::vector<double> outL ((std::size_t) frames);
    std::vector<double> outR ((std::size_t) frames);
    std::copy (inL.begin(), inL.end(), outL.begin());
    std::copy (inR.begin(), inR.end(), outR.begin());

    for (int offset = 0; offset < frames; offset += c.blockSize)
    {
        const int count = std::min (c.blockSize, frames - offset);
        dsp.processDouble (outL.data() + offset, outR.data() + offset, count);
    }

    double sum = 0.0;
    for (int i = windowStart; i < windowEnd; ++i)
    {
        const double band = outL[(std::size_t) i] - inL[(std::size_t) i];
        sum += band * band;
    }

    const double rms = std::sqrt (sum / (double) (windowEnd - windowStart));
    return 20.0 * std::log10 (std::max (rms, 1.0e-30));
}

} // namespace

bool runCinzelParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (1357u);
    Parametric original (nullptr);
    forja::dsp::CinzelDSP adapter;
    adapter.setNoiseState (seeds.first, seeds.second);

    const std::vector<float> defaults {
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f
    };

    const auto setParameters = [] (Parametric& orig,
                                   forja::dsp::CinzelDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 10; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::CinzelDSP::Parameters params;
        params.highFreq = p[0];
        params.highGain = p[1];
        params.highReso = p[2];
        params.highMidFreq = p[3];
        params.highMidGain = p[4];
        params.highMidReso = p[5];
        params.lowMidFreq = p[6];
        params.lowMidGain = p[7];
        params.lowMidReso = p[8];
        params.mix = p[9];
        params.dyn = p.size() > 10 ? p[10] : 0.0f;
        params.bypass = false;
        dsp.setParameters (params);
    };

    std::vector<parity::Case> cases;

    const struct { parity::Signal signal; double level; double frequency; } signals[] = {
        { parity::Signal::sine, -18.0, 1000.0 },
        { parity::Signal::sine, 0.0, 1000.0 },
        { parity::Signal::sine60, -18.0, 60.0 },
        { parity::Signal::noise, -18.0, 1000.0 },
        { parity::Signal::square, 0.0, 1000.0 },
        { parity::Signal::dcPos, 0.0, 1000.0 },
        { parity::Signal::dcNeg, 0.0, 1000.0 },
        { parity::Signal::silence, 0.0, 1000.0 },
        { parity::Signal::impulse, 0.0, 1000.0 },
        { parity::Signal::step, 0.0, 1000.0 },
        { parity::Signal::burst, 0.0, 1000.0 },
    };

    for (const auto& s : signals)
        cases.push_back (makeCase (s.signal, s.level, s.frequency, 48000.0, defaults));

    for (const double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, sr, defaults));

    // Static path only: dyn stays at 0 so the original and the adapter must
    // match sample by sample.
    for (int paramIndex = 0; paramIndex < 10; ++paramIndex)
    {
        for (const float value : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto p = defaults;
            p[(std::size_t) paramIndex] = value;
            cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, 48000.0, p));
        }
    }

    int failures = 0;
    int index = 0;
    for (const auto& c : cases)
    {
        const auto result = parity::runCase (original, adapter, c, setParameters);
        parity::report ("Cinzel", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Cinzel parity: %d/%d cases passed\n", index - failures, index);

    // Behaviour: dyn must act on the band that is loud, not on its neighbours.
    BandProbe target;
    target.highMidFreq = 0.4f;   // ~1010 Hz
    target.highMidGain = 0.75f;
    const double targetStatic = bandEnergyDb (target, 0.0f);
    const double targetDynamic = bandEnergyDb (target, 0.5f);
    const double targetReduction = targetDynamic - targetStatic;

    BandProbe neighbour;
    neighbour.highFreq = 0.8f;    // ~8.9 kHz, far from the 1 kHz burst
    neighbour.highGain = 0.75f;
    const double neighbourStatic = bandEnergyDb (neighbour, 0.0f);
    const double neighbourDynamic = bandEnergyDb (neighbour, 0.5f);
    const double neighbourReduction = neighbourDynamic - neighbourStatic;

    const bool targetOk = targetReduction <= -6.0;
    const bool neighbourOk = std::abs (neighbourReduction) < 1.0;

    std::printf ("Cinzel dynamics: target %.1f dB [static %.1f / dyn %.1f], neighbour %.2f dB [static %.1f / dyn %.1f]\n",
                 targetReduction, targetStatic, targetDynamic,
                 neighbourReduction, neighbourStatic, neighbourDynamic);

    if (! targetOk)
        std::printf ("FAIL Cinzel dynamics: target band reduced only %.1f dB (need <= -6)\n",
                     targetReduction);
    if (! neighbourOk)
        std::printf ("FAIL Cinzel dynamics: neighbour band moved %.2f dB (need < 1)\n",
                     neighbourReduction);

    forja::dsp::CinzelDSP probe;
    bool ok = failures == 0 && targetOk && neighbourOk;
    const int latency = parity::measureDspLatency (probe, 48000.0);
    if (latency != 0)
    {
        std::printf ("FAIL Cinzel latency: expected 0, measured %d\n", latency);
        ok = false;
    }
    return parity::checkSilence ("Cinzel", probe) && ok;
}
