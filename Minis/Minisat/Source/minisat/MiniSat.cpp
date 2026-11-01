#include "MiniSat.h"

namespace forja::minisat
{

forja::BrasaDSP::Parameters MiniSat::parametersForDrive (float drive, bool bypassed) noexcept
{
    const auto clamped = forja::clamp01 (drive);

    forja::BrasaDSP::Parameters parameters;
    parameters.input = kInputAtZeroDrive + (kInputAtFullDrive - kInputAtZeroDrive) * clamped;
    parameters.tube = kTubeFixed;
    parameters.outputDb = 0.0f;   // sem makeup gain: a camada nao compensa a valve
    parameters.bypass = bypassed;
    return parameters;
}

void MiniSat::prepare (double sampleRate, int maximumBlockSize)
{
    dsp.prepare (sampleRate);

    // O scratch mono e pre-dimensionado aqui: o caminho de audio nunca cresce
    // buffer. Blocos maiores que a capacidade passam direto (sem valvula) so
    // no caso mono; em estereo nao ha scratch e o processamento e integral.
    const auto frames = juce::jmin (kCapacity, juce::jmax (1, maximumBlockSize));
    monoScratch.setSize (1, frames, false, false, true);
}

void MiniSat::process (float* left, float* right, int numSamples, bool bypassed) noexcept
{
    dsp.setParameters (parametersForDrive (drive.load (std::memory_order_relaxed), bypassed));

    const bool mono = (right == nullptr) || (right == left);

    if (! mono)
    {
        dsp.process (left, right, numSamples);
        return;
    }

    if (numSamples > monoScratch.getNumSamples())
        return;   // sem alocacao no audio: bloco extraordinario passa seco

    monoScratch.copyFrom (0, 0, left, 0, numSamples);
    dsp.process (left, monoScratch.getWritePointer (0), numSamples);
}

} // namespace forja::minisat
