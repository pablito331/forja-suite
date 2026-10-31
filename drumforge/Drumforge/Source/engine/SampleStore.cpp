#include "SampleStore.h"

namespace forja::drumforge
{

namespace
{

// O ThreadPool do JUCE só aceita ThreadPoolJob; este adaptador dá um job
// descartável para uma tarefa qualquer, apagado pelo pool ao terminar.
class FunctionJob final : public juce::ThreadPoolJob
{
public:
    explicit FunctionJob (std::function<void()> taskToRun)
        : juce::ThreadPoolJob ("drumforge-load"), task (std::move (taskToRun))
    {
    }

    JobStatus runJob() override
    {
        task();
        return jobHasFinished;
    }

private:
    std::function<void()> task;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FunctionJob)
};

// Decodifica a foto de preview no thread de carga: uma JPEG 4K do drum_sklad
// travaria a UI e moraria na RAM por um quadradinho. Reduzida para
// no maximo 512 px; arquivo podre vira imagem nula e a UI desenha a arte vetorial.
juce::Image loadPreviewImage (const juce::String& path)
{
    if (path.isEmpty())
        return {};

    const juce::File file (path);

    if (! file.existsAsFile() || file.getSize() <= 0)
        return {};

    auto stream = file.createInputStream();

    if (stream == nullptr)
        return {};

    auto image = juce::ImageFileFormat::loadFrom (*stream);

    if (image.isNull())
        return {};

    constexpr int maxEdge = 512;
    const int width = image.getWidth();
    const int height = image.getHeight();

    if (juce::jmax (width, height) > maxEdge)
    {
        const double scale = (double) maxEdge / (double) juce::jmax (width, height);
        image = image.rescaled (juce::jmax (1, (int) (width * scale + 0.5)),
                                juce::jmax (1, (int) (height * scale + 0.5)));
    }

    return image;
}

} // namespace

SampleStore::SampleStore() = default;

SampleStore::~SampleStore()
{
    cancel();
}

void SampleStore::cancel()
{
    shared->generation.fetch_add (1, std::memory_order_release);
    pool.removeAllJobs (true, 4000);
    shared->loading.store (false, std::memory_order_relaxed);
}

std::shared_ptr<const SampleData> SampleStore::readSample (juce::AudioFormatManager& formats,
                                                           const juce::File& file)
{
    auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file));

    if (reader == nullptr)
        return nullptr;

    const double sourceRate = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;

    // Teto de 60 s por sample: evita um arquivo corrompido puxar gigabytes.
    const auto maxLength = (juce::int64) (sourceRate * 60.0);
    const int length = (int) juce::jlimit<juce::int64> (1, maxLength, reader->lengthInSamples);
    const int channels = juce::jlimit (1, 2, (int) reader->numChannels);

    auto data = std::make_shared<SampleData>();
    data->sampleRate = sourceRate;
    data->length = length;
    data->channels = channels;
    data->audio.setSize (channels, length);

    if (! reader->read (&data->audio, 0, length, 0, true, channels > 1))
        return nullptr;

    return data;
}

void SampleStore::loadAsync (Kit kit, Completion completion)
{
    auto state = shared;
    const int myGeneration = state->generation.fetch_add (1, std::memory_order_acq_rel) + 1;

    state->loading.store (true, std::memory_order_relaxed);
    state->progress.store (0, std::memory_order_relaxed);

    pool.addJob (new FunctionJob ([state, kit = std::move (kit), completion = std::move (completion),
                                    myGeneration]
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        auto loaded = std::make_unique<LoadedKit>();
        loaded->kit = kit;

        const int total = juce::jmax (1, kit.sampleCount());
        int done = 0;

        for (int padIndex = 0; padIndex < kNumPads; ++padIndex)
        {
            const auto& pad = kit.pads[(size_t) padIndex];
            auto& padLayers = loaded->layers[(size_t) padIndex];
            padLayers.clear();

            for (const auto& layer : pad.layers)
            {
                std::vector<std::shared_ptr<const SampleData>> roundRobins;
                roundRobins.reserve (layer.size());

                for (const auto& path : layer)
                {
                    if (state->generation.load (std::memory_order_acquire) != myGeneration)
                        return;

                    if (path.isNotEmpty())
                    {
                        auto sample = readSample (formats, juce::File (path));

                        if (sample == nullptr)
                            ++loaded->failedSamples;
                        else
                        {
                            ++loaded->totalSamples;
                            roundRobins.push_back (std::move (sample));
                        }
                    }

                    ++done;
                    state->progress.store (juce::jlimit (0, 99, (done * 100) / total),
                                           std::memory_order_relaxed);
                }

                if (! roundRobins.empty())
                    padLayers.push_back (std::move (roundRobins));
            }

            if (padLayers.size() > (size_t) kMaxLayers)
                padLayers.resize ((size_t) kMaxLayers);

            if (! padLayers.empty())
                ++loaded->padsWithAudio;
        }

        if (state->generation.load (std::memory_order_acquire) != myGeneration)
            return;

        loaded->preview = loadPreviewImage (kit.imageFile);

        state->loading.store (false, std::memory_order_relaxed);
        state->progress.store (100, std::memory_order_relaxed);

        juce::MessageManager::callAsync ([state, completion = std::move (completion),
                                          loaded = std::move (loaded), myGeneration] () mutable
        {
            if (state->generation.load (std::memory_order_acquire) == myGeneration && completion != nullptr)
            {
                juce::String error;

                if (loaded->totalSamples == 0)
                    error = "Nenhum sample pode ser lido. Confira os caminhos do kit.";
                else if (loaded->failedSamples > 0)
                    error = juce::String (loaded->failedSamples) + " sample(s) ignorado(s) por falha de leitura.";

                completion (std::move (loaded), error);
            }
        });
    }), true);
}

} // namespace forja::drumforge
