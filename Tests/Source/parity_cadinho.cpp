#include "parity_common.h"

#include "X2Buss.h"
#include "forja/dsp/ForjaCadinho.h"

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

bool runCadinhoParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (8642u);

    const std::vector<float> defaults {
        0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f
    };

    const auto setParameters = [] (X2Buss& orig,
                                   forja::dsp::CadinhoDSP& dsp,
                                   const std::vector<float>& p)
    {
        for (int i = 0; i < 10; ++i)
            orig.setParameter (i, p[(std::size_t) i]);

        forja::dsp::CadinhoDSP::Parameters params;
        params.high = p[0];
        params.highMid = p[1];
        params.lowMid = p[2];
        params.bass = p[3];
        params.highFreq = p[4];
        params.highMidFreq = p[5];
        params.lowMidFreq = p[6];
        params.bassFreq = p[7];
        params.thresh = p[8];
        params.fader = p[9];
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

    for (int paramIndex = 0; paramIndex < 10; ++paramIndex)
    {
        for (const float value : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto p = defaults;
            p[(std::size_t) paramIndex] = value;
            cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, 48000.0, p));
        }
    }

    const std::vector<float> engaged {
        0.7f, 0.4f, 0.6f, 0.3f, 0.6f, 0.4f, 0.7f, 0.35f, 0.3f, 0.7f
    };
    cases.push_back (makeCase (parity::Signal::noise, -10.0, 1000.0, 48000.0, engaged));
    cases.push_back (makeCase (parity::Signal::sine, -10.0, 120.0, 96000.0, engaged));
    cases.push_back (makeCase (parity::Signal::square, -6.0, 300.0, 44100.0, engaged));

    int failures = 0;
    int index = 0;
    for (const auto& c : cases)
    {
        // Fresh pair per case: X2Buss holds long filter/ClipOnly state, so a
        // parameter jump would otherwise leave both sides different tails.
        std::srand (8642u);
        X2Buss original (nullptr);
        forja::dsp::CadinhoDSP adapter;
        adapter.setNoiseState (seeds.first, seeds.second);

        const auto result = parity::runCase (original, adapter, c, setParameters);
        parity::report ("Cadinho", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Cadinho parity: %d/%d cases passed\n", index - failures, index);

    forja::dsp::CadinhoDSP probe;
    bool ok = failures == 0;

    for (const double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const int latency = parity::measureDspLatency (probe, sr);
        const int expected = probe.latencySamples();
        std::printf ("Cadinho latency at %.0f: measured %d, reported %d\n", sr, latency, expected);
        if (latency != expected)
            ok = false;
    }

    return parity::checkSilence ("Cadinho", probe) && ok;
}
