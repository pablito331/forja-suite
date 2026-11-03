#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "Params.h"
#include "dsp/Biquad.h"
#include "dsp/Compressor.h"
#include "dsp/Hammer.h"
#include "dsp/Resonance.h"
#include "dsp/Filters.h"
#include "dsp/Soundboard.h"
#include "dsp/Shimmer.h"
#include "dsp/Space.h"
#include "dsp/Master.h"

// typedef opaque do TinySoundFont; mantido opaco aqui (so o .cpp inclui tsf.h).
struct tsf;

namespace forja::pianoforge
{

// Suavizador por bloco (porta de Parameters.hpp::SmoothedValue). A regra do
// conserto antigo e reproduzida aqui: avancar o bloco inteiro de uma vez, nao
// uma amostra por chamada -- com o coeficiente de uma amostra os 20 ms viravam
// ~5 s num bloco de 256 e EQ, Reverb, Chorus, Tampa e Volume pareciam mortos.
// O resultado e o mesmo decaimento exponencial avaliado no fim do bloco, que e
// o instante em que o valor volta a ser lido.
class SmoothedValue
{
public:
    void reset (float initialValue, float sampleRate, float smoothTimeSec = 0.02f) noexcept
    {
        current = initialValue;
        target = initialValue;
        invTauPerSample = sampleRate > 1000.0f ? 1.0f / (smoothTimeSec * sampleRate) : 0.01f;
    }

    void setTarget (float newTarget) noexcept { target = newTarget; }
    float get() const noexcept { return current; }

    void advance (std::size_t frames) noexcept
    {
        if (frames == 0)
            return;

        const float f = static_cast<float> (frames > 100000 ? 100000 : frames);
        const float alpha = 1.0f - std::exp (-f * invTauPerSample);
        current += (target - current) * alpha;
    }

private:
    float current = 0.0f;
    float target = 0.0f;
    float invTauPerSample = 0.01f;
};

// Estado dos 22 parametros no dominio do engine. Os valores chegam ja
// escalonados (o processor faz toEngineDomain na fronteira do APVTS). O canal
// entre o que escreve (thread de audio, no processBlock) e o que o DSP le sao
// os atomicos values[]; smoothed[] so e tocado pela thread de audio.
class ParameterState
{
public:
    ParameterState()
    {
        for (int i = 0; i < Param_Count; ++i)
        {
            const auto& def = kParamDefs[i];
            const float initial = def.min + def.default01 * (def.max - def.min);
            values[i].store (initial, std::memory_order_relaxed);
            smoothed[i].reset (initial, 44100.0f);
        }
    }

    void prepare (float sampleRate) noexcept
    {
        for (int i = 0; i < Param_Count; ++i)
            smoothed[i].reset (values[i].load (std::memory_order_relaxed), sampleRate, 0.02f);
    }

    void setValue (int paramId, float engineValue) noexcept
    {
        if (paramId < 0 || paramId >= Param_Count)
            return;

        // Descartar NaN: fmin/fmax devolveriam o operando nao-NaN e um host com
        // automacao quebrada mandaria o volume a fundo. Heranca do conserto antigo.
        if (! std::isfinite (engineValue))
            return;

        const auto& def = kParamDefs[paramId];
        const float clamped = std::fmax (def.min, std::fmin (def.max, engineValue));
        values[paramId].store (clamped, std::memory_order_relaxed);
    }

    float getValue (int paramId) const noexcept
    {
        if (paramId < 0 || paramId >= Param_Count)
            return 0.0f;
        return values[paramId].load (std::memory_order_relaxed);
    }

    float getSmoothed (int paramId) const noexcept
    {
        if (paramId < 0 || paramId >= Param_Count)
            return 0.0f;
        return smoothed[paramId].get();
    }

