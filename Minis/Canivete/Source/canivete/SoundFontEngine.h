#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

// typedef opaque do TinySoundFont; mantido opaco aqui (só o .cpp inclui tsf.h).
struct tsf;

namespace forja::canivete
{

// Fronteira reutilizavel do CANIVETE SUICO: envolve o TinySoundFont (MIT) em um
// player SoundFont multi-timbral. Carrega um bank .sf2/.sf3 em thread de
// background, responde a MIDI (16 canais, program change + CC + pitch bend) e
// entrega um par estereo. Vive separado do AudioProcessor para que o plugin
// standalone e a gaveta do ForjaRack acionem exatamente o mesmo caminho de audio.
//
// Multi-timbral: cada canal MIDI (0..15) guarda um preset proprio; o numero 9 e
// tratado como bateria (banco 128 do GM). O program change escreve o preset do
// canal sem alocar (o tsf v0.9 guarda um unico presetIndex por canal). Os 16
// canais e o pool de vozes sao pre-criados na CARGA (thread de mensagens), para
// o processBlock nunca tocar malloc.
class SoundFontEngine
{
public:
    // Concluir carga: string de erro vazio significa sucesso.
    using Completion = std::function<void (juce::String)>;

    SoundFontEngine();
    ~SoundFontEngine();

    // Chamado com o processamento suspenso (prepareToPlay do host). Configura a
    // taxa do handle ativo e dimensiona o scratch de render.
    void prepare (double sampleRate, int maximumBlockSize);

    // Thread de audio. Adota o handle publicado pela carga, injeta MIDI nos
    // canais, renderiza no pool e soma o par estereo em |stereo| ja com o trim
    // do master e a rampa de bypass aplicados.
    void process (juce::MidiBuffer& midi,
                  juce::AudioBuffer<float>& stereo,
                  int numSamples,
                  bool bypassed) noexcept;

    struct Meter
    {
        float peakLeft = 0.0f;
        float peakRight = 0.0f;
        int activeVoices = 0;
        bool clip = false;
    };

    // Espelho do ultimo bloco, lido pela UI e pela gaveta do rack. Os campos sao
    // atomicos separados de proposito: um std::atomic<Meter> de 16 bytes vira
    // chamada a libatomic (__atomic_store_16/__atomic_load_16) no GCC de x86_64,
    // e uma estatica (o SharedCode do plugin) nao pode adicionar -latomic no link
    // -- o plugin entao falha em tempo de link de Linux. Atomicos de 4/1 byte sao
    // sempre lock-free e nao precisam de biblioteca nenhuma.
    Meter readMeter() const noexcept
    {
        Meter m {};
        m.peakLeft = meterPeakL.load (std::memory_order_relaxed);
        m.peakRight = meterPeakR.load (std::memory_order_relaxed);
        m.activeVoices = meterVoices.load (std::memory_order_relaxed);
        m.clip = meterClip.load (std::memory_order_relaxed);
        return m;
    }

    void setVolume (float volume01) noexcept { volume.store (volume01, std::memory_order_relaxed); }
    float getVolume() const noexcept { return volume.load (std::memory_order_relaxed); }

    // ---- carga de bank (thread de mensagens) ----
    // Aceita .sf2 ou .sf3 (SF3 so quando o build traz stb_vorbis; ver
    // PROVENANCE.md). Le os bytes em background e monta o tsf fora do audio.
    void loadBankFile (const juce::File& file, Completion onDone = {});
    void cancelLoad();
    bool isLoading() const noexcept { return loading.load (std::memory_order_relaxed); }

    // Colhe o resultado da thread de carga na thread de mensagens. Chamado
    // pelo mesmo timer que faz o reap (standalone) ou pelo idle() da gaveta.
    void pollLoadResult();

    bool hasBank() const { return activeBank != nullptr; }
    int getNumPresets() const;
    juce::String getPresetName (int presetIndex) const;
    // Preset atual do canal melodic 0 (leitura aproximada para a UI).
    int getSelectedPresetIndex() const;
    juce::String getSelectedPresetName() const;

    // Instrumento do canal melodic 0 pedido pela UI/estado, pelo INDICE na
    // lista do bank (0..getNumPresets()-1). Guardado em atomic e aplicado pelo
    // processBlock no proximo bloco via tsf_channel_set_presetindex: nada de
    // tocar no handle do tsf fora da thread de audio, e nenhuma alocaao.
    void requestPreset (int presetIndex) noexcept
    {
        desiredPreset.store (presetIndex, std::memory_order_relaxed);
    }

