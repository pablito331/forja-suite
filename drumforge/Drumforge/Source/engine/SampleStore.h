#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "Kit.h"

namespace forja::drumforge
{

struct SampleData
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    int length = 0;
    int channels = 1;
};

// Um kit carregado guarda os metadados e os buffers decodificados por pad,
// camada e round-robin. O ponteiro entregue ao motor é somente leitura.
struct LoadedKit
{
    Kit kit;
    std::array<std::vector<std::vector<std::shared_ptr<const SampleData>>>, kNumPads> layers;
    int padsWithAudio = 0;
    int totalSamples = 0;
    int failedSamples = 0;

    // Preview do kit decodificado no thread de carga (image.jpg/png da pasta ou
    // linha #image do .dfkit). Nulo quando nao ha foto ou se o arquivo falhou.
    juce::Image preview;
};

// Leitura de samples em background: o motor de áudio só vê o LoadedKit pronto.
class SampleStore
{
public:
    using Completion = std::function<void (std::unique_ptr<const LoadedKit>, juce::String)>;

    SampleStore();
    ~SampleStore();

    void loadAsync (Kit kit, Completion completion);
    void cancel();

    bool isLoading() const noexcept { return shared->loading.load (std::memory_order_relaxed); }
    int getProgress() const noexcept { return shared->progress.load (std::memory_order_relaxed); }

    static std::shared_ptr<const SampleData> readSample (juce::AudioFormatManager& formats,
                                                         const juce::File& file);

private:
    // Estado compartilhado com o job de fundo: a thread de audio e a UI podem
    // segurar este bloco mesmo depois que o SampleStore for destruido.
    struct SharedState
    {
        std::atomic<bool> loading { false };
        std::atomic<int> progress { 0 };
        std::atomic<int> generation { 0 };
    };

    juce::ThreadPool pool { 1 };
    std::shared_ptr<SharedState> shared { std::make_shared<SharedState>() };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampleStore)
};

} // namespace forja::drumforge
