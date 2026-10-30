#include "parity_common.h"

#include "CrickBass.h"
#include "forja/dsp/ForjaCrick.h"

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

bool runCrickParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (5306u);
    CrickBass original (nullptr);
    forja::dsp::CrickDSP adapter;
    adapter.setNoiseState (seeds.first, seeds.second);

    const std::vector<float> defaults { 0.5f, 0.5f };

    const auto setParameters = [] (CrickBass& orig,
                                   forja::dsp::CrickDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 2; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::CrickDSP::Parameters params;
        params.drive = p[0];
        params.tone = p[1];
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
        parity::report ("Crick", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Crick parity: %d/%d cases passed\n", index - failures, index);

    bool ok = failures == 0;
    for (const double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        forja::dsp::CrickDSP probe;
        const int latency = parity::measureDspLatency (probe, sr);
        std::printf ("Crick latency (%.0f Hz): %d (adapter reports %d)\n",
                     sr, latency, probe.latencySamples());
        if (latency != probe.latencySamples())
            ok = false;
    }

    forja::dsp::CrickDSP probe;
    return parity::checkSilence ("Crick", probe) && ok;
}
