#include "parity_common.h"

#include "Pop2.h"
#include "forja/dsp/ForjaMalho.h"

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

bool runMalhoParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (9012u);
    Pop2 original (nullptr);
    forja::dsp::MalhoDSP adapter;
    adapter.setNoiseState (seeds.first, seeds.second);

    const std::array<float, 6> defaults { 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f };

    const auto setParameters = [] (Pop2& orig,
                                   forja::dsp::MalhoDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 5; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::MalhoDSP::Parameters params;
        params.compress = p[0];
        params.attack = p[1];
        params.release = p[2];
        params.drive = p[3];
        params.mix = p[4];
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

    for (int paramIndex = 0; paramIndex < 5; ++paramIndex)
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
        parity::report ("Malho", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Malho parity: %d/%d cases passed\n", index - failures, index);
    return failures == 0;
}
