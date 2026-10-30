#include "parity_common.h"

#include "Wolfbot.h"
#include "forja/dsp/ForjaLobo.h"

#include <cstdio>

namespace
{

parity::Case makeCase (parity::Signal signal, double levelDb, double frequency,
                       double sampleRate)
{
    parity::Case c;
    c.signal = signal;
    c.levelDb = levelDb;
    c.frequency = frequency;
    c.sampleRate = sampleRate;
    c.seconds = 3.0;
    return c;
}

} // namespace

bool runLoboParity()
{
    const auto seeds = parity::simulateAirwindowsNoiseSeed (5304u);
    Wolfbot original (nullptr);
    forja::dsp::LoboDSP adapter;
    adapter.setNoiseState (seeds.first, seeds.second);

    const auto setParameters = [] (Wolfbot&,
                                   forja::dsp::LoboDSP& dsp,
                                   const std::vector<float>&)
    {
        forja::dsp::LoboDSP::Parameters params;
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
        cases.push_back (makeCase (s.signal, s.level, s.frequency, 48000.0));

    for (const double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        cases.push_back (makeCase (parity::Signal::sine, -12.0, 1000.0, sr));

    int failures = 0;
    int index = 0;
    for (const auto& c : cases)
    {
        const auto result = parity::runCase (original, adapter, c, setParameters);
        parity::report ("Lobo", index, c, result);
        if (! result.ok)
            ++failures;
        ++index;
    }

    std::printf ("Lobo parity: %d/%d cases passed\n", index - failures, index);

    forja::dsp::LoboDSP probe;
    bool ok = failures == 0;
    const int latency = parity::measureDspLatency (probe, 48000.0);
    std::printf ("Lobo latency (48 kHz): %d (adapter reports %d)\n",
                 latency, probe.latencySamples());
    if (latency != probe.latencySamples())
        ok = false;
    return parity::checkSilence ("Lobo", probe) && ok;
}
