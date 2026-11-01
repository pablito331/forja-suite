// Gaveta HOSPEDADA: hosting dinamico de plugins de Documents/Forja/Rack. Nao ha
// paridade aqui (o DSP vem de fora); o que se valida e o bridge. Um stub de
// AudioPluginInstance cobre adocao de efeito/instrumento, deteccao de papel por
// bus, mapa dinamico de params, trava da UI nativa, MIDI nativo (void* do core)
// e a propagacao da latencia do plugin pelo engine ate o host.
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "rack/HostedModule.h"
#include "rack/ModuleCatalog.h"
#include "rack/RackEngine.h"
#include "rack/RackState.h"

namespace
{

int failures = 0;

void check (bool condition, const char* label)
{
    if (! condition)
    {
        std::printf ("  FAIL: %s\n", label);
        ++failures;
    }
    else
    {
        std::printf ("  ok: %s\n", label);
    }
}

std::vector<float> makeSine (int n, double sr, double hz)
{
    std::vector<float> buf (static_cast<size_t> (n));

    for (int i = 0; i < n; ++i)
        buf[(size_t) i] = static_cast<float> (0.3 * std::sin (2.0 * 3.14159265358979 * hz * i / sr));

    return buf;
}

bool allFinite (const std::vector<float>& a)
{
    for (float v : a)
        if (! std::isfinite (v))
            return false;

    return true;
}

bool anyDifferent (const std::vector<float>& a, const std::vector<float>& b)
{
    for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        if (std::abs (a[i] - b[i]) > 1e-9f)
            return true;

    return false;
}

bool approxScale (const std::vector<float>& a, const std::vector<float>& in, float gain)
{
    for (size_t i = 0; i < a.size(); ++i)
        if (std::abs (a[i] - in[i] * gain) > 1e-6f)
            return false;

    return true;
}

// Stub base: um AudioPluginInstance de mentira, exatamente o tipo de objeto que
// o HostedLibrary devolveria apos carregar um .vst3 da pasta Rack. O core do
// rack so enxerga ModuleBase; o bridge e testado por aqui.
class StubPlugin : public juce::AudioPluginInstance
{
public:
    using Buses = juce::AudioProcessor::BusesProperties;

    explicit StubPlugin (const Buses& buses)
        : juce::AudioPluginInstance (buses)
    {
    }

    const juce::String getName() const override { return "Stub Plugin"; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return midiIn; }
    bool producesMidi() const override { return false; }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    bool hasEditor() const override { return false; }

    void fillInPluginDescription (juce::PluginDescription& desc) const override
    {
        desc.name = getName();
    }

protected:
    bool midiIn = false;

private:
    // createEditor e private pure virtual no JUCE 9: obrigatorio implementar,
    // mas nunca chamado porque hasEditor devolve false.
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
};

// Efeito: um ganho linear exposto como unico param "gain" (0..1).
class GainStub final : public StubPlugin
{
public:
    GainStub()
        : StubPlugin (Buses()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        auto param = std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "gain", 1 }, "gain", 0.0, 1.0, 1.0f);
        gain = param.get();

        // Call qualified: AudioPluginInstance esconde addParameter por using
        // private, mas no AudioProcessor ele e publico (JUCE 9: cru e dono).
        juce::AudioProcessor::addParameter (param.release());

        setLatencySamples (64);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.applyGain (gain->get());
    }

    float currentGain() const { return gain->get(); }

private:
    juce::AudioParameterFloat* gain = nullptr;
};

// Instrumento: sem bus de entrada; toca um barramento DC quando uma nota esta
// ativa e conta CCs vindos pelo MidiBuffer nativo.
class InstrumentStub final : public StubPlugin
{
public:
    InstrumentStub()
        : StubPlugin (Buses()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        midiIn = true;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        buffer.clear();

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (message.isNoteOn())
                active = true;
            else if (message.isNoteOff())
                active = false;
            else if (message.isController())
                ++ccSeen;
        }

        if (active)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* data = buffer.getWritePointer (ch);

                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    data[i] = 0.25f;
            }
        }
    }

    int controllerCount() const { return ccSeen; }

private:
    bool active = false;
    int ccSeen = 0;
};

// Recusado na adocao: sem saida de audio nao ha como caber na corrente.
class NoOutputStub final : public StubPlugin
{
public:
    NoOutputStub()
        : StubPlugin (Buses()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true))
    {
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.clear();
    }
};

forjarack::RackSnapshot passthroughSnapshot()
{
    return forjarack::RackSnapshot();   // trims 0.5 -> 0 dB; gavetas cegas
}

} // namespace

