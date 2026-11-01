#pragma once

#include <juce_core/juce_core.h>

namespace forjarackui
{

// Contrato fino entre a gaveta hospedada (SlotComponent) e o que o processador
// sabe sobre o plugin carregado naquele slot. A UI nunca toca no engine nem no
// AudioPluginInstance diretamente: so pergunta (quantos knobs, rotulos, tem
// editor nativo) e pede (escolher arquivo, abrir editor). O processor implementa.
//
// O numero de knobs e os rotulos de uma gaveta hospedada NAO vem do ModuleSpec
// (que e fixo); vem do plugin vivo no slot, por isto a interface.
class IHostedSlot
{
public:
    virtual ~IHostedSlot() = default;

    // Verdadeiro se o indice de modulo do slot e a gaveta HOSPEDADO.
    virtual bool isHosted (int slot) const = 0;

    // Arquivos de plugin encontrados em Documents/Forja/Rack (nomes exibidos,
    // ordem estavel). Vazio se a pasta nao existe ou esta vazia.
    virtual juce::StringArray fileList() const = 0;

    // Nome do arquivo escolhido para o slot (vazio = nenhum carregado).
    virtual juce::String fileName (int slot) const = 0;
    // Carrega (ou recarrega) o arquivo no slot; nullifica se nome vazio.
    virtual void chooseFile (int slot, const juce::String& name) = 0;
    // Revarre Documents/Forja/Rack (botao PROCURAR). Devolve quantos encontrou.
    virtual int rescan() = 0;

    // Mapa de knobs do plugin carregado: quantos (ate o teto do rack) e o
    // rotulo de cada um. 0 quando nao ha instancia viva.
    virtual int paramCount (int slot) const = 0;
    virtual juce::String paramLabel (int slot, int index) const = 0;

    // UI nativa do plugin: se ha, abre em janela separada (a "aba" pedida para
    // plugins cuja tela nao segue o padrao Forja). Sem tela propria, o processador
    // monta uma lista generica de parametros.
    virtual bool canOpenEditor (int slot) const = 0;
    virtual void openEditor (int slot) = 0;
};

} // namespace forjarackui
