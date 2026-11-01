#include "HostedLibrary.h"

namespace forjarack
{

HostedLibrary::HostedLibrary()
{
    // Formatos padrao COM suporte a UI (para o botao abrir editor nativo). Os
    // formatos reais dependem das flags de hosting do build (VST3/AU/VST2); sem
    // elas o manager fica vazio e o refresh devolve 0 (pasta ignorada).
    juce::addDefaultFormatsToManager (manager);
}

HostedLibrary::~HostedLibrary() = default;

juce::File HostedLibrary::rackFolder()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("Forja")
                      .getChildFile ("Rack");

    if (! folder.isDirectory())
        folder.createDirectory();

    return folder;
}

int HostedLibrary::refresh()
{
    found.clear();

    const auto folder = rackFolder();

    if (! folder.isDirectory())
        return 0;

    const juce::FileSearchPath path (folder.getFullPathName());

    // Varre formato por formato: cada um diz quais arquivos daquele diretorio
    // podem conte-lo e entonces enumera os descritores daqueles arquivos.
    for (auto* format : manager.getFormats())
    {
        const auto files = format->searchPathsForPlugins (path, true, false);

        for (const auto& fileOrId : files)
            format->findAllTypesForFile (found, fileOrId);
    }

    return found.size();
}

juce::StringArray HostedLibrary::fileNames() const
{
    juce::StringArray names;

    for (const auto* desc : found)
        names.add (desc->name);

    return names;
}

std::unique_ptr<HostedModule> HostedLibrary::load (const juce::String& fileName,
                                                   double sampleRate, int blockSize,
                                                   juce::String& error) const
{
    const juce::PluginDescription* match = nullptr;

    for (const auto* desc : found)
    {
        if (desc->name == fileName)
        {
            match = desc;
            break;
        }
    }

    if (match == nullptr)
    {
        error = "plugin nao encontrado na pasta do rack";
        return nullptr;
    }

    auto instance = manager.createPluginInstance (*match, sampleRate, blockSize, error);

    if (instance == nullptr)
        return nullptr;

    auto module = std::make_unique<HostedModule>();

    if (! module->adoptInstance (std::move (instance), error))
        return nullptr;

    return module;
}

} // namespace forjarack
