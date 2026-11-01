#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "RackState.h"

#include "forja/dsp/ForjaCera.h"
#include "forja/dsp/ForjaCinzel.h"
#include "forja/dsp/ForjaCorpo.h"
#include "forja/dsp/ForjaDique.h"
#include "forja/dsp/ForjaEco.h"
#include "forja/dsp/ForjaEscoria.h"
#include "forja/dsp/ForjaFolego.h"
#include "forja/dsp/ForjaGranalha.h"
#include "forja/dsp/ForjaImpeto.h"
#include "forja/dsp/ForjaLaminador.h"
#include "forja/dsp/ForjaLima.h"
#include "forja/dsp/ForjaLixa.h"
#include "forja/dsp/ForjaMalho.h"
#include "forja/dsp/ForjaMel.h"
#include "forja/dsp/ForjaMuralha.h"
#include "forja/dsp/ForjaOxido.h"
#include "forja/dsp/ForjaPrensa.h"
#include "forja/dsp/ForjaRebarba.h"
#include "forja/dsp/ForjaRepique.h"
#include "forja/dsp/ForjaSalao.h"
#include "forja/dsp/ForjaTempera.h"
#include "forja/dsp/ForjaTorno.h"

namespace forjarack
{

// Interface uniforme de um modulo de slot. Cada modulo envolve um adaptador
// Airwindows MIT ja validado; o rack so conhece este contrato. Efeitos
// processam in place (estereo); instrumentos recebem MIDI via processMidi e
// escrevem audio proprio nos buffers fornecidos (o engine soma na cadeia).
// Expansoes futuras (leitor sf2/sf3, mini drum, outros mini plugins) entram
// pelo mesmo contrato, registradas no ModuleCatalog.
class ModuleBase
{
public:
    virtual ~ModuleBase() = default;

    virtual void prepare (double sampleRate) = 0;
    virtual void setParam (int index, float value) = 0;
    virtual void setBypass (bool bypassed) = 0;
    virtual void process (float* left, float* right, int numSamples) = 0;
    virtual float readOutputPeak() = 0;
    virtual int latencySamples() const = 0;

    // Tamanho de bloco maxima com que o engine vai rodar (dado antes de prepare).
    // So quem monta um grafo de edicao interno (modulo hospedado) precisa; os
    // adaptadores de um buffer fixo ignoram. Nao aloca no caminho de audio.
    virtual void setBlockSize (int maxSamples) noexcept { (void) maxSamples; }

    // Reducao de ganho do ultimo bloco em dB (negativa; 0 = comprimir nada).
    // Compressores/limitadores sobrescrevem pelo MeterFrame do adaptador;
    // modulos sem deteccao de nivel devolvem 0 e a gaveta nao mostra a barra.
    virtual float readGainReductionDb() const noexcept { return 0.0f; }

    // Instrumentos sobrescrevem: geram audio a partir de MIDI, ignorando o
    // conteudo recebido nos buffers. Chamado antes de process() no mesmo bloco.
    virtual bool isInstrument() const noexcept { return false; }
    virtual void processMidi (const MidiEventSpan&) noexcept {}

    // Faxina na thread de mensagens (o engine expoe idleModules): modulos que
    // carregam material em background (mini drum) liberam o substituido aqui.
    virtual void idle() {}

    // ---- gaveta de arquivos (instrumentos com biblioteca propria) ----------
    // MINIDRUM e CANIVETE tem mais que um knob de indice: expoe a lista da
    // biblioteca no seletor da gaveta e aceita um arquivo fora dela escolhido
    // no FileChooser do processador. Tudo aqui roda na thread de mensagens;
    // std::string carrega o caminho cru (o nucleo do rack nao conhece juce::File
    // nem UTF-8: as libs aceitam ASCII; a UI mostra so o nome do arquivo).
    virtual bool supportsDrawerFiles() const noexcept { return false; }
    virtual std::vector<std::string> drawerFileNames() const { return {}; }
    // Revarre a biblioteca propria (botao BUSCAR do rack depositou arquivo novo).
    virtual void drawerRefreshLibrary() {}
    // Nome do material carregado agora ("" = nada carregado). Biblioteca:
    // o nome da lista; externo: o nome do arquivo aberto.
    virtual std::string drawerCurrentName() const { return {}; }
    // Escolha pela lista da biblioteca (caminho exato de drawerFileNames);
    // o modulo adota no proximo idle(). False se o nome nao esta na lista.
    virtual bool drawerSelectFile (const std::string& name) { (void) name; return false; }
    // Arquivo escolhido fora da biblioteca (caminho absoluto). O modulo carrega
    // e deixa de obedecer ao knob de indice ate a proxima escolha pela lista.
    virtual void drawerLoadExternalFile (const std::string& path) { (void) path; }
};

template <typename Dsp>
class AdapterModule : public ModuleBase
{
public:
    void prepare (double sampleRate) override { dsp.prepare (sampleRate); }

