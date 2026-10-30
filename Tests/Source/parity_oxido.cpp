#include "parity_common.h"

#include "IronOxide5.h"
#include "forja/dsp/ForjaOxido.h"

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

bool runOxidoParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (8642u);

    const std::vector<float> defaults {
        0.5f, 0.562341325190349f, 0.562341325190349f, 0.5f, 0.5f, 0.5f, 1.0f
    };

    const auto setParameters = [] (IronOxide5& orig,
                                   forja::dsp::OxidoDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 7; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::OxidoDSP::Parameters params;
        params.input = p[0];
        params.tapeHigh = p[1];
        params.tapeLow = p[2];
        params.flutter = p[3];
        params.noise = p[4];
        params.output = p[5];
        params.invDryWet = p[6];
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

    for (int paramIndex = 0; paramIndex < 7; ++paramIndex)
    {
        for (const float value : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto p = defaults;
            p[(std::size_t) paramIndex] = value;
            cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, 48000.0, p));
        }
    }

    const std::vector<float> engaged {
        0.7f, 0.4f, 0.7f, 0.6f, 0.3f, 0.7f, 0.35f
    };
    cases.push_back (makeCase (parity::Signal::noise, -10.0, 1000.0, 48000.0, engaged));
    cases.push_back (makeCase (parity::Signal::sine, -10.0, 120.0, 96000.0, engaged));
    cases.push_back (makeCase (parity::Signal::square, -6.0, 300.0, 44100.0, engaged));

    int failures = 0;
    int index = 0;
    for (const auto& c : cases)
    {
        // Fresh pair per case: the Iron Oxide rings and the flutter sweep hold
        // long state, so a parameter jump would leave different tails.
        std::srand (8642u);
        IronOxide5 original (nullptr);
        forja::dsp::OxidoDSP adapter;
        adapter.setNoiseState (seeds.first, seeds.second);

        const auto result = parity::runCase (original, adapter, c, setParameters);
        parity::report ("Oxido", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Oxido parity: %d/%d cases passed\n", index - failures, index);

    forja::dsp::OxidoDSP probe;
    // Flutter is a modulated delay by design, so it can never be part of PDC.
    // With it neutral the Iron Oxide chain has no fixed delay.
    forja::dsp::OxidoDSP::Parameters latencyParams;
    latencyParams.flutter = 0.0f;
    probe.setParameters (latencyParams);

    bool ok = failures == 0;

    for (const double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        // Zero the RNG before the impulse so the injected noise floor does not
        // register as an early output sample.
        probe.prepare (sr);
        probe.setNoiseState (0u, 0u);
        const int latency = parity::measureLatency ([&probe] (double* l, double* r, int n)
        {
            probe.processDouble (l, r, n);
        });
        const int expected = probe.latencySamples();
        std::printf ("Oxido latency at %.0f: measured %d, reported %d\n", sr, latency, expected);
        if (latency != expected)
            ok = false;
    }

    return parity::checkSilence ("Oxido", probe) && ok;
}
