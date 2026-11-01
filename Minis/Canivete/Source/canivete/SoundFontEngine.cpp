// TU UNICO do TinySoundFont: #define TSF_IMPLEMENTacao aqui; nenhum outro
// arquivo da suixa ve a implementacao. A versao em Forja/Source/forja/vendor/
// soundfont/tsf.h NUNCA e editada (ver PROVENANCE.md).
//
// SF3: o decodificador Ogg do tsf acende quando FORJA_CANIVETE_SF3 esta
// definido (CMake detecta stb_vorbis na pasta de vendor). O stb_vorbis entra
// neste mesmo TU em modo completo (declaracoes + implementacao) ANTES do tsf,
// e o macro STB_VORBIS_INCLUDE_STB_VORBIS_H e o gancho que o tsf.h le para
// ligar o tsf_decode_ogg.

#include "SoundFontEngine.h"

#include <algorithm>
#include <cmath>

#if defined (FORJA_CANIVETE_SF3)
  #define STB_VORBIS_INCLUDE_STB_VORBIS_H
  #include "stb_vorbis.c"
#endif

#define TSF_IMPLEMENTATION
#include "tsf.h"

namespace forja::canivete
{

namespace
{
constexpr int kDrumChannel = 9;        // convencao GM: banco 128 no canal 9
constexpr int kMaxChannels = 16;
constexpr int kMaxVoices = 128;
constexpr double kBypassRampSeconds = 0.010;   // crossfade de 10 ms do bypass
} // namespace

// tsf_close vive aqui: o header so declara ~Bank com tsf opaco (handle e
// forward-declared la), entao a unica TU que ve a implementacao do tsf fecha o
// handle. Smart pointers chamam isto quando o bank sai de uso.
SoundFontEngine::Bank::~Bank()
{
    if (handle != nullptr)
        tsf_close (handle);
}

SoundFontEngine::SoundFontEngine()
{
}

SoundFontEngine::~SoundFontEngine()
{
    cancelRequested.store (true, std::memory_order_relaxed);

    if (loader.joinable())
        loader.join();

    // Dona dos handles: a thread de mensagens fecha tudo. O que o audio ainda
    // apontava (pending/current) e liberado junto com activeBank/retiredBanks.
    std::scoped_lock lock (resultMutex);
    resultQueued = {};
    resultReady = true;
}

void SoundFontEngine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate;

    // Scratch de render: par estereo desenweavado com folga para 192 kHz.
    renderScratch.assign (static_cast<size_t> (maximumBlockSize) * 2u, 0.0f);

    if (auto* bank = activeBank.get(); bank != nullptr && bank->handle != nullptr)
        tsf_set_output (bank->handle, TSF_STEREO_UNWEAVED,
                        static_cast<int> (newSampleRate), 0.0f);
}

bool SoundFontEngine::sf3Available() noexcept
{
#if defined (FORJA_CANIVETE_SF3)
    return true;
#else
    return false;
#endif
}

void SoundFontEngine::configureBank (tsf* handle, double rate)
{
    tsf_set_output (handle, TSF_STEREO_UNWEAVED, static_cast<int> (rate), 0.0f);
    tsf_set_max_voices (handle, kMaxVoices);
}

void SoundFontEngine::applyChannelDefaults (tsf* handle) noexcept
{
    // Pre-cria os 16 canais AQUI (thread de carga/mensagens): o tsf so realoca
    // o array de canais na primeira escrita de cada canal, entao depois disto
    // o processBlock nunca toca malloc. Preset 0 em todo canal; o 9 ja nasce
    // como bateria do GM.
    for (int channel = 0; channel < kMaxChannels; ++channel)
        tsf_channel_set_presetnumber (handle, channel, 0, channel == kDrumChannel ? 1 : 0);
}

