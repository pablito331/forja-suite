// TU UNICO do TinySoundFont dentro do PIANOFORGE: #define TSF_IMPLEMENTATION
// aqui; nenhum outro arquivo da suíte ve a implementacao. O vendor em
// Forja/Source/forja/vendor/soundfont/tsf.h NUNCA e editado. Este TU proprio
// mantem o caminho GM do CANIVETE intacto (cada plugin de soundfont da suite
// compila o seu). O stb_vorbis (SF3) entra aqui, antes do tsf, no mesmo padrao
// da SoundFontEngine do CANIVETE.

#include "PianoEngine.h"

#include "Bundle.h"

#include <algorithm>
#include <cmath>
#include <limits>

// Coexistencia no ForjaRack: quando o mesmo binario tambem compila a
// SoundFontEngine do CANIVETE (outro TU com TSF/stb), a copia do stb_vorbis ja
// esta linkada la. O CMake define FORJA_PIANOFORGE_SHARES_STB nesse alvo e o
// PIANOFORGE nao re-inclui o stb (o caminho SF3 da gaveta fica desligado; os
// outros alvos, plugin standalone e teste, continuam com SF3 completo).
#if defined (FORJA_CANIVETE_SF3) && ! defined (FORJA_PIANOFORGE_SHARES_STB)
  #include "stb_vorbis.c"
#endif

// TSF_STATIC da ligacao interna as funcoes da API: cada TU de soundfont da
// suite mantem a sua copia sem colidir ao link (o ForjaRack hospeda o tsf do
// CANIVETE e o do PIANOFORGE no mesmo binario). Toda chamada a tsf vive neste
// arquivo, entao a visibilidade local e segura.
#define TSF_STATIC
#define TSF_IMPLEMENTATION
#include "tsf.h"

namespace forja::pianoforge
{

namespace
{
// Fronteira de fatia custa ~6 us no tsf com o piano cheio de vozes (medido no
// projeto-irmao: tools/pforge_dsp_stages). Nota e ataque; CC e bend sao
// ganho/estado ja quantizados por bloco pelo proprio ParameterState, entao uma
// fronteira so e aberta para nota a cada kMinSlice e para controle a cada
// kCcSlice. Os dois valores e o teto de vozes (256) sao os medidos no repo
// antigo, nao chutes.
constexpr std::size_t kMinSlice = 16;
constexpr std::size_t kCcSlice = 64;
constexpr double kBypassRampSeconds = 0.010;   // crossfade de 10 ms do bypass

bool looksLikeBundle (const juce::File& file)
{
    return file.getFileExtension().equalsIgnoreCase (".pforge");
}
} // namespace

// tsf_close vive aqui: o header so declara ~Bank com tsf opaco, entao a unica
// TU que ve a implementacao fecha o handle. Smart pointers chamam isto quando
// o bank sai de uso.
PianoEngine::Bank::~Bank()
{
    if (handle != nullptr)
        tsf_close (handle);
}

PianoEngine::PianoEngine() = default;

PianoEngine::~PianoEngine()
{
    cancelRequested.store (true, std::memory_order_relaxed);

    if (loader.joinable())
        loader.join();

    std::scoped_lock lock (resultMutex);
    resultQueued = {};
    resultReady = true;
}

void PianoEngine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate;

    // Scratch de render: par entrelacado com folga para o maior bloco pedido.
    renderScratch.assign (static_cast<std::size_t> (maximumBlockSize) * 2u, 0.0f);

    const auto rate = static_cast<float> (newSampleRate);
    params.prepare (rate);
    hammerModel.prepare (rate);
    sympResonance.prepare (rate);
    soundboardResponse.prepare (rate);
    soundboardModes.prepare (rate);
    soundboardEQ.prepare (rate);
    lidFilter.prepare (rate);
    compressor.prepare (rate);
    chorus.prepare (rate);
    reverb.prepare (rate);
    molho.prepare (rate);

    if (auto* bank = activeBank.get(); bank != nullptr && bank->handle != nullptr)
        tsf_set_output (bank->handle, TSF_STEREO_INTERLEAVED, static_cast<int> (newSampleRate), 0.0f);
}

bool PianoEngine::sf3Available() noexcept
{
#if defined (FORJA_CANIVETE_SF3) && ! defined (FORJA_PIANOFORGE_SHARES_STB)
    return true;
#else
    return false;
#endif
}