    // Uma vez por bloco, antes do primeiro fatiamento: recarrega os alvos a
    // partir dos atomicos e avanca as amostras do bloco inteiro.
    void updateBlock (std::size_t frames) noexcept
    {
        for (int i = 0; i < Param_Count; ++i)
        {
            smoothed[i].setTarget (values[i].load (std::memory_order_relaxed));
            smoothed[i].advance (frames);
        }
    }

private:
    std::array<std::atomic<float>, Param_Count> values;
    std::array<SmoothedValue, Param_Count> smoothed;
};

// Camada de amostras + modelo fisico do PIANOFORGE. Envelope o TinySoundFont
// (um banco de piano por vez, preset unico no canal 0) e acrescenta a cadeia
// portada do projeto-irmao: martelo, ressonancia simpatica, EQ de corpo, tampa,
// compressor de um knob, chorus, reverb FDN, molho (camada de oitava viva) e
// estagio master. O bank e lido em thread de background e
// publicado por token (mesmo contrato do CANIVETE); nada toca o handle do tsf
// fora da thread de audio e nada aloca no processBlock.
//
// O processBlock fatia o bloco nos instantes em que algo mudou (fila de eventos
// com delayFrames, fronteiras fundidas em kMinSlice) para a nota soar no tempo
// certo -- comportamento que um player comum nao tem e a razao de ser do piano.
class PianoEngine
{
public:
    using Completion = std::function<void (juce::String)>;

    PianoEngine();
    ~PianoEngine();

    // Chamado com o processamento suspenso (prepareToPlay). Configura a taxa do
    // handle ativo e dimensiona o scratch de render.
    void prepare (double sampleRate, int maximumBlockSize);

    // Thread de audio. Adota o banco publicado, injeta MIDI na fila com o
    // tempo do bloco, renderiza fatiado pela cadeia DSP e escreve o par estereo
    // em |stereo| ( sobrescreve as duas faixas ) ja com o bypass suavizado.
    void process (juce::MidiBuffer& midi,
                  juce::AudioBuffer<float>& stereo,
                  int numSamples,
                  bool bypassed) noexcept;

    // Empurra um parametro ja no dominio do engine (o processor converte do
    // 0..1 do APVTS). Nao-RT seguro: grava no atomico lido pelo audio.
    void setParameter (int paramId, float engineValue) noexcept
    {
        params.setValue (paramId, engineValue);
    }

    float getParameter (int paramId) const noexcept { return params.getValue (paramId); }

    struct Meter
    {
        float peakLeft = 0.0f;
        float peakRight = 0.0f;
        int activeVoices = 0;
        bool clip = false;
    };

    // Espelho do ultimo bloco. Atomicos separados de 4/1 byte de proposito: um
    // std::atomic<Meter> de 16 bytes viraria chamada a libatomic no GCC x86_64 e
    // uma estatica nao pode adicionar -latomic no link (mesma razao do CANIVETE).
    Meter readMeter() const noexcept
    {
        Meter m {};
        m.peakLeft = meterPeakL.load (std::memory_order_relaxed);
        m.peakRight = meterPeakR.load (std::memory_order_relaxed);
        m.activeVoices = meterVoices.load (std::memory_order_relaxed);
        m.clip = meterClip.load (std::memory_order_relaxed);
        return m;
    }

    // ---- carga de banco (thread de mensagens) ----
    // Aceita .sf2/.sf3 soltos e .pforge (bundle do projeto-irmao: SF2 dentro).
    // Le os bytes em background e monta o tsf fora do audio.
    void loadBankFile (const juce::File& file, Completion onDone = {});
    void cancelLoad();
    bool isLoading() const noexcept { return loading.load (std::memory_order_relaxed); }
    void pollLoadResult();

    bool hasBank() const { return activeBank != nullptr; }
    int getNumPresets() const;
    juce::String getPresetName (int presetIndex) const;
    int getSelectedPresetIndex() const;
    juce::String getSelectedPresetName() const;

    // Preset unico pedido pela UI/estado, pelo INDICE na lista do banco.
    void requestPreset (int presetIndex) noexcept
    {
        desiredPreset.store (presetIndex, std::memory_order_relaxed);
    }

    // Nome do produto dentro de um .pforge (vazio para banco solto).
    juce::String getProductName() const { return productName; }

    juce::String getBankName() const;
    juce::String getStatus() const { return status; }

    static bool sf3Available() noexcept;

    void reapRetiredBanks();

    // ---- janelas de teste para o estado interno (harness da suite) ----
    bool hasPendingBankForTest() const noexcept
    {
        return pendingBankRaw.load (std::memory_order_relaxed) != nullptr;
    }

    int retiredBankCountForTest() const noexcept
    {
        return static_cast<int> (retiredBanks.size());
    }

    long activeBankUseCountForTest() const noexcept
    {
        return activeBank.use_count();
    }

    // Leitura dos suavizadores depois de um bloco (teste de rampa/zipper).
    float smoothedForTest (int paramId) const noexcept { return params.getSmoothed (paramId); }

private:
    struct Bank
    {
        tsf* handle = nullptr;
        juce::String name;
        int presetCount = 0;
        juce::StringArray presetNames;

