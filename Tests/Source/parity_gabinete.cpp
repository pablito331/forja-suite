#include "parity_common.h"

#include "Cabs2.h"
#include "forja/dsp/ForjaGabinete.h"

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
    c.seconds = 3.0;
    return c;
}

} // namespace

bool runGabineteParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (5301u);
    Cabs2 original (nullptr);
    forja::dsp::GabineteDSP adapter;
    adapter.setNoiseState (seeds.first, seeds.second);

    const std::vector<float> defaults { 1.0f, 0.0f };

    const auto setParameters = [] (Cabs2& orig,
                                   forja::dsp::GabineteDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 2; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::GabineteDSP::Parameters params;
        params.lowpass = p[0];
        params.hipass = p[1];
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

    for (int paramIndex = 0; paramIndex < 2; ++paramIndex)
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
        parity::report ("Gabinete", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Gabinete parity: %d/%d cases passed\n", index - failures, index);

    forja::dsp::GabineteDSP probe;
    bool ok = failures == 0;
    const int latency = parity::measureDspLatency (probe, 48000.0);
    std::printf ("Gabinete latency (48 kHz): %d (adapter reports %d)\n",
                 latency, probe.latencySamples());
    if (latency != probe.latencySamples())
        ok = false;
    return parity::checkSilence ("Gabinete", probe) && ok;
}