void PianoEngine::configureBank (tsf* handle, double rate)
{
    tsf_set_output (handle, TSF_STEREO_INTERLEAVED, static_cast<int> (rate), 0.0f);
    tsf_set_max_voices (handle, kMaxVoices);

    // Pre-aloca o canal 0 e escolhe o preset 0 longe do audio. Um vel 0 viraria
    // note-off e nao alocaria voz; o aquecimento a vel baixo forca o
    // tsf_channel_init e o pool de vozes agora, para o primeiro acorde depois do
    // swap nao tocar malloc na thread de audio (o mesmo truque do setupSf2 do
    // projeto-irmao).
    tsf_channel_set_presetindex (handle, 0, 0);
    tsf_channel_note_on (handle, 0, 60, 0.001f);
    tsf_channel_note_off (handle, 0, 60);
    tsf_channel_sounds_off_all (handle, 0);
}

void PianoEngine::loadBankFile (const juce::File& file, Completion onDone)
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
        auto queueResult = [this] (std::unique_ptr<Bank> bank, juce::String error, juce::String product)
        {
            std::scoped_lock lock (resultMutex);
            if (cancelRequested.load (std::memory_order_relaxed))
                return;   // o unique_ptr devolvido fecha o handle aqui
            resultQueued = { std::move (bank), std::move (error), std::move (product) };
            resultReady = true;
        };

        const juce::File target (path);

        if (! target.existsAsFile())
        {
            queueResult (nullptr, "arquivo nao encontrado", {});
            return;
        }

        juce::MemoryBlock bytes;
        juce::String product;

        if (looksLikeBundle (target))
        {
            // .pforge do projeto-irmao: le o container no worker, confere o CRC e
            // extrai os bytes do SF2 dentro. Bundle corrompido vira erro legivel,
            // nunca o banco no ar derrubado.
            Bundle bundle;
            juce::String berr;

            if (! bundle.open (target, berr))
            {
                queueResult (nullptr, berr, {});
                return;
            }

            if (! bundle.extractBankBytes (bytes, berr))
            {
                queueResult (nullptr, berr, {});
                return;
            }

            product = bundle.productName();

            if (product.isEmpty())
                product = target.getFileNameWithoutExtension();
        }
        else
        {
            if (! target.loadFileAsData (bytes))
            {
                queueResult (nullptr, "nao foi possivel ler os bytes", {});
                return;
            }
        }

        if (cancelRequested.load (std::memory_order_relaxed))
        {
            queueResult (nullptr, {}, {});
            return;
        }

        if (bytes.getSize() > static_cast<std::size_t> (std::numeric_limits<int>::max()))
        {
            queueResult (nullptr, "banco grande demais para o tsf", {});
            return;
        }

        tsf* handle = tsf_load_memory (bytes.getData(), static_cast<int> (bytes.getSize()));

        if (handle == nullptr)
        {
            const bool sf3 = target.getFileExtension().equalsIgnoreCase (".sf3");
            queueResult (nullptr,
                         sf3 && sf3Available() ? "SF3 invalido ou amostras Ogg corrompidas"
                                               : "SoundFont invalido (SF2/SF3)",
                         {});
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

        queueResult (std::move (bank), {}, product);
    });
}

void PianoEngine::pollLoadResult()
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
                                        : "Falha ao carregar o piano: " + result.error;
        if (completion != nullptr)
            completion (status);
        return;
    }

    bankName = result.bank->name;
    productName = result.product;
    selectedPreset.store (-1, std::memory_order_relaxed);
    adopt (std::move (result.bank));

    status = "Piano " + bankName + " pronto";
    if (completion != nullptr)
        completion ({});
}

void PianoEngine::cancelLoad()
{
    cancelRequested.store (true, std::memory_order_relaxed);
}

void PianoEngine::adopt (std::unique_ptr<Bank> bank)
{
    Bank* raw = bank.get();
    const int newToken = bankToken.load (std::memory_order_relaxed) + 1;

    // O bank substituido so e fechado depois que o audio publicar o token novo
    // (mesmo contrato do CANIVETE/MiniDrum): activeBank guarda a posse shared,
    // entao o pending publicado permanece valido mesmo se uma segunda carga
    // chegar antes do audio colher a primeira.
    if (activeBank != nullptr)
        retiredBanks.push_back ({ std::move (activeBank), newToken });

    pendingBankRaw = raw;
    activeBank = std::shared_ptr<Bank> (std::move (bank));
    bankToken.store (newToken, std::memory_order_release);
}

