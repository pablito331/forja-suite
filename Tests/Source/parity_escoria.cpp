#include "parity_common.h"

#include "DeBess.h"
#include "forja/dsp/ForjaEscoria.h"

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

} // namespace

bool runEscoriaParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (2468u);
    DeBess original (nullptr);
    forja::dsp::EscoriaDSP adapter;
    adapter.setNoiseState (seeds.first, seeds.second);

    const std::vector<float> defaults { 0.0f, 0.5f, 0.5f, 0.5f, 0.0f };

    const auto setParameters = [] (DeBess& orig,
                                   forja::dsp::EscoriaDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 5; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::EscoriaDSP::Parameters params;
        params.intense = p[0];
        params.sharp = p[1];
        params.depth = p[2];
        params.filter = p[3];
        params.sense = p[4];
        params.bypass = false;
        dsp.setParameters (params);
    };

    std::vector<parity::Case> cases;

    const struct { parity::Signal signal; double level; double frequency; } signals[] = {
        { parity::Signal::sine, -18.0, 1000.0 },
        { parity::Signal::sine, 0.0, 1000.0 },
        { parity::Signal::sine, -12.0, 6500.0 },
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

    for (int paramIndex = 0; paramIndex < 5; ++paramIndex)
    {
        for (const float value : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto p = defaults;
            p[(std::size_t) paramIndex] = value;
            cases.push_back (makeCase (parity::Signal::noise, -12.0, 1000.0, 48000.0, p));
        }
    }

    const std::vector<float> active { 0.8f, 0.7f, 0.6f, 0.4f, 0.0f };
    cases.push_back (makeCase (parity::Signal::noise, -6.0, 1000.0, 48000.0, active));
    cases.push_back (makeCase (parity::Signal::sine, -6.0, 7000.0, 96000.0, active));

    int failures = 0;
    int index = 0;
    for (const auto& c : cases)
    {
        const auto result = parity::runCase (original, adapter, c, setParameters);
        parity::report ("Escoria", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Escoria parity: %d/%d cases passed\n", index - failures, index);

    forja::dsp::EscoriaDSP probe;
    bool ok = failures == 0;
    const int latency = parity::measureDspLatency (probe, 48000.0);
    if (latency != 0)
    {
        std::printf ("FAIL Escoria latency: expected 0, measured %d\n", latency);
        ok = false;
    }
    return parity::checkSilence ("Escoria", probe) && ok;
}
