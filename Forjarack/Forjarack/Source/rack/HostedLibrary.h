#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "HostedModule.h"

namespace forjarack
{

// Bibliotecas de gaveta hospedada: varre Documents/Forja/Rack em busca de
// plugins (VST3/AU/VST2 shared) e monta HostedModules a partir deles. E a
// ponte entre "a pessoa soltou um arquivo na pasta" e "o engine tem um modulo
// naquele slot". Toda operacao e na thread de mensagens (o processor chama sob
// suspendProcessing); nada aqui toca no caminho de audio.
//
// Licenca: o plugin hospedado e uma obra separada carregada em runtime (modelo
// de DAW). Ele mantem a LICENCA dele (MIT, GPL, comercial, freeware) e o
// ForjaRack nao o linka em build. O codigo da suite segue MIT.
class HostedLibrary
{
public:
    HostedLibrary();
    ~HostedLibrary();

    HostedLibrary (const HostedLibrary&) = delete;
    HostedLibrary& operator= (const HostedLibrary&) = delete;

    // Pasta padrao das gavetas: Documents/Forja/Rack (criada se faltar).
    static juce::File rackFolder();

    // Revarre a pasta e atualiza a lista de descritores. Devolve quantos
    // plugins foram encontrados. Chamado na abertura do editor e no botao
    // PROCURAR da gaveta hospedada.
    int refresh();

    // Nomes exibidos (estaveis, na ordem do scan). Vazio se nada encontrado.
    juce::StringArray fileNames() const;

    // Monta um HostedModule a partir do descritor cujo nome de exibicao e
    // `fileName`. `sampleRate`/`blockSize` sao as dicas de criacao do plugin (o
    // engine re-prepare depois com os valores reais). Devolve nulo + motivo em
    // `error` se nao acha ou nao carrega. Nao prepara: o engine.adoptSlotModule
    // faz setBlockSize + prepare.
    std::unique_ptr<HostedModule> load (const juce::String& fileName,
                                        double sampleRate, int blockSize,
                                        juce::String& error) const;

private:
    juce::AudioPluginFormatManager manager;
    juce::OwnedArray<juce::PluginDescription> found;
};

} // namespace forjarack