void PianoEngine::reapRetiredBanks()
{
    const int accepted = acceptedToken.load (std::memory_order_acquire);

    while (! retiredBanks.empty() && retiredBanks.front().token <= accepted)
        retiredBanks.erase (retiredBanks.begin());
}

int PianoEngine::getNumPresets() const
{
    if (auto* bank = activeBank.get())
        return bank->presetCount;
    return 0;
}

juce::String PianoEngine::getPresetName (int presetIndex) const
{
    auto* bank = activeBank.get();
    if (bank == nullptr || presetIndex < 0 || presetIndex >= bank->presetNames.size())
        return {};
    return bank->presetNames[presetIndex];
}

int PianoEngine::getSelectedPresetIndex() const
{
    return selectedPreset.load (std::memory_order_relaxed);
}

juce::String PianoEngine::getSelectedPresetName() const
{
    return getPresetName (getSelectedPresetIndex());
}

juce::String PianoEngine::getBankName() const
{
    if (productName.isNotEmpty())
        return productName;
    return bankName.isNotEmpty() ? bankName : juce::String ("Nenhum piano");
}

// ------------------------------------------------------------------ fila ---

void PianoEngine::stage (const PendingEvent& ev) noexcept
{
    if (pendingCount >= kMaxPendingEvents)
    {
        dispatch (ev);   // fila cheia: age ja em vez de perder o evento
        return;
    }
    pending[static_cast<std::size_t> (pendingCount++)] = ev;
}

void PianoEngine::dispatch (const PendingEvent& ev) noexcept
{
    switch (ev.kind)
    {
        case PendingEvent::NoteOn:  applyNoteOn (ev.a, ev.b); break;
        case PendingEvent::NoteOff: applyNoteOff (ev.a); break;
        case PendingEvent::Control: applyCC (ev.a, ev.b); break;
        case PendingEvent::Bend:    applyBend (ev.b); break;
    }
}

void PianoEngine::applyNoteOn (int note, int rawVelocity) noexcept
{
    Bank* b = currentBank;
    if (b == nullptr || b->handle == nullptr)
        return;   // sem piano: MIDI algum produz som, nem o martelo

    const float curve = params.getValue (Param_VelCurve);
    const int curvedVel = VelocityCurve::process (rawVelocity, curve);

    // O martelo somava demais: 100% do knob passa a valer o que 10% valia. A
    // escala mora no encontro do controle com o modelo, nao dentro do modelo.
    const float hammerAmt = params.getValue (Param_Hammer) * kHammerEscala;
    const float hammerTone = params.getValue (Param_HammerTone);

    if (hammerAmt > 0.001f)
        hammerModel.trigger (note, curvedVel, hammerAmt, hammerTone);

    sympResonance.noteOn (note, curvedVel);
    tsf_channel_note_on (b->handle, 0, note, static_cast<float> (curvedVel) / 127.0f);
}

void PianoEngine::applyNoteOff (int note) noexcept
{
    Bank* b = currentBank;
    if (b == nullptr)
        return;

    sympResonance.noteOff (note);

    if (b->handle != nullptr)
        tsf_channel_note_off (b->handle, 0, note);
}

void PianoEngine::applyCC (int ccNumber, int ccValue) noexcept
{
    Bank* b = currentBank;
    if (b == nullptr)
        return;

    if (ccNumber == 64)
        sympResonance.setSustainPedal (ccValue >= 64);   // CC 64 = pedal de sustain

    if (b->handle != nullptr)
        tsf_channel_midi_control (b->handle, 0, ccNumber, ccValue);
}

void PianoEngine::applyBend (int pitchValue) noexcept
{
    Bank* b = currentBank;
    if (b == nullptr || b->handle == nullptr)
        return;

    tsf_channel_set_pitchwheel (b->handle, 0, juce::jlimit (0, 16383, pitchValue));
}

void PianoEngine::allNotesOff() noexcept
{
    if (auto* b = currentBank; b != nullptr && b->handle != nullptr)
        tsf_channel_sounds_off_all (b->handle, 0);

    for (auto& v : pending)
        v = {};
    pendingCount = 0;

    sympResonance.setSustainPedal (false);
}

// ----------------------------------------------------------------- render ---

