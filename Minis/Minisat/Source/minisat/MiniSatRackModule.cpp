#include "MiniSatRackModule.h"

#include <algorithm>

#include "rack/ModuleCatalog.h"

namespace forja::minisat
{

void MiniSatModule::prepare (double sampleRate)
{
    // O scratch mono do wrapper e pre-dimensionado na thread de mensagens;
    // o processo de audio so da clear/addEvent e nunca pede memoria.
    sat.prepare (sampleRate, MiniSat::kCapacity);
    sat.setDrive (0.0f);
    bypassedFlag = false;
    lastPeak = 0.0f;
}

void MiniSatModule::setParam (int index, float value)
{
    if (index == 0)
        sat.setDrive (juce::jlimit (0.0f, 1.0f, value));
}

void MiniSatModule::process (float* left, float* right, int numSamples)
{
    sat.process (left, right, numSamples, bypassedFlag);

    const auto frame = sat.readMeters (true);
    lastPeak = std::max (frame.outPeakL, frame.outPeakR);
}

void registerMinisatModule()
{
    if (forjarack::ModuleCatalog::indexOf ("minisat") >= 0)
        return;

    forjarack::ModuleSpec spec;
    spec.id = "minisat";
    spec.name = "MINISAT";
    spec.category = forjarack::ModuleCategory::drive;
    spec.accent = 0xffff5a3c;   // laranja quente da saturacao (kAccentDrive)
    spec.role = forjarack::ModuleRole::effect;
    spec.numParams = 1;
    spec.knobs[0] = { "DRIVE", 0.0f };   // 0 = estado default documentado do BRASA
    spec.face = forjarack::ModuleFace::valve;   // a valvula do Tube2 no face do slot

    spec.factory = [] () -> std::unique_ptr<forjarack::ModuleBase>
    {
        return std::make_unique<MiniSatModule>();
    };

    forjarack::ModuleCatalog::registerModule (spec);
}

} // namespace forja::minisat
