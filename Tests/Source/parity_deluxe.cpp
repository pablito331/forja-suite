#include "parity_common.h"

#include "ChimeyDeluxe.h"
#include "forja/dsp/ForjaDeluxe.h"

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
    c.seconds = 2.0;
    return c;
}

} // namespace

bool runDeluxeParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (5305u);
    ChimeyDeluxe original (nullptr);
    forja::dsp::DeluxeDSP adapter;
    adapter.prepare (48000.0);
    adapter.setNoiseState (seeds.first, seeds.second);

    const std::vector<float> defaults {
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f
    };

    const auto setParameters = [] (ChimeyDeluxe& orig,
                                   forja::dsp::DeluxeDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 10; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::DeluxeDSP::Parameters params;
        params.hell = p[0];
        params.fizz = p[1];
        params.pick = p[2];
        params.satan = p[3];
        params.danger = p[4];
        params.critical = p[5];
        params.hMeat = p[6];
        params.lMeat = p[7];
        params.swing = p[8];
        params.rarely = p[9];
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

    for (int paramIndex = 0; paramIndex < 10; ++paramIndex)
    {
        for (const float value : { 0.0f, 0.5f, 1.0f })
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
        parity::report ("Deluxe", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Deluxe parity: %d/%d cases passed\n", index - failures, index);

    forja::dsp::DeluxeDSP probe;
    bool ok = failures == 0;
    const int latency = parity::measureDspLatency (probe, 48000.0);
    std::printf ("Deluxe latency (48 kHz): %d (adapter reports %d)\n",
                 latency, probe.latencySamples());
    if (latency != probe.latencySamples())
        ok = false;
    return parity::checkSilence ("Deluxe", probe) && ok;
}