int main()
{
    using namespace forjarack;

    juce::ScopedJuceInitialiser_GUI juceSetup;

    // Espelha o registro de producao: ficha HOSPEDADO no catalogo, fabrica nula.
    if (ModuleCatalog::indexOf ("hospedado") < 0)
    {
        ModuleSpec hosted;
        hosted.id = "hospedado";
        hosted.name = "HOSPEDADO";
        hosted.category = ModuleCategory::hosted;
        hosted.accent = 0xff5ac8c0;
        hosted.role = ModuleRole::effect;
        hosted.numParams = 0;
        hosted.factory = nullptr;
        hosted.face = ModuleFace::generic;
        ModuleCatalog::registerModule (hosted);
    }

    const int hostedIndex = ModuleCatalog::indexOf ("hospedado");
    check (hostedIndex > 0, "catalog exposes the hosted drawer spec");

    constexpr int n = 512;
    constexpr double sr = 48000.0;

    auto inL = makeSine (n, sr, 220.0);
    auto inR = makeSine (n, sr, 220.0);

    // 1) Adocao recusa instancia sem saida de audio.
    {
        HostedModule probe;
        juce::String error;
        const bool ok = probe.adoptInstance (std::make_unique<NoOutputStub>(), error);
        check (! ok && error.isNotEmpty(), "instance without output bus is rejected");
    }

    // 2) Efeito hospedado roda na cadeia, escala o sinal pelo param dinamico e
    // publica pico/latencia pelo engine.
    RackEngine engine;
    engine.prepare (sr, n);

    GainStub* gainRaw = nullptr;
    HostedModule* modRaw = nullptr;

    {
        auto module = std::make_unique<HostedModule>();
        auto instance = std::make_unique<GainStub>();
        gainRaw = instance.get();

        juce::String error;
        check (module->adoptInstance (std::move (instance), error), "effect stub adopted");
        check (! module->isInstrument(), "effect detected via input bus");

        engine.adoptSlotModule (0, hostedIndex, std::move (module));
        modRaw = dynamic_cast<HostedModule*> (engine.moduleAt (0));
        check (modRaw != nullptr, "engine keeps the hosted module alive in the slot");
        check (engine.latencySamples() == 64, "hosted latency propagates through the rack");

        auto snap = passthroughSnapshot();
        snap.slots[0].params[0] = 0.5f;

        auto a = inL;
        auto b = inR;
        engine.process (snap, a.data(), b.data(), n);

        check (allFinite (a) && allFinite (b), "hosted effect keeps signal finite");
        check (approxScale (a, inL, 0.5f) && approxScale (b, inR, 0.5f),
               "rack knob drives the hosted param end to end");
        check (engine.readSlotPeak (0) > 0.0f, "hosted slot publishes an output peak");
    }

    // 3) Mapa dinamico de params e leitura de valor atual do plugin vivo.
    {
        check (modRaw->numHostedParams() == 1, "hosted param count is dynamic");
        check (modRaw->hostedParamLabel (0) == "gain", "hosted param label comes from plugin");
        check (std::abs (modRaw->hostedParamValue (0) - 0.5f) < 1e-6f, "value readback matches");
    }

    // 4) Trava da UI nativa: com o editor do plugin aberto, o rack nao escreve.
    {
        modRaw->setParamsLockedFromHost (true);
        modRaw->setParam (0, 0.9f);
        check (std::abs (gainRaw->currentGain() - 0.5f) < 1e-6f,
               "locked params ignore rack pushes");

        modRaw->setParamsLockedFromHost (false);
        modRaw->setParam (0, 0.25f);
        check (std::abs (gainRaw->currentGain() - 0.25f) < 1e-6f,
               "unlock restores rack control over params");
    }

    // 5) Instrumento hospedado: zero input bus => papel instrumento; o MIDI
    // nativo (void* no core) chega inteiro ao plugin, nota e CC.
    {
        engine.setSlotModule (0, 0);   // solta o efeito anterior
        check (engine.latencySamples() == 0, "removing the hosted module restores zero latency");

        auto module = std::make_unique<HostedModule>();
        auto instance = std::make_unique<InstrumentStub>();
        auto* stubRaw = instance.get();

        juce::String error;
        check (module->adoptInstance (std::move (instance), error), "instrument stub adopted");
        check (module->isInstrument(), "instrument detected via zero input bus");

        engine.adoptSlotModule (0, hostedIndex, std::move (module));

        juce::MidiBuffer native;
        native.addEvent (juce::MidiMessage::noteOn (1, 69, 0.8f), 0);
        native.addEvent (juce::MidiMessage::controllerEvent (1, 7, 100), 1);

        MidiEvent pod { 0, 0, 69, 102, true };
        const MidiEventSpan span { &pod, 1, &native };

        const auto snap = passthroughSnapshot();
        auto a = inL;
        auto b = inR;
        engine.process (snap, a.data(), b.data(), n, span);

        check (allFinite (a) && anyDifferent (a, inL),
               "hosted instrument adds its voice to the chain");
        check (stubRaw->controllerCount() >= 1,
               "native midi buffer reaches the hosted plugin (CC pass-through)");
        check (engine.readSlotPeak (0) > 0.0f, "instrument slot publishes an output peak");
    }

    if (failures == 0)
    {
        std::printf ("forjarack_hosted: PASS\n");
        return 0;
    }

    std::printf ("forjarack_hosted: FAIL (%d)\n", failures);
    return 1;
}
