#pragma once

#include <juce_core/juce_core.h>

namespace forja::pianoforge
{

// Leitor do container .pforge (magic "PFG1"), o formato que o packaging do
// projeto-irmao empacota. Porta de pianoforge/src/core/Bundle.{hpp,cpp} para o
// padrao da suite: nada de miniz -- CRC32 via juce::CRC32, payload zlib via
// juce::GZIPDecompressorInputStream e manifest via juce::JSON.
//
// Layout: magic(4) version u16 flags u16 ndir u32, entao ndir registros de nome
// variavel, entao os payloads. Cada registro traz o CRC32 do payload gravado, e
// nada e entregue a um sampler sem essa conta bater: bundle corrompido vira erro
// legivel, nunca ruido na saida nem o banco no ar derrubado.
//
// Nada aqui e seguro para a thread de audio: so roda no worker de carga.
class Bundle
{
public:
    enum : std::uint16_t { kRaw = 0, kZlib = 1, kManifest = 2 };

    struct Entry
    {
        juce::String name;      // caminho relativo dentro do bundle
        std::uint16_t type = kRaw;
        std::uint64_t off = 0;  // deslocamento do payload no arquivo
        std::uint64_t size = 0; // bytes gravados
        std::uint64_t usize = 0; // bytes depois de descompactar
        std::uint32_t crc = 0;
    };

    // manifest.presets[]: nome, arte, banco e os 17 parametros de um preset.
    struct Preset
    {
        juce::String name;
        juce::String image;
        juce::String sf2;
        juce::NamedValueSet params;
    };

    // So le cabecalho, diretorio e o manifest. Um bundle de gigabyte nao entra
    // inteiro na memoria aqui.
    bool open (const juce::File& file, juce::String& err);

    const juce::Array<Entry>& entries() const noexcept { return entries_; }
    const juce::String& manifest() const noexcept { return manifest_; }
    bool isOpen() const noexcept { return ! entries_.isEmpty(); }

    // O banco do bundle: o arquivo dito no manifest (presets[].sf2), senao o
    // primeiro entry que parece banco. nullptr se nao ha.
    const Entry* bankEntry() const noexcept;

    // "produto" do manifest, para a lista da UI mostrar o nome do piano.
    juce::String productName() const;

    // manifest.presets[], na ordem gravada.
    const juce::Array<Preset>& presets() const noexcept { return presets_; }

    // Confere o CRC32 do entry lendo-o em janelas de 64 KB (nada cru na RAM do
    // audio). Falso com erro legivel quando o CRC nao bate.
    bool verifyEntry (const Entry& e, juce::String& err) const;

    // Le o payload para um MemoryBlock ja descompactado, conferindo o CRC32.
    // Para o SF2 cru dentro do bundle: prefira verifyEntry + extractBankBytes.
    bool readEntry (const Entry& e, juce::MemoryBlock& out, juce::String& err) const;

    // O bytes do banco do bundle (verifica CRC e descompacta se preciso) prontos
    // para tsf_load_memory. Falso com erro legivel quando ha corrupecao.
    bool extractBankBytes (juce::MemoryBlock& out, juce::String& err) const;

    const juce::File& path() const noexcept { return path_; }

private:
    juce::File path_;
    juce::Array<Entry> entries_;
    juce::String manifest_;
    juce::Array<Preset> presets_;
};

} // namespace forja::pianoforge
