#include "parity_common.h"

#include "ChimeyGuitar2.h"
#include "forja/dsp/ForjaChime.h"

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

bool runChimeParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (5303u);
    ChimeyGuitar2 original (nullptr);
    forja::dsp::ChimeDSP adapter;
    adapter.setNoiseState (seeds.first, seeds.second);

    const std::vector<float> defaults {
        0.6f, 0.8f, 0.7f, 0.5f, 0.7f, 0.4f, 0.8f, 0.4f, 0.8f, 0.8f
    };

    const auto setParameters = [] (ChimeyGuitar2& orig,
                                   forja::dsp::ChimeDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 10; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::ChimeDSP::Parameters params;
        params.compres = p[0];
        params.presence = p[1];
        params.high = p[2];
        params.mid = p[3];
        params.low = p[4];
        params.sub = p[5];
        params.hspeaker = p[6];
        params.lspeaker = p[7];
        params.speed = p[8];
        params.output = p[9];
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
        parity::report ("Chime", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Chime parity: %d/%d cases passed\n", index - failures, index);

    forja::dsp::ChimeDSP probe;
    bool ok = failures == 0;
    const int latency = parity::measureDspLatency (probe, 48000.0);
    std::printf ("Chime latency (48 kHz): %d (adapter reports %d)\n",
                 latency, probe.latencySamples());
    if (latency != probe.latencySamples())
        ok = false;
    return parity::checkSilence ("Chime", probe) && ok;
}
