#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "engine/DrumEngine.h"
#include "engine/Kit.h"
#include "engine/SampleStore.h"

namespace forja::minidrum
{

// Fronteira reutilizavel do MiniDrum: um unificador do engine do Drumforge em
// um tocador de kit compacto. Carrega um kit (.dfkit / drumkit.txt / pasta),
// toca o premix do kit com um unico knob de volume e entrega um par estereo.
// Vive separado do AudioProcessor para que o plugin standalone e, depois, a
// gaveta do ForjaRack acionem exatamente o mesmo caminho de audio.
//
// Diferente do Drumforge, nao expoe a mesa de 16 canais: os pads tocam com a
// mix gravada no kit (quando ha), e o knob VOLUME manda no trim do master.
class MiniDrum
{
public:
    // Concluir carga: string de erro vazio significa sucesso.
    using Completion = std::function<void (juce::String)>;

    MiniDrum();
    ~MiniDrum();

    void prepare (double sampleRate, int maximumBlockSize);

    // Thread de audio. |stereo| e a saida principal (2 canais, |numSamples| frames);
    // os barramentos auxiliares do engine sao descartados por nao haver mesa aqui.
    void process (juce::MidiBuffer& midi,
                  juce::AudioBuffer<float>& stereo,
                  int numSamples,
                  bool bypassed) noexcept;

    void setVolume (float volume01) noexcept { volume.store (volume01, std::memory_order_relaxed); }
    float getVolume() const noexcept { return volume.load (std::memory_order_relaxed); }

    forja::drumforge::DrumEngine::MasterMeter readMasterMeter() noexcept
    {
        return engine.readMasterMeter();
    }

    // ---- carga de kit (thread de mensagens) ----
    // Aceita .dfkit, drumkit.txt/.labooh ou pasta de samples; Kit::loadFromFile
    // decide o formato. A foto de preview vem decodificada dentro do LoadedKit.
    void loadKitFile (const juce::File& file, Completion onDone = {});
    void cancelLoad();
    bool isLoading() const;

    std::shared_ptr<const forja::drumforge::LoadedKit> getLoadedKit() const
    {
        return activeKit;
    }

    juce::String getKitName() const;
    juce::String getStatus() const { return status; }
    int getProgress() const;
    double getSampleRate() const noexcept { return sampleRate; }

    // Libera kits antigos somente depois que o audio adotou o token novo. Chamado
    // pelo timer do processor (standalone) ou pelo timer do rack para nao
    // segurar buffers de um kit substituido.
    void reapRetiredKits();

private:
    struct RetiredKit
    {
        std::shared_ptr<const forja::drumforge::LoadedKit> kit;
        int token = 0;
    };

    forja::drumforge::EngineParameters buildParameters() const noexcept;
    void adopt (std::unique_ptr<const forja::drumforge::LoadedKit> loaded);

    forja::drumforge::DrumEngine engine;
    forja::drumforge::SampleStore store;

    std::shared_ptr<const forja::drumforge::LoadedKit> activeKit;
    std::vector<RetiredKit> retiredKits;
    int kitToken = 0;

    // Kit visto pela thread de audio. A mix gravada no kit alimenta os pads do
    // engine; o ponteiro cru e publicado em pendingKitRaw ANTES do requestKit e
    // consumido no primeiro process() apos a troca (mesma janela de adocao do
    // engine), entao o reap so libera um kit substituido quando nem o engine
    // nem buildParameters ainda o alcancam.
    std::atomic<const forja::drumforge::Kit*> pendingKitRaw { nullptr };
    const forja::drumforge::Kit* currentKit = nullptr;

    double sampleRate = 48000.0;
    std::atomic<float> volume { 0.5f };
    juce::String status { "Nenhum kit carregado" };

    // Guarda contra o callback de carga cair depois da destruicao do wrapper.
    std::shared_ptr<std::atomic<bool>> alive;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiniDrum)
};

} // namespace forja::minidrum