        Bank() = default;
        ~Bank();
        JUCE_DECLARE_NON_COPYABLE (Bank)
    };

    // Evento esperado pelo bloco em curso. Nada aqui aloca: a fila e um array
    // fixo, e o que o RT pode fazer dentro do process.
    struct PendingEvent
    {
        int delay = 0;
        enum Kind { NoteOn, NoteOff, Control, Bend } kind = NoteOn;
        int a = 0;   // nota ou numero de CC
        int b = 0;   // velocidade, valor de CC ou pitch bend
    };

    static void configureBank (tsf* handle, double sampleRate);
    void adopt (std::unique_ptr<Bank> bank);

    void stage (const PendingEvent& ev) noexcept;
    void dispatch (const PendingEvent& ev) noexcept;
    void applyNoteOn (int note, int rawVelocity) noexcept;
    void applyNoteOff (int note) noexcept;
    void applyCC (int ccNumber, int ccValue) noexcept;
    void applyBend (int pitchValue) noexcept;
    void allNotesOff() noexcept;

    void render (float* outL, float* outR, int numFrames) noexcept;
    void renderSlice (float* outL, float* outR, int numFrames) noexcept;

    static constexpr int kMaxChannels = 1;   // piano: so o canal 0
    static constexpr int kMaxVoices = 256;
    static constexpr int kMaxPendingEvents = 512;

    // Carga em thread propria; o resultado chega por fila colhida em
    // pollLoadResult (thread de mensagens). Nada do loader toca `this` depois
    // da destruicao.
    std::thread loader;
    std::mutex resultMutex;

    struct LoadResult
    {
        std::unique_ptr<Bank> bank;
        juce::String error;
        juce::String product;
    };

    LoadResult resultQueued;
    bool resultReady = false;
    Completion pendingCompletion;

    std::shared_ptr<Bank> activeBank;          // dona: thread de mensagens
    std::atomic<Bank*> pendingBankRaw { nullptr };
    Bank* currentBank = nullptr;               // vista pelo audio (so lida no process)

    struct RetiredBank
    {
        std::shared_ptr<Bank> bank;
        int token = 0;
    };

    std::vector<RetiredBank> retiredBanks;
    std::atomic<int> bankToken { 0 };
    std::atomic<int> acceptedToken { 0 };

    ParameterState params;

    // Corpo fisico portado do projeto-irmao (engine/dsp/*.h). Estado por
    // instancia, buffers pre-dimensionados para 192 kHz em std::array fixo:
    // nenhum static, nenhuma alocaao no prepare (o prepare so re-acerta as
    // taxas) nem no processBlock.
    HammerModel hammerModel;
    SympatheticResonance sympResonance;
    SoundboardResponse soundboardResponse;   // curva de radiacao do corpo (Forja)
    SoundboardModes soundboardModes;         // modos/anel da madeira (Forja)
    SoundboardEQ soundboardEQ;               // trim de 3 bandas do usuario
    LidFilter lidFilter;
    WorshipCompressor compressor;            // compressor Forja de um knob (achata + makeup)
    Chorus chorus;
    Reverb reverb;
    Molho molho;                             // shimmer de oitava com cauda de difusao (Forja)
    MasterStage masterStage;

    std::vector<float> renderScratch;   // par entrelacado do tsf, pre-alocado
    std::array<PendingEvent, kMaxPendingEvents> pending;
    int pendingCount = 0;

    std::atomic<float> meterPeakL { 0.0f };
    std::atomic<float> meterPeakR { 0.0f };
    std::atomic<int> meterVoices { 0 };
    std::atomic<bool> meterClip { false };

    double sampleRate = 48000.0;
    std::atomic<int> selectedPreset { -1 };
    std::atomic<int> desiredPreset { 0 };
    int appliedPreset = -1;
    float appliedTune = 999999.0f;   // audio-only: ultimo engineTune ja aplicado
    std::atomic<bool> cancelRequested { false };
    std::atomic<bool> loading { false };
    juce::String status { "Nenhum piano carregado" };
    juce::String bankName;
    juce::String productName;

    // Rampa de bypass (ganho 0..1) suavizada no audio; nunca reseta o tsf.
    float bypassEnv = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoEngine)
};

} // namespace forja::pianoforge
