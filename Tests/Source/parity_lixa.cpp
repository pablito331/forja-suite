#include "parity_common.h"

#include "PurestDrive.h"
#include "forja/dsp/ForjaLixa.h"

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

bool runLixaParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (7101u);
    PurestDrive original (nullptr);
    forja::dsp::LixaDSP adapter;
    adapter.setNoiseState (seeds.first, seeds.second);

    const std::vector<float> defaults { 0.0f };

    const auto setParameters = [] (PurestDrive& orig,
                                   forja::dsp::LixaDSP& dsp,
                                   const std::vector<float>& p)
    {
        orig.setParameter (0, p[0]);

        forja::dsp::LixaDSP::Parameters params;
        params.drive = p[0];
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

    for (const float value : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, 48000.0,
                                   std::vector<float> { value }));

    int failures = 0;
    int index = 0;
    for (const auto& c : cases)
    {
        const auto result = parity::runCase (original, adapter, c, setParameters);
        parity::report ("Lixa", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Lixa parity: %d/%d cases passed\n", index - failures, index);

    forja::dsp::LixaDSP probe;
    bool ok = failures == 0;
    const int latency = parity::measureDspLatency (probe, 48000.0);
    if (latency != 0)
    {
        std::printf ("FAIL Lixa latency: expected 0, measured %d\n", latency);
        ok = false;
    }
    return parity::checkSilence ("Lixa", probe) && ok;
}
