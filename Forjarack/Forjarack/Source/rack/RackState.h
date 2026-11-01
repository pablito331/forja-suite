#pragma once

#include <array>
#include <cstdint>

namespace forjarack
{

// Rack Serie 500: 8 slots verticais em cadeia. Cada slot hospeda um modulo que
// envolve um adaptador Airwindows MIT ja validado. Parametros sao normalizados
// 0..1 e o escalonamento mora no DSP (regra da suite).
inline constexpr int kNumSlots = 8;
// Teto de 10 porque o LAMINADOR (TapeHeads) tem nove knobs expostos. O numero
// de knobs por gaveta vem do spec do modulo, entao teto maior nao muda nenhuma
// gaveta que ja existe.
inline constexpr int kMaxModuleParams = 10;

// Teto de eventos MIDI curtidos copiados por bloco para o span POD do rack.
// Excedentes sao descartados (instrumentos reais raramente passam disso).
inline constexpr int kMaxRackMidiEvents = 256;

// Identidade visual/funcional de um modulo no registro. 'effect' transforma o
// audio da cadeia in place; 'instrument' gera audio proprio, que o engine soma
// na cadeia (o sinal recebido passa intacto pelo slot).
enum class ModuleRole : std::uint8_t
{
    effect = 0,
    instrument
};

// Agrupamento visual: da a cada gaveta uma cor de fachada coerente com a paleta.
enum class ModuleCategory : std::uint8_t
{
    color = 0,     // aquecimento / saturacao leve
    drive,         // saturacao / fita
    dynamics,      // compressao / limitacao
    eq,            // equalizacao
    space,         // ambiente / delay
    lofi,          // degradacao intencional (bits, taxa, forma)
    instrument,    // gera audio (sf2/sf3, mini drum, futuros modulos expansiveis)
    hosted,        // plugin externo carregado em runtime de Documents/Forja/Rack
    numCategories
};

// "Cara" de hardware da gaveta: desenho esmaecido (marca d'agua) que aparece
// atras dos knobs para dar identidade ao modulo, sem mexer no layout. E
// ortogonal a ModuleCategory (que so decide a cor): OXIDO e familia drive mas
// tem face tape. 'generic' nao desenha nada; um instrumento generico cai em
// 'drum'. 'valve' e 'drum' ja tem renderer mas ainda sem dono no rack --
// acendem quando um saturador de valvula ou o mini drum entrarem no registro.
enum class ModuleFace : std::uint8_t
{
    generic = 0, // sem marca (vazio / sem tema definido)
    warmth,      // cor: arcs de calor
    drive,       // saturacao solida: onda clipada
    tape,        // maquina de fita: rolos + trilha
    dynamics,    // compressor/limiter: mostrador VU
    eq,          // equalizador: curva de resposta
    space,       // ambiente/delay: ondas concentricas
    digital,     // lo-fi: grade de pixels / bit-crush
    valve,       // valvula: bulbo + placas (gancho, sem dono ainda)
    drum         // kit: aro + peles (gancho para instrumentos)
};

// Evento MIDI curto reduzido a POD: o rack nao expoe juce::MidiMessage ao
// modulo para o nucleo permanecer compilavel sem JUCE (testes headless).
struct MidiEvent
{
    int frame = 0;              // amostra dentro do bloco atual
    std::uint8_t channel = 0;
    std::uint8_t note = 0;
    std::uint8_t velocity = 0;  // 0 = note off
    bool noteOn = false;
};

struct MidiEventSpan
{
    const MidiEvent* events = nullptr;
    int numEvents = 0;

    // Ponteiro opaco para o buffer MIDI nativo do host (juce::MidiBuffer*) quando
    // um modulo hospedado precisa da trilha completa (CC/pitch bend/program). O
    // nucleo e compilavel sem JUCE entao ve apenas void*; os modulos POD-propri
    // (minidrum, canivete) e os testes headless o deixam em nullptr.
    void* native = nullptr;
};

// Instantaneo por slot lido pelo audio. POD, sem alocar no caminho de audio.
// moduleIndex e a posicao no registro do catalogo; indice desconhecido vira
// pass-through (o grafo so contem modulos criados na thread de mensagens).
//
// A gaveta tem uma unica chave de on/off (bypass). Power e mute existiram e
// foram removidos: numa cadeia em serie os tres significavam "nao processa",
// entao so duplicavam a decisao. O solo continua e e a unica chave que altera
// as outras gavetas.
struct SlotSnapshot
{
    int moduleIndex = 0;       // 0 = vazio (painel cego)
    bool bypass = false;       // true = pass-through exato nessa gaveta
    bool solo = false;         // so conta se a gaveta nao estiver em bypass
    std::array<float, kMaxModuleParams> params {};
};

struct RackSnapshot
{
    std::array<SlotSnapshot, kNumSlots> slots {};
    float inputTrim = 0.5f;   // 0..1 -> -24..+24 dB
    float outputTrim = 0.5f;  // 0..1 -> -24..+24 dB
    bool phaseInvert = false;
    bool globalBypass = false;
};

} // namespace forjarack
