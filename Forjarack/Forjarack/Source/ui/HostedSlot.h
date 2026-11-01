#pragma once

#include <juce_core/juce_core.h>

namespace forjarackui
{

// Contrato fino entre a gaveta de arquivos (SlotComponent) e o que o
// processador sabe sobre o slot. A UI nunca toca no engine nem no
// AudioPluginInstance diretamente: so pergunta (quantos knobs, rotulos, tem
// editor nativo) e pede (escolher arquivo, abrir editor). O processor implementa.
//
// Duas origens para a mesma banda de arquivos:
//  - HOSPEDADO: a lista sao arquivos de plugin em Documents/Forja/Rack; o
//    numero de knobs e os rotulos NAO vem do ModuleSpec (que e fixo), vem do
//    plugin vivo no slot.
//  - Gaveta de instrumento com biblioteca propria (MINIDRUM, CANIVETE): a
//    lista e a biblioteca do modulo (kits .dfkit, banks .sf2/.sf3) e ARQUIVO
//    abre um FileChooser para trazer material de fora da biblioteca.
class IHostedSlot
{
public:
    virtual ~IHostedSlot() = default;

    // Verdadeiro se o indice de modulo do slot e a gaveta HOSPEDADO.
    virtual bool isHosted (int slot) const = 0;

    // Arquivos de plugin encontrados em Documents/Forja/Rack (nomes exibidos,
    // ordem estavel). Vazio se a pasta nao existe ou esta vazia.
    virtual juce::StringArray fileList() const = 0;

    // Nome do arquivo escolhido para o slot (vazio = nenhum carregado). Na
    // gaveta de instrumentos e o material vivo (nome exibido da lista ou do
    // externo carregado; sem modulo, o nome da escolha persistida).
    virtual juce::String fileName (int slot) const = 0;
    // Carrega (ou recarrega) o arquivo no slot; nullifica se nome vazio.
    // Na gaveta de instrumentos, nome vazio devolve o knob ao comando.
    virtual void chooseFile (int slot, const juce::String& name) = 0;
    // Revarre Documents/Forja/Rack (botao BUSCAR da hospedada) e reescaneia as
    // bibliotecas dos modulos-drawer. Devolve quantos a hospedada encontrou.
    virtual int rescan() = 0;
    // ARQUIVO na gaveta de instrumentos: FileChooser para um kit/bank fora da
    // biblioteca. No HOSPEDADO nao ha arquivo a escolher no disco: revarre a
    // pasta de plugins (mesmo efeito do BUSCAR).
    virtual void browseFile (int slot) = 0;

    // Mapa de knobs do plugin carregado: quantos (ate o teto do rack) e o
    // rotulo de cada um. 0 quando nao ha instancia viva.
    virtual int paramCount (int slot) const = 0;
    virtual juce::String paramLabel (int slot, int index) const = 0;

    // UI nativa do plugin: se ha, abre em janela separada (a "aba" pedida para
    // plugins cuja tela nao segue o padrao Forja). Sem tela propria, o processador
    // monta uma lista generica de parametros.
    virtual bool canOpenEditor (int slot) const = 0;
    virtual void openEditor (int slot) = 0;

    // ---- gaveta de instrumentos com biblioteca (MINIDRUM, CANIVETE) --------
    // Mesma banda do HOSPEDADO (seletor + botao), mas a lista e a biblioteca
    // do modulo e o botao e ARQUIVO (FileChooser) em vez de EDITAR.
    virtual bool isDrawer (int slot) const = 0;
    // Biblioteca do modulo como caminhos completos (identificadores da lista;
    // a UI mostra o nome). O rotulo do que esta carregado vem de fileName().
    virtual juce::StringArray drawerFileNames (int slot) const = 0;

    // ---- instrumento interno (CANIVETE: presets do bank carregado) ---------
    // Segunda fileira na gaveta, so para modulos cujo bank tem lista de
    // presets. A lista e viva: fica vazia enquanto o bank carrega em background.
    virtual bool isDrawerWithInstruments (int slot) const { (void) slot; return false; }
    virtual juce::StringArray drawerInstrumentNames (int slot) const
        { (void) slot; return {}; }
    virtual juce::String currentInstrument (int slot) const { (void) slot; return {}; }
    // Escolhe o preset pelo NOME (o indice nao e estavel entre banks); o modulo
    // resolve quando o bank assentar. Nome vazio = seguir o preset default.
    virtual void chooseInstrument (int slot, const juce::String& name)
        { (void) slot; (void) name; }
};

} // namespace forjarackui