void SoundFontEngine::loadBankFile (const juce::File& file, Completion onDone)
{
    if (loader.joinable())
        loader.join();   // uma carga por vez; a anterior ja publicou ou falhou

    {
        std::scoped_lock lock (resultMutex);
        resultQueued = {};
        resultReady = false;
        pendingCompletion = std::move (onDone);
    }

    cancelRequested.store (false, std::memory_order_relaxed);
    loading.store (true, std::memory_order_relaxed);
    status = "Lendo " + file.getFileNameWithoutExtension() + "...";

    const auto path = file.getFullPathName();
    const auto rate = sampleRate;

    loader = std::thread ([this, path, rate]
    {
        auto queueResult = [this] (std::unique_ptr<Bank> bank, juce::String error)
        {
            std::scoped_lock lock (resultMutex);
            if (cancelRequested.load (std::memory_order_relaxed))
                return;   // o unique_ptr devolvido fecha o handle aqui
            resultQueued = { std::move (bank), std::move (error) };
            resultReady = true;
        };

        juce::File target (path);
        juce::MemoryBlock bytes;

        if (! target.existsAsFile())
        {
            queueResult (nullptr, "arquivo nao encontrado");
            return;
        }

        if (! target.loadFileAsData (bytes))
        {
            queueResult (nullptr, "nao foi possivel ler os bytes");
            return;
        }

        if (cancelRequested.load (std::memory_order_relaxed))
        {
            queueResult (nullptr, {});
            return;
        }

        tsf* handle = tsf_load_memory (bytes.getData(),
                                       static_cast<int> (bytes.getSize()));

        if (handle == nullptr)
        {
            queueResult (nullptr, sf3Available() && target.getFileExtension().equalsIgnoreCase (".sf3")
                                       ? "SF3 invalido ou amostras Ogg corrompidas"
                                       : "SoundFont invalido (SF2/SF3)");
            return;
        }

        auto bank = std::make_unique<Bank>();
        bank->handle = handle;
        bank->name = target.getFileNameWithoutExtension();
        bank->presetCount = tsf_get_presetcount (handle);

        for (int index = 0; index < bank->presetCount; ++index)
            if (const char* name = tsf_get_presetname (handle, index))
                bank->presetNames.add (juce::String (name));
            else
                bank->presetNames.add ("Preset " + juce::String (index + 1));

        configureBank (handle, rate);
        applyChannelDefaults (handle);

        queueResult (std::move (bank), {});
    });
}

void SoundFontEngine::pollLoadResult()
{
    LoadResult result;
    Completion completion;

    {
        std::scoped_lock lock (resultMutex);
        if (! resultReady)
            return;
        result = std::move (resultQueued);
        completion = std::move (pendingCompletion);
        resultQueued = {};
        resultReady = false;
    }

    loading.store (false, std::memory_order_relaxed);

    if (result.bank == nullptr)
    {
        status = result.error.isEmpty() ? juce::String ("Carga cancelada")
                                        : "Falha ao carregar o bank: " + result.error;
        if (completion != nullptr)
            completion (status);
        return;
    }

    bankName = result.bank->name;
    selectedPreset.store (-1, std::memory_order_relaxed);
    adopt (std::move (result.bank));

    status = "Bank " + bankName + " pronto";
    if (completion != nullptr)
        completion ({});
}

void SoundFontEngine::cancelLoad()
{
    cancelRequested.store (true, std::memory_order_relaxed);
}

void SoundFontEngine::adopt (std::unique_ptr<Bank> bank)
{
    Bank* raw = bank.get();
    const int newToken = bankToken.load (std::memory_order_relaxed) + 1;

    // O bank substituido so e fechado depois que o audio publicar o token novo
    // (mesmo contrato de reap do MiniDrum). activeBank guarda a posse shared:
    // o pending publicado permanece valido mesmo se uma segunda carga chegar
    // antes do audio colher o primeiro.
    if (activeBank != nullptr)
        retiredBanks.push_back ({ std::move (activeBank), newToken });

    pendingBankRaw = raw;
    activeBank = std::shared_ptr<Bank> (std::move (bank));
    bankToken.store (newToken, std::memory_order_release);
}

void SoundFontEngine::reapRetiredBanks()
{
    const int accepted = acceptedToken.load (std::memory_order_acquire);

    while (! retiredBanks.empty() && retiredBanks.front().token <= accepted)
        retiredBanks.erase (retiredBanks.begin());
}

int SoundFontEngine::getNumPresets() const
{
    if (auto* bank = activeBank.get())
        return bank->presetCount;
    return 0;
}

juce::String SoundFontEngine::getPresetName (int presetIndex) const
{
    auto* bank = activeBank.get();
    if (bank == nullptr || presetIndex < 0 || presetIndex >= bank->presetNames.size())
        return {};
    return bank->presetNames[presetIndex];
}

int SoundFontEngine::getSelectedPresetIndex() const
{
    return selectedPreset.load (std::memory_order_relaxed);
}

juce::String SoundFontEngine::getSelectedPresetName() const
{
    return getPresetName (getSelectedPresetIndex());
}

juce::String SoundFontEngine::getBankName() const
{
    return bankName.isNotEmpty() ? bankName : juce::String ("Nenhum bank");
}

