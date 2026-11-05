#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cmath>
#include <functional>

#include "engine/TracoEngine.h"
#include "forja/ui/ForjaSpectrum.h"

class TracoAudioProcessor final : public juce::AudioProcessor
{
public:
    TracoAudioProcessor();
    ~TracoAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Traco"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    static constexpr int numBands = forja::traco::TracoEngine::numBands;
    static constexpr int spectrumBands = forja::ui::ForjaSpectrum::bandCount;

    struct MeterSnapshot
    {
        float inPeakL = 0.0f;
        float inPeakR = 0.0f;
        float outPeakL = 0.0f;
        float outPeakR = 0.0f;
        float essGainReductionDb = 0.0f; // positivo: quanto o de-esser cortou
        bool inClip = false;
        bool outClip = false;
    };

    // Troca os picos acumulados por zero. Chamado apenas pela UI.
    MeterSnapshot readMeters();
    void resetClip();
    bool isBypassed() const;

    void readSpectrumIn (float* out, int count) const noexcept { analyserIn.readBands (out, count); }
    void readSpectrumOut (float* out, int count) const noexcept { analyserOut.readBands (out, count); }

    // Sample rate que o editor usa para desenhar a curva estatica. getSampleRate()
    // so e confiavel depois de prepareToPlay via host; com a DAW parada pode vir 0,
    // entao o grafico usa o ultimo sr conhecido (ou o default do engine).
    double displaySampleRate() const noexcept { return sr > 0.0 ? sr : 48000.0; }

    // Sonda para o teste ponta a ponta: a curva analitica que o engine esta
    // usando agora (mesma funcao que o grafico vai desenhar).
    double probeResponseDbAt (double freqHz) const noexcept { return engine.responseDbAt (freqHz); }
    bool probeEngineHpOn() const noexcept { return engine.params().hpOn; }
    double probeEngineHpFreq() const noexcept { return engine.params().hpFreq; }
    forja::traco::Coeffs probeEngineHpCoeffs() const noexcept { return engine.hpTargetCoeffsForTest(); }

    // Escala o valor cru 0..1 de um parametro no nome dado para grandeza
    // fisica. Dividido do processBlock para o teste conseguir vestir o mesmo
    // mapa APVTS -> engine sem depender do JUCE inteiro.
    static forja::traco::TracoEngine::Params paramsFromValues (
        const std::function<float (const juce::String&)>& load);

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static void publishMax (std::atomic<float>& target, float value) noexcept;

    forja::traco::TracoEngine engine;
    forja::ui::ForjaSpectrum analyserIn;
    forja::ui::ForjaSpectrum analyserOut;

    juce::AudioBuffer<float> dryScratch;
    juce::AudioBuffer<float> monoScratch;
    float bypassGain = 1.0f; // 1 = engine ativo; rampa de 10 ms ate 0 no bypass
    double sr = 48000.0;     // sample rate armazenado (getSampleRate() so e valido via host)

    std::atomic<float> meterInL { 0.0f };
    std::atomic<float> meterInR { 0.0f };
    std::atomic<float> meterOutL { 0.0f };
    std::atomic<float> meterOutR { 0.0f };
    std::atomic<float> meterEssGr { 0.0f };
    std::atomic<bool> meterInClip { false };
    std::atomic<bool> meterOutClip { false };
    std::atomic<bool> clipResetRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TracoAudioProcessor)
};
