#include "parity_common.h"

#include "Echojam.h"
#include "forja/dsp/ForjaEco.h"

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

bool runEcoParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (1207u);

    const std::vector<float> defaults { 0.5f, 0.5f, 0.5f, 0.5f };

    const auto setParameters = [] (Echojam& orig,
                                   forja::dsp::EcoDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 4; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::EcoDSP::Parameters params;
        params.select = p[0];
        params.regen = p[1];
        params.speed = p[2];
        params.dryWet = p[3];
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

    // Every one of the 17 discrete spaces, always landing on its exact index.
    for (int space = 0; space < 17; ++space)
    {
        auto p = defaults;
        p[0] = ((float) space + 0.5f) / 17.0f;
        cases.push_back (makeCase (parity::Signal::noise, -16.0, 1000.0, 48000.0, p));
    }

    for (int paramIndex = 1; paramIndex < 4; ++paramIndex)
    {
        for (const float value : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto p = defaults;
            p[(std::size_t) paramIndex] = value;
            cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, 48000.0, p));
        }
    }

    const std::vector<float> engaged { 2.5f / 17.0f, 0.8f, 0.7f, 0.6f };
    cases.push_back (makeCase (parity::Signal::noise, -10.0, 1000.0, 48000.0, engaged));
    cases.push_back (makeCase (parity::Signal::sine, -10.0, 120.0, 96000.0, engaged));
    cases.push_back (makeCase (parity::Signal::square, -6.0, 300.0, 44100.0, engaged));

    int failures = 0;
    int index = 0;
    for (const auto& c : cases)
    {
        // Fresh pair per case: the 32 delay lines hold long tails and the
        // original clears them whenever the selected space changes.
        std::srand (1207u);
        // The 32 inline delay lines are ~1.8 MB, past the default 1 MB stack.
        auto original = std::make_unique<Echojam> (nullptr);
        forja::dsp::EcoDSP adapter;
        adapter.setNoiseState (seeds.first, seeds.second);

        const auto result = parity::runCase (*original, adapter, c, setParameters);
        parity::report ("Eco", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Eco parity: %d/%d cases passed\n", index - failures, index);

    // Dry/Wet at zero leaves only the dry path, which has no fixed delay.
    forja::dsp::EcoDSP probe;
    forja::dsp::EcoDSP::Parameters latencyParams;
    latencyParams.dryWet = 0.0f;
    probe.setParameters (latencyParams);
    probe.setNoiseState (0u, 0u);

    bool ok = failures == 0;

    for (const double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        probe.prepare (sr);
        const int latency = parity::measureLatency ([&probe] (double* l, double* r, int n)
        {
            probe.processDouble (l, r, n);
        });
        const int expected = probe.latencySamples();
        std::printf ("Eco latency (dry) at %.0f: measured %d, reported %d\n", sr, latency, expected);
        if (latency != expected)
            ok = false;
    }

    forja::dsp::EcoDSP silenceProbe;
    return parity::checkSilence ("Eco", silenceProbe) && ok;
}
