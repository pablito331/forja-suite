#include "parity_common.h"

#include "TapeDelay2.h"
#include "forja/dsp/ForjaRepique.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

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

bool runRepiqueParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (9173u);

    const std::vector<float> defaults {
        1.0f, 0.0f, 0.5f, 0.0f, 0.0f, 1.0f
    };

    const auto setParameters = [] (TapeDelay2& orig,
                                   forja::dsp::RepiqueDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 6; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::RepiqueDSP::Parameters params;
        params.time = p[0];
        params.regen = p[1];
        params.freq = p[2];
        params.reso = p[3];
        params.flutter = p[4];
        params.dryWet = p[5];
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

    for (const double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0, 352800.0 })
        cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, sr, defaults));

    for (int paramIndex = 0; paramIndex < 6; ++paramIndex)
    {
        for (const float value : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto p = defaults;
            p[(std::size_t) paramIndex] = value;
            cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, 48000.0, p));
        }
    }

    const std::vector<float> engaged {
        0.8f, 0.7f, 0.4f, 0.6f, 0.5f, 0.5f
    };
    cases.push_back (makeCase (parity::Signal::noise, -10.0, 1000.0, 48000.0, engaged));
    cases.push_back (makeCase (parity::Signal::sine, -10.0, 120.0, 96000.0, engaged));
    cases.push_back (makeCase (parity::Signal::square, -6.0, 300.0, 44100.0, engaged));

    int failures = 0;
    int index = 0;
    for (const auto& c : cases)
    {
        // Fresh pair per case: the ring, filters and flutter sweep hold long
        // state, so a parameter jump would leave different tails. The two
        // 88211-sample rings are ~1.4 MB, past the default 1 MB stack.
        std::srand (9173u);
        auto original = std::make_unique<TapeDelay2> (nullptr);
        forja::dsp::RepiqueDSP adapter;
        adapter.setNoiseState (seeds.first, seeds.second);

        const auto result = parity::runCase (*original, adapter, c, setParameters);
        parity::report ("Repique", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Repique parity: %d/%d cases passed\n", index - failures, index);

    bool ok = failures == 0;

    // The ring is a musical delay, never latency compensation. With Time at
    // zero it still sits 88200 samples back, so an impulse test would measure
    // the music, not PDC: the declared value is asserted instead.
    forja::dsp::RepiqueDSP probe;
    if (probe.latencySamples() != 0)
    {
        std::printf ("FAIL Repique reports %d samples of PDC\n", probe.latencySamples());
        ok = false;
    }

    return parity::checkSilence ("Repique", probe) && ok;
}