void PianoEngine::render (float* outL, float* outR, int numFrames) noexcept
{
    const auto frames = static_cast<std::size_t> (numFrames);

    if (numFrames <= 0)
    {
        pendingCount = 0;
        return;
    }

    // O smoothing e por bloco, nao por fatiamento: avanca as amostras do bloco
    // inteiro de uma vez. Fatiar mais nao acelera a rampa dos parametros.
    params.updateBlock (frames);

    // Ordena por tempo preservando a ordem de chegada nos empates (insertion
    // sort estavel; quase linear quando o host ja entrega em ordem, o normal).
    for (int i = 1; i < pendingCount; ++i)
    {
        const PendingEvent ev = pending[static_cast<std::size_t> (i)];
        int j = i - 1;
        while (j >= 0 && pending[static_cast<std::size_t> (j)].delay > ev.delay)
        {
            pending[static_cast<std::size_t> (j + 1)] = pending[static_cast<std::size_t> (j)];
            --j;
        }
        pending[static_cast<std::size_t> (j + 1)] = ev;
    }

    const auto isNote = [] (const PendingEvent& e) noexcept
    {
        return e.kind == PendingEvent::NoteOn || e.kind == PendingEvent::NoteOff;
    };

    std::size_t pos = 0;
    int next = 0;

    while (pos < frames)
    {
        while (next < pendingCount && pending[static_cast<std::size_t> (next)].delay <= static_cast<int> (pos))
        {
            dispatch (pending[static_cast<std::size_t> (next)]);
            ++next;
        }

        std::size_t stop = frames;
        std::size_t minSlice = kMinSlice;

        if (next < pendingCount)
        {
            stop = static_cast<std::size_t> (juce::jlimit (0, numFrames,
                                                           pending[static_cast<std::size_t> (next)].delay));
            minSlice = isNote (pending[static_cast<std::size_t> (next)]) ? kMinSlice : kCcSlice;
        }

        // Junta fronteiras curtas: um CC pouco antes de uma nota nao deve parar
        // na nota (isso criava mais fronteiras do que o piso resolve). A fusao
        // aproxima a fronteira da proxima nota, nunca passa dela, e nunca desce
        // abaixo do piso de nota.
        if (stop < frames && stop - pos < minSlice)
        {
            std::size_t target = pos + minSlice;

            for (int i = next; i < pendingCount; ++i)
                if (isNote (pending[static_cast<std::size_t> (i)]))
                {
                    target = std::min (target,
                                       static_cast<std::size_t> (juce::jlimit (0, numFrames,
                                                                               pending[static_cast<std::size_t> (i)].delay)));
                    break;
                }

            target = std::max (target, pos + kMinSlice);
            stop = std::min (target, frames);

            while (next < pendingCount && pending[static_cast<std::size_t> (next)].delay < static_cast<int> (stop))
            {
                dispatch (pending[static_cast<std::size_t> (next)]);
                ++next;
            }
        }

        renderSlice (outL + pos, outR + pos, static_cast<int> (stop - pos));
        pos = stop;
    }

    // Eventos atrasados alem do bloco (host pode mandar tempo > n_samples):
    // aplicados no fim para nao perder estado (pedal, bend) para o proximo bloco.
    while (next < pendingCount)
    {
        dispatch (pending[static_cast<std::size_t> (next)]);
        ++next;
    }

    pendingCount = 0;
}

