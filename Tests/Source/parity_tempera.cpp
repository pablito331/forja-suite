#include "parity_common.h"

#include "ButterComp2.h"
#include "forja/dsp/ForjaTempera.h"

#include <cstdio>

namespace
{

parity::Case makeCase (parity::Signal signal, double levelDb, double frequency,
                       double sampleRate, const std::array<float, 6>& params)
{
    parity::Case c;
    c.signal = signal;
    c.levelDb = levelDb;
    c.frequency = frequency;
    c.sampleRate = sampleRate;
    c.params.assign (params.begin(), params.end());
    return c;
}

} // namespace

bool runTemperaParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (1234u);
    ButterComp2 original (nullptr);
    forja::dsp::TemperaDSP adapter;
    adapter.setNoiseState (seeds.first, seeds.second);

    const std::array<float, 6> defaults { 0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f };

    const auto setParameters = [] (ButterComp2& orig,
                                   forja::dsp::TemperaDSP& dsp,
                                   const std::vector<float>& p)
    {
        orig.setParameter (0, p[0]);
        orig.setParameter (1, p[1]);
        orig.setParameter (2, p[2]);

        forja::dsp::TemperaDSP::Parameters params;
        params.compress = p[0];
        params.output = p[1];
        params.mix = p[2];
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
    };

    for (const auto& s : signals)
        cases.push_back (makeCase (s.signal, s.level, s.frequency, 48000.0, defaults));

    for (const double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, sr, defaults));

    for (int paramIndex = 0; paramIndex < 3; ++paramIndex)
    {
        for (const float value : { 0.25f, 0.5f, 0.75f, 1.0f })
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
        parity::report ("Tempera", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Tempera parity: %d/%d cases passed\n", index - failures, index);
    return failures == 0;
}