    void setBypass (bool bypassed) override { params.bypass = bypassed; }

    void process (float* left, float* right, int numSamples) override
    {
        dsp.setParameters (params);
        dsp.process (left, right, numSamples);

        const auto frame = dsp.readMeters (true);
        lastPeak = std::max (frame.outPeakL, frame.outPeakR);
        lastGrDb = frame.gainReductionDb;
    }

    float readOutputPeak() override { return lastPeak; }
    int latencySamples() const override { return dsp.latencySamples(); }
    float readGainReductionDb() const noexcept override { return lastGrDb; }

protected:
    Dsp dsp;
    typename Dsp::Parameters params {};
    float lastPeak = 0.0f;
    float lastGrDb = 0.0f;
};

// Cada wrapper fixa o mapa indice -> campo do Parameters do adaptador. Campos
// nao expostos ficam no default do adaptador (neutro para o modelo).
class CeraModule final : public AdapterModule<forja::dsp::CeraDSP>
{
public:
    void setParam (int index, float value) override
    {
        if (index == 0)
            params.warmth = value;
    }
};

class LixaModule final : public AdapterModule<forja::dsp::LixaDSP>
{
public:
    void setParam (int index, float value) override
    {
        if (index == 0)
            params.drive = value;
    }
};

class MelModule final : public AdapterModule<forja::dsp::MelDSP>
{
public:
    void setParam (int index, float value) override
    {
        if (index == 0)
            params.sweeten = value;
    }
};

class TemperaModule final : public AdapterModule<forja::dsp::TemperaDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.compress = value; break;
            case 1: params.output = value; break;
            case 2: params.mix = value; break;
            default: break;
        }
    }
};

class MuralhaModule final : public AdapterModule<forja::dsp::MuralhaDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.boost = value; break;
            case 1: params.match = value; break;
            case 2: params.noise = value; break;
            case 3: params.ceiling = value; break;
            case 4: params.mode = value; break;
            default: break;
        }
    }
};

class CinzelModule final : public AdapterModule<forja::dsp::CinzelDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.highGain = value; break;
            case 1: params.highMidGain = value; break;
            case 2: params.lowMidGain = value; break;
            case 3: params.highFreq = value; break;
            case 4: params.lowMidFreq = value; break;
            case 5: params.mix = value; break;
            default: break;
        }
    }
};

class OxidoModule final : public AdapterModule<forja::dsp::OxidoDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.input = value; break;
            case 1: params.tapeHigh = value; break;
            case 2: params.tapeLow = value; break;
            case 3: params.flutter = value; break;
            case 4: params.noise = value; break;
            case 5: params.output = value; break;
            case 6: params.invDryWet = value; break;
            default: break;
        }
    }
};

class FolegoModule final : public AdapterModule<forja::dsp::FolegoDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.regen = value; break;
            case 1: params.derez = value; break;
            case 2: params.filter = value; break;
            case 3: params.earlyRf = value; break;
            case 4: params.position = value; break;
            case 5: params.dryWet = value; break;
            default: break;
        }
    }
};

class SalaoModule final : public AdapterModule<forja::dsp::SalaoDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.regen = value; break;
            case 1: params.position = value; break;
            case 2: params.distance = value; break;
            case 3: params.downRez = value; break;
            case 4: params.dryWet = value; break;
            default: break;
        }
    }

    void prepare (double sampleRate) override
    {
        // No rack o SALAO roda sem o room tone do kRockstar2: sem entrada o
        // master nao pode assobiar com a mesa parada (mesma decisao do Drumforge).
        AdapterModule::prepare (sampleRate);
        params.roomTone = 0.0f;
    }
};