void PianoEngine::renderSlice (float* outL, float* outR, int numFrames) noexcept
{
    if (numFrames <= 0)
        return;

    const auto frames = static_cast<std::size_t> (numFrames);

    // 1. Amostra crua do backend (tsf entrega entrelacado; desenlaça para as
    // duas faixas do barramento).
    Bank* b = currentBank;

    if (b != nullptr && b->handle != nullptr)
    {
        tsf_render_float (b->handle, renderScratch.data(), numFrames, 0);

        for (std::size_t i = 0; i < frames; ++i)
        {
            outL[i] = renderScratch[i * 2 + 0];
            outR[i] = renderScratch[i * 2 + 1];
        }
    }
    else
    {
        std::fill_n (outL, frames, 0.0f);
        std::fill_n (outR, frames, 0.0f);
    }

    // 2. Transiente fisico do martelo.
    hammerModel.process (outL, outR, frames);

    // 3. Ressonancia simpatica (lavagem harmonica em notas sustentadas e pedal).
    sympResonance.setParameters (params.getSmoothed (Param_SympRes), params.getValue (Param_SympTune));
    sympResonance.process (outL, outR, frames);

    // 4. Resposta do tampo: a curva de radiacao do corpo de um piano de cauda
    //    (peito no baixo-medio, articulacao no bridge, ar, sub domado). Corpo da
    //    Forja; unity com o knob no zero, entao nao destroi a amostra.
    soundboardResponse.setAmount (params.getSmoothed (Param_Body));
    soundboardResponse.process (outL, outR, frames);

    // 5. Modos do tampo: a madeira continua vibrando depois da nota (o "dentro do
    //    piano"). Aditivo puro, sem realimentacao; knob no zero = bypass.
    soundboardModes.setAmount (params.getSmoothed (Param_Ring));
    soundboardModes.process (outL, outR, frames);

    // 6. EQ de corpo em 3 bandas (trim do usuario sobre a curva do tampo).
    soundboardEQ.setGains (params.getSmoothed (Param_BassEQ),
                           params.getSmoothed (Param_MidEQ),
                           params.getSmoothed (Param_HighEQ));
    soundboardEQ.process (outL, outR, frames);

    // 7. Filtro acustico da tampa (transparente em 1.0).
    lidFilter.setLid (params.getSmoothed (Param_Lid));
    lidFilter.process (outL, outR, frames);

    // 8. Compressor de um knob: compressor ORIGINAL da Forja (engine/dsp/
    //    Compressor.h), NAO o Dyno -- amassa a dinamica ANTES dos espacos como numa
    //    toma de worship, com joelho macio e ratio que sobem com o knob para
    //    ACHATAR os picos. O makeup automatico segura o nivel percebido, entao o
    //    knob nao derruba o volume. Em amount 0 (default) o processo e transparente,
    //    amostra a amostra (bypass honesto, sem clique).
    {
        const float compAmount = params.getSmoothed (Param_Compressor);
        compressor.setAmount (compAmount);
        compressor.process (outL, outR, static_cast<int> (frames));
    }

    // 9. Chorus stereo.
    chorus.setMix (params.getSmoothed (Param_Chorus));
    chorus.process (outL, outR, frames);

    // 10. Reverb de concerto (FDN, tipo de sala).
    reverb.setType (static_cast<int> (params.getValue (Param_ReverbType)));
    reverb.setMix (params.getSmoothed (Param_Reverb));
    reverb.process (outL, outR, frames);

    // 11. Molho (camada Forja): shimmer ESPECTRAL (tecnica do Atlantis,
    //    reimplementada) de INTERVALO FIXO. Halo difuso uma oitava acima (+12) --
    //    e a 12a com a quinta -- por cima do sinal ja espacializado, gerado num
    //    resonador de espectro com fase aleatoria (sem "voz robotizada"). O shift
    //    nao realimenta, entao so +12 e +12a, sempre harmonico. O knob molho e o
    //    nivel/intensidade do halo (max dosado a 30%), a quinta dosa a 12a, e a
    //    cauda dosa o tempo (RT).
    molho.setAmount (params.getSmoothed (Param_Molho));
    molho.setTone (params.getSmoothed (Param_Quinta));
    molho.setTail (params.getSmoothed (Param_Tail));
    molho.process (outL, outR, frames);

    // 12. Estagio master: largura M/S, trim dB, volume e soft-clip.
    masterStage.process (outL, outR, frames,
                         params.getSmoothed (Param_Volume),
                         params.getValue (Param_TrimDb),
                         params.getValue (Param_Width));
}

// ---------------------------------------------------------------- process ---