    juce::String getBankName() const;
    juce::String getStatus() const { return status; }

    // SF3 (amostras Ogg) so existe quando o build traz o stb_vorbis na pasta de
    // vendor (ver PROVENANCE.md). False = o player le apenas .sf2.
    static bool sf3Available() noexcept;

    // Libera banks substituidos somente depois que o audio adotou o novo token.
    // Chamado pelo timer do processor (standalone) ou pelo idle() da gaveta.
    void reapRetiredBanks();

    // ---- unicas janelas de teste para o estado interno (harness da suíte) ----
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

private:
    struct Bank
    {
        tsf* handle = nullptr;
        juce::String name;
        int presetCount = 0;
        std::int8_t channelProgram[16] {};   // espelho do ultimo program change por canal
        std::int8_t channelBankMsb[16] {};   // bank select MSB (CC0) e LSB (CC32)
        std::int8_t channelBankLsb[16] {};
        juce::StringArray presetNames;       // congelado na carga (leitura segura na UI)

        Bank() = default;                    // make_unique no loader; posse por smart pointer
        ~Bank();                             // tsf_close definido no .cpp (tsf opaco aqui)

        JUCE_DECLARE_NON_COPYABLE (Bank)     // nao copiasivel; posse sempre por smart pointer
    };

    static void configureBank (tsf* handle, double sampleRate);
    void adopt (std::unique_ptr<Bank> bank);
    void applyChannelDefaults (tsf* handle) noexcept;
    void handleMidiEvent (Bank& bank, const juce::MidiMessage& message, int channel) noexcept;

    // Carga em thread propria. O resultado chega por fila colhida em
    // pollLoadResult (thread de mensagens): nada do loader toca `this` depois
    // da destruicao, nem callback pendurado no MessageManager.
    std::thread loader;
    std::mutex resultMutex;

    struct LoadResult
    {
        std::unique_ptr<Bank> bank;
        juce::String error;
    };

    LoadResult resultQueued;      // error vazio + bank nulo = sem resultado
    bool resultReady = false;
    Completion pendingCompletion;

    // Handle em uso pela thread de audio e o proximo a adotar (publicado por
    // token). pendingBankRaw e trocado no primeiro process() apos a carga; a
    // posse fica em activeBank/retiredBanks (thread de mensagens), entao o
    // ponteiro cru publicado nunca e dono nem vira dangling.
    std::shared_ptr<Bank> activeBank;        // dona: thread de mensagens
    std::atomic<Bank*> pendingBankRaw { nullptr };
    Bank* currentBank = nullptr;   // vista pelo audio (so lida no process)

    struct RetiredBank
    {
        std::shared_ptr<Bank> bank;
        int token = 0;
    };

    std::vector<RetiredBank> retiredBanks;
    std::atomic<int> bankToken { 0 };
    std::atomic<int> acceptedToken { 0 };

    std::vector<float> renderScratch;   // par estereo desenweavado, pre-alocado
    std::atomic<float> meterPeakL { 0.0f };
    std::atomic<float> meterPeakR { 0.0f };
    std::atomic<int> meterVoices { 0 };
    std::atomic<bool> meterClip { false };

    double sampleRate = 48000.0;
    std::atomic<float> volume { 0.5f };   // 0,5 = 0 dB na curva do rack
    std::atomic<int> selectedPreset { -1 };  // preset do canal 0, espelhado pelo audio
    std::atomic<int> desiredPreset { 0 };    // indice de preset do canal 0 pedido pela UI
    int appliedPreset = -1;                  // audio-only: ultimo indice ja aplicado no canal 0
    std::atomic<bool> cancelRequested { false };
    std::atomic<bool> loading { false };
    juce::String status { "Nenhum bank carregado" };
    juce::String bankName;

    // Rampa de bypass (ganho 0..1) suavizada no audio; nunca reseta o tsf.
    float bypassEnv = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundFontEngine)
};

// Trim do master na mesma curva do rack: 0,5 = 0 dB, faixa -24..+24 dB.
inline double trimGain (double volume01)
{
    const auto db = (volume01 - 0.5) * 48.0;
    return db >= 24.0 ? 4.0 : juce::jmax (0.0, std::pow (10.0, db / 20.0));
}

} // namespace forja::canivete