class RepiqueModule final : public AdapterModule<forja::dsp::RepiqueDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.time = value; break;
            case 1: params.regen = value; break;
            case 2: params.freq = value; break;
            case 3: params.reso = value; break;
            case 4: params.flutter = value; break;
            case 5: params.dryWet = value; break;
            default: break;
        }
    }

    void prepare (double sampleRate) override
    {
        AdapterModule::prepare (sampleRate);
        params.sync = false;   // rack roda assincrono; tempo vem do knob
    }
};

// Modulos de um knob das familias OneKnob e seus primos: curva nao-linear com
// um unico controle de porta do sinal.
class CorpoModule final : public AdapterModule<forja::dsp::CorpoDSP>
{
public:
    void setParam (int index, float value) override
    {
        if (index == 0)
            params.input = value;
    }
};

class ImpetoModule final : public AdapterModule<forja::dsp::ImpetoDSP>
{
public:
    void setParam (int index, float value) override
    {
        if (index == 0)
            params.input = value;
    }
};

class RebarbaModule final : public AdapterModule<forja::dsp::RebarbaDSP>
{
public:
    void setParam (int index, float value) override
    {
        if (index == 0)
            params.slew = value;
    }
};

class DiqueModule final : public AdapterModule<forja::dsp::DiqueDSP>
{
public:
    void setParam (int, float) override {}   // SafeLimiting nao tem knob
};

class TornoModule final : public AdapterModule<forja::dsp::TornoDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.input = value; break;
            case 1: params.treble = value; break;
            case 2: params.bass = value; break;
            default: break;
        }
    }
};

class LimaModule final : public AdapterModule<forja::dsp::LimaDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.high = value; break;
            case 1: params.highMid = value; break;
            case 2: params.lowMid = value; break;
            case 3: params.bass = value; break;
            case 4: params.highFreq = value; break;
            case 5: params.highMidFreq = value; break;
            case 6: params.lowMidFreq = value; break;
            case 7: params.bassFreq = value; break;
            default: break;
        }
    }
};

class MalhoModule final : public AdapterModule<forja::dsp::MalhoDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.compress = value; break;
            case 1: params.attack = value; break;
            case 2: params.release = value; break;
            case 3: params.drive = value; break;
            case 4: params.mix = value; break;
            default: break;
        }
    }
};

class PrensaModule final : public AdapterModule<forja::dsp::PrensaDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.pressure = value; break;
            case 1: params.speed = value; break;
            case 2: params.mewiness = value; break;
            case 3: params.pawclaw = value; break;
            case 4: params.output = value; break;
            case 5: params.mix = value; break;
            default: break;
        }
    }
};

class EscoriaModule final : public AdapterModule<forja::dsp::EscoriaDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.intense = value; break;
            case 1: params.sharp = value; break;
            case 2: params.depth = value; break;
            case 3: params.filter = value; break;
            case 4: params.sense = value; break;
            default: break;
        }
    }
};

class GranalhaModule final : public AdapterModule<forja::dsp::GranalhaDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.derez = value; break;
            case 1: params.trim = value; break;
            case 2: params.square = value; break;
            case 3: params.bright = value; break;
            case 4: params.bassis = value; break;
            case 5: params.output = value; break;
            default: break;
        }
    }
};

class LaminadorModule final : public AdapterModule<forja::dsp::LaminadorDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.input = value; break;
            case 1: params.tilt = value; break;
            case 2: params.shape = value; break;
            case 3: params.flutter = value; break;
            case 4: params.flutSpd = value; break;
            case 5: params.bias = value; break;
            case 6: params.headBmp = value; break;
            case 7: params.headFrq = value; break;
            case 8: params.output = value; break;
            default: break;
        }
    }
};

// ECO roda assincrono igual ao REPIQUE: o rack nao lê playhead, entao a
// velocidade vem do knob e sync/division ficam no default do adaptador.
class EcoModule final : public AdapterModule<forja::dsp::EcoDSP>
{
public:
    void setParam (int index, float value) override
    {
        switch (index)
        {
            case 0: params.select = value; break;
            case 1: params.regen = value; break;
            case 2: params.speed = value; break;
            case 3: params.dryWet = value; break;
            default: break;
        }
    }

    void prepare (double sampleRate) override
    {
        AdapterModule::prepare (sampleRate);
        params.sync = false;
    }
};

} // namespace forjarack