void SoundFontEngine::handleMidiEvent (Bank& bank, const juce::MidiMessage& message, int channel) noexcept
{
    tsf* handle = bank.handle;

    if (message.isNoteOff() || (message.isNoteOn() && message.getVelocity() == 0))
    {
        tsf_channel_note_off (handle, channel, message.getNoteNumber());
        return;
    }

    if (message.isNoteOn())
    {
        tsf_channel_note_on (handle, channel, message.getNoteNumber(),
                             static_cast<float> (message.getVelocity()) / 127.0f);
        return;
    }

    if (message.isProgramChange())
    {
        const auto program = static_cast<std::int8_t> (message.getProgramChangeNumber());
        bank.channelProgram[channel] = program;

        tsf_channel_set_presetnumber (handle, channel, program,
                                      channel == kDrumChannel ? 1 : 0);

        // Espelho do canal 0 para a UI (leitura atomica, sem tocar no handle).
        if (channel == 0)
            selectedPreset.store (tsf_channel_get_preset_index (handle, 0),
                                  std::memory_order_relaxed);
        return;
    }

    if (message.isController())
    {
        const auto controller = message.getControllerNumber();
        const auto value = message.getControllerValue();

        if (controller == 0 || controller == 32)
        {
            // Bank select MSB/LSB (GM Level 2): re-resolve o preset do canal
            // com o ultimo programa espelhado. No canal 9 vale a regra de
            // bateria: MSB vira banco de variacao (128 | MSB), LSB em cima.
            if (controller == 0)
                bank.channelBankMsb[channel] = static_cast<std::int8_t> (value);
            else
                bank.channelBankLsb[channel] = static_cast<std::int8_t> (value);

            const auto program = static_cast<int> (bank.channelProgram[channel]) & 0x7F;

            if (channel == kDrumChannel)
                tsf_channel_set_bank_preset (handle, channel,
                                             128 | static_cast<int> (bank.channelBankMsb[channel]),
                                             program);
            else
                tsf_channel_set_bank_preset (handle, channel,
                                             (static_cast<int> (bank.channelBankMsb[channel]) & 0x7F) * 128
                                                 + (static_cast<int> (bank.channelBankLsb[channel]) & 0x7F),
                                             program);

            if (channel == 0)
                selectedPreset.store (tsf_channel_get_preset_index (handle, 0),
                                      std::memory_order_relaxed);
            return;
        }

        tsf_channel_midi_control (handle, channel, controller, value);
        return;
    }

    if (message.isPitchWheel())
    {
        tsf_channel_set_pitchwheel (handle, channel, message.getPitchWheelValue());
        return;
    }

    // aftertouch, timing clock e Sysex ficam de fora no MVP multi-timbral
}

void SoundFontEngine::process (juce::MidiBuffer& midi,
                               juce::AudioBuffer<float>& stereo,
                               int numSamples,
                               bool bypassed) noexcept
{
    if (auto* pending = pendingBankRaw.exchange (nullptr, std::memory_order_acq_rel))
    {
        currentBank = pending;

        // Token colhido com monotonia: se uma segunda carga publicou um token
        // maior antes deste bloco, nao se pode rebaixar o accepted.
        const int token = bankToken.load (std::memory_order_acquire);
        int accepted = acceptedToken.load (std::memory_order_relaxed);
        while (accepted < token
               && ! acceptedToken.compare_exchange_weak (accepted, token,
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed))
        {
        }

        selectedPreset.store (-1, std::memory_order_relaxed);
    }

    // Sai silencio se nao ha bank (mas nao reseta estado nenhum).
    if (currentBank == nullptr || currentBank->handle == nullptr)
    {
        stereo.clear();
        midi.clear();
        return;
    }

    Bank& bank = *currentBank;

    for (const auto event : midi)
    {
        const auto message = event.getMessage();
        const int channel = message.getChannel() - 1;   // JUCE 1..16 -> tsf 0..15

        if (channel >= 0 && channel < kMaxChannels)
            handleMidiEvent (bank, message, channel);
    }

    midi.clear();

    float* scratch = renderScratch.data();
    tsf_render_float (bank.handle, scratch, numSamples, 0);

    float* outL = stereo.getWritePointer (0);
    float* outR = stereo.getNumChannels() > 1 ? stereo.getWritePointer (1) : outL;
    const float* renL = scratch;
    const float* renR = scratch + numSamples;

    // Rampa do bypass: crossfade linear de 10 ms, AMOSTRA A AMOSTRA. Um degrau
    // por bloco atrasaria a rampa em dezenas de blocos (o teste de bypass pega
    // isso). Nunca reseta o tsf: a voz segue viva, so o ganho e que desliga.
    const double rampPerSample = 1.0 / (kBypassRampSeconds * sampleRate);
    const double step = bypassed ? -rampPerSample : rampPerSample;
    const double volumeGain = trimGain (volume.load (std::memory_order_relaxed));
    double env = static_cast<double> (bypassEnv);

    Meter meter {};

    for (int i = 0; i < numSamples; ++i)
    {
        env = juce::jlimit (0.0, 1.0, env + step);
        const double gain = volumeGain * env;

        const float l = static_cast<float> (renL[i] * gain);
        const float r = static_cast<float> (renR[i] * gain);

        outL[i] += l;
        outR[i] += r;

        meter.peakLeft = juce::jmax (meter.peakLeft, std::abs (l));
        meter.peakRight = juce::jmax (meter.peakRight, std::abs (r));
    }

    bypassEnv = static_cast<float> (env);

    meter.clip = meter.peakLeft > 1.0f || meter.peakRight > 1.0f;
    meter.activeVoices = tsf_active_voice_count (bank.handle);
    lastMeter.store (meter, std::memory_order_relaxed);
}

} // namespace forja::canivete