void PianoEngine::process (juce::MidiBuffer& midi,
                           juce::AudioBuffer<float>& stereo,
                           int numSamples,
                           bool bypassed) noexcept
{
    float* outL = stereo.getNumChannels() > 0 ? stereo.getWritePointer (0) : nullptr;
    float* outR = stereo.getNumChannels() > 1 ? stereo.getWritePointer (1) : outL;

    if (outL == nullptr)
    {
        midi.clear();
        return;
    }

    // Adota o banco publicado pela carga.
    if (auto* adopted = pendingBankRaw.exchange (nullptr, std::memory_order_acq_rel))
    {
        currentBank = adopted;

        const int token = bankToken.load (std::memory_order_acquire);
        int accepted = acceptedToken.load (std::memory_order_relaxed);
        while (accepted < token
               && ! acceptedToken.compare_exchange_weak (accepted, token,
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed))
        {
        }

        selectedPreset.store (-1, std::memory_order_relaxed);
        appliedPreset = -1;
        appliedTune = 999999.0f;
        allNotesOff();
    }

    if (currentBank == nullptr || currentBank->handle == nullptr)
    {
        stereo.clear();
        midi.clear();
        meterPeakL.store (0.0f, std::memory_order_relaxed);
        meterPeakR.store (0.0f, std::memory_order_relaxed);
        meterVoices.store (0, std::memory_order_relaxed);
        meterClip.store (false, std::memory_order_relaxed);
        return;
    }

    Bank& bank = *currentBank;

    // Preset unico pedido pela UI (canal 0). tsf_channel_set_presetindex so
    // escreve o indice: nenhuma alocaao. appliedPreset marca ate um indice
    // invalido para nao retentar a cada bloco.
    const int want = desiredPreset.load (std::memory_order_relaxed);

    if (want >= 0 && want != appliedPreset && bank.presetCount > 0)
    {
        const int idx = juce::jmin (want, bank.presetCount - 1);
        appliedPreset = idx;

        if (tsf_channel_set_presetindex (bank.handle, 0, idx) != 0)
            selectedPreset.store (idx, std::memory_order_relaxed);
    }

    // Afinacao global (engineTune) em cents -> semitons no tsf.
    const float tune = params.getValue (Param_EngineTune);

    if (tune != appliedTune)
    {
        appliedTune = tune;
        tsf_channel_set_tuning (bank.handle, 0, tune / 100.0f);
    }

    // Enfileira os eventos do bloco pelo tempo dentro do bloco; o render cuida
    // de aplicar cada um no instante certo (fatiado).
    pendingCount = 0;

    for (const auto event : midi)
    {
        const auto message = event.getMessage();
        const int delay = event.samplePosition;

        if (message.isNoteOn() && message.getVelocity() > 0)
        {
            PendingEvent ev;
            ev.delay = delay;
            ev.kind = PendingEvent::NoteOn;
            ev.a = message.getNoteNumber();
            ev.b = message.getVelocity();
            stage (ev);
        }
        else if (message.isNoteOff() || message.isNoteOn())
        {
            PendingEvent ev;
            ev.delay = delay;
            ev.kind = PendingEvent::NoteOff;
            ev.a = message.getNoteNumber();
            stage (ev);
        }
        else if (message.isController())
        {
            PendingEvent ev;
            ev.delay = delay;
            ev.kind = PendingEvent::Control;
            ev.a = message.getControllerNumber();
            ev.b = message.getControllerValue();
            stage (ev);
        }
        else if (message.isPitchWheel())
        {
            PendingEvent ev;
            ev.delay = delay;
            ev.kind = PendingEvent::Bend;
            ev.b = message.getPitchWheelValue();   // 0..16383
            stage (ev);
        }
    }

    midi.clear();

    render (outL, outR, numSamples);

    // Rampa do bypass: crossfade linear de 10 ms, amostra a amostra. Um degrau
    // por bloco atrasaria a rampa em dezenas de blocos. Nunca reseta o tsf: a
    // voz segue viva, so o ganho desliga.
    const double rampPerSample = 1.0 / (kBypassRampSeconds * sampleRate);
    const double step = bypassed ? -rampPerSample : rampPerSample;
    double env = static_cast<double> (bypassEnv);

    Meter meter {};

    for (int i = 0; i < numSamples; ++i)
    {
        env = juce::jlimit (0.0, 1.0, env + step);
        const float gain = static_cast<float> (env);

        const float l = outL[i] * gain;
        const float r = outR[i] * gain;
        outL[i] = l;
        outR[i] = r;

        meter.peakLeft = juce::jmax (meter.peakLeft, std::abs (l));
        meter.peakRight = juce::jmax (meter.peakRight, std::abs (r));
    }

    bypassEnv = static_cast<float> (env);

    meter.clip = meter.peakLeft > 1.0f || meter.peakRight > 1.0f;
    meter.activeVoices = tsf_active_voice_count (bank.handle);

    meterPeakL.store (meter.peakLeft, std::memory_order_relaxed);
    meterPeakR.store (meter.peakRight, std::memory_order_relaxed);
    meterVoices.store (meter.activeVoices, std::memory_order_relaxed);
    meterClip.store (meter.clip, std::memory_order_relaxed);
}

} // namespace forja::pianoforge
