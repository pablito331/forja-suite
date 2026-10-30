#include "parity_common.h"

#include "kRockstar2.h"
#include "forja/dsp/ForjaSalao.h"

#include <cstdio>
#include <cstdlib>

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

bool runSalaoParity()
{
    const unsigned seed = 3311u;
    const auto seeds = parity::simulateAirwindowsNoiseSeed (seed);

    const std::vector<float> defaults {
        0.5f, 0.75f, 1.0f, 0.25f, 0.5f
    };

    const auto setParameters = [] (kRockstar2& orig,
                                   forja::dsp::SalaoDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 5; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::SalaoDSP::Parameters params;
        params.regen = p[0];
        params.position = p[1];
        params.downRez = p[2];
        params.distance = p[3];
        params.dryWet = p[4];
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

    for (int paramIndex = 0; paramIndex < 5; ++paramIndex)
    {
        for (const float value : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto p = defaults;
            p[(std::size_t) paramIndex] = value;
            cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, 48000.0, p));
        }
    }

    const std::vector<float> engaged {
        0.7f, 0.4f, 0.6f, 0.6f, 0.35f
    };
    cases.push_back (makeCase (parity::Signal::noise, -10.0, 1000.0, 48000.0, engaged));
    cases.push_back (makeCase (parity::Signal::sine, -10.0, 120.0, 96000.0, engaged));
    cases.push_back (makeCase (parity::Signal::square, -6.0, 300.0, 44100.0, engaged));

    int failures = 0;
    int index = 0;
    for (const auto& c : cases)
    {
        // Fresh pair per case: the room tone, the noise generators and the
        // 90 delay lines hold long state, so a parameter jump would leave
        // different tails.
        std::srand (seed);
        kRockstar2 original (nullptr);
        forja::dsp::SalaoDSP adapter;
        adapter.setNoiseState (seeds.first, seeds.second);

        const auto result = parity::runCase (original, adapter, c, setParameters);
        parity::report ("Salao", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Salao parity: %d/%d cases passed\n", index - failures, index);

    bool ok = failures == 0;

    // The space is musical; the undersampling reconstruction is not PDC.
    forja::dsp::SalaoDSP probe;
    if (probe.latencySamples() != 0)
    {
        std::printf ("FAIL Salao reports %d samples of PDC\n", probe.latencySamples());
        ok = false;
    }

    return ok;
}
