#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "Module.h"
#include "RackState.h"

namespace forjarack
{

// Um knob visivel de um modulo: rotulo ASCII (o nome acentuado vive no
// FUNDAMENTOS) e o default documentado do adaptador original.
struct KnobSpec
{
    const char* label = "";
    float defaultValue = 0.5f;
};

// Ficha de um modulo no registro: identidade estavel, familia visual, acento
// da gaveta, papel (efeito ou instrumento), mapa de knobs e fabrica.
struct ModuleSpec
{
    using Factory = std::unique_ptr<ModuleBase> (*)();

    const char* id = "vazio";          // ASCII, estavel; imutavel apos publicado
    const char* name = "VAZIO";        // rotulo no seletor da gaveta
    ModuleCategory category = ModuleCategory::color;
    std::uint32_t accent = 0xff6b7280; // ARGB, cor da fachada da gaveta
    ModuleRole role = ModuleRole::effect;
    std::uint8_t numParams = 0;
    std::array<KnobSpec, kMaxModuleParams> knobs {};
    Factory factory = nullptr;         // nullptr = pass-through (vazio)
    ModuleFace face = ModuleFace::generic; // marca d'agua da gaveta

    // Compressores/limitadores que medem reducao de ganho real no adaptador
    // acendem a barra de GR da gaveta. So e true quando o DSP do modulo devolve
    // gainReductionDb != 0 (nao inventamos telemetria: o medidor so mostra o
    // que o modelo ja calcula).
    bool showsGainReduction = false;
};

// Registro de modulos do rack: unica fonte de verdade para UI, defaults e
// fabrica. Os built-in chegam no ModuleCatalog.cpp; expansoes (leitor
// sf2/sf3, mini drum que le kits do Drumforge, outros mini plugins) se
// publicam por registerModule() na inicializacao da thread de mensagens.
//
// Regras do registro (preservam presets de usuarios):
//  - indice e a posicao na ordem de registro; o seletor do APVTS grava o
//    indice, entao modulos publicados NUNCA podem ser removidos ou reordenados
//    - novas expansoes entram sempre ACRESCENTADAS ao fim;
//  - 'id' e o contrato de longo prazo: nunca mude a string de um modulo ja
//    publicado, mesmo que o rotulo mude;
//  - o caminho de audio nunca toca o registro (o engine guarda ponteiros ja
//    criados sob suspendProcessing); indice fora do registro = pass-through.
class ModuleCatalog
{
public:
    static int count();
    static const ModuleSpec& specByIndex (int index);   // fora da faixa = vazio
    static int indexOf (const char* id);                // -1 se nao existe

    // Publica uma expansao. Somente thread de mensagens (init); id duplicado
    // e rejeitado.
    static bool registerModule (const ModuleSpec& spec);

    // Fabrica: devolve o modulo pronto ou nullptr para 'vazio'/indice invalido.
    static std::unique_ptr<ModuleBase> create (int index);

    // Defaults documentados do modulo (0..1), posicao alem de numParams = 0.
    static std::array<float, kMaxModuleParams> defaultParams (int index);
};

} // namespace forjarack
