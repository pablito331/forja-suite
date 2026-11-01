// Teste funcional do RackEngine (ForjaRack). O rack nao tem paridade contra um
// vendor: o que se valida aqui e a cadeia em serie, o roteamento por slot e as
// garantias de seguranca em time-real (pass-through deterministico, sem NaN).
#include <cmath>
#include <cstdio>
#include <vector>

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

bool identical (const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i])
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

forjarack::RackSnapshot passthroughSnapshot()
{
    forjarack::RackSnapshot snap;   // trims 0.5 -> 0 dB; gavetas sem modulo
    return snap;
}

// Devolve o rack as oito gavetas cegas. Como uma gaveta com modulo roda sem
// nenhuma chave ligada, os casos que medem pass-through precisam esvaziar o
// que os testes anteriores carregaram.
void clearRack (forjarack::RackEngine& box)
{
    for (int slot = 0; slot < forjarack::kNumSlots; ++slot)
        box.setSlotModule (slot, 0);
}

// Sonda de expansao: um instrumento falso registrado em runtime, exatamente
// como um leitor sf2/sf3 ou a mini drum entrariam no registro do rack.
class BlinkModule final : public forjarack::ModuleBase
{
public:
    void prepare (double sr) override { sampleRate = sr; }
    void setParam (int, float) override {}
    void setBypass (bool) override {}
    bool isInstrument() const noexcept override { return true; }

    void processMidi (const forjarack::MidiEventSpan& span) noexcept override
    {
        for (int i = 0; i < span.numEvents; ++i)
        {
            const auto& e = span.events[i];

            if (e.noteOn && e.velocity > 0)
            {
                active = true;
                note = e.note;
            }
            else if (! e.noteOn && e.note == note)
            {
                active = false;
            }
        }
    }

    // O engine zera o scratch antes; o instrumento ESCREVE sua voz aqui.
    void process (float* left, float* right, int n) override
    {
        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            double v = 0.0;

            if (active)
            {
                const double freq = 440.0 * std::pow (2.0, (static_cast<double> (note) - 69.0) / 12.0);
                v = 0.25 * std::sin (2.0 * 3.14159265358979 * freq * phase / sampleRate);
                phase += 1.0;
                peak = std::max (peak, static_cast<float> (std::abs (v)));
            }

            left[i] = static_cast<float> (v);
            right[i] = static_cast<float> (v);
        }

        lastPeak = peak;
    }

    float readOutputPeak() override { return lastPeak; }
    int latencySamples() const override { return 0; }

private:
    double sampleRate = 48000.0;
    double phase = 0.0;
    bool active = false;
    std::uint8_t note = 0;
    float lastPeak = 0.0f;
};

std::unique_ptr<forjarack::ModuleBase> createBlink()
{
    return std::make_unique<BlinkModule>();
}

} // namespace

int main()
{
    using namespace forjarack;

    constexpr int n = 512;
    constexpr double sr = 48000.0;

    RackEngine engine;
    engine.prepare (sr, n);

    auto inL = makeSine (n, sr, 220.0);
    auto inR = makeSine (n, sr, 220.0);

    // 1) Rack vazio (sem modulos) e pass-through exato.
    {
        auto a = inL;
        auto b = inR;
        engine.process (passthroughSnapshot(), a.data(), b.data(), n);
        check (identical (a, inL) && identical (b, inR), "empty rack is bit-exact pass-through");
    }

    // 2) Potencia (input trim 0 dB) nao altera; fase inverte so o direito.
    {
        auto snap = passthroughSnapshot();
        snap.phaseInvert = true;

        auto a = inL;
        auto b = inR;
        engine.process (snap, a.data(), b.data(), n);

        bool rightFlipped = true;

        for (int i = 0; i < n; ++i)
            if (std::abs (b[(size_t) i] + inR[(size_t) i]) > 1e-9f)
                rightFlipped = false;

        check (identical (a, inL) && rightFlipped, "phase invert flips right only");
    }

    // 3) Modulo carregado roda sem nenhuma chave ligada, permanece finito e
    // publica pico.
    {
        engine.setSlotModule (0, ModuleCatalog::indexOf ("lixa"));

        auto snap = passthroughSnapshot();
        snap.slots[0].params[0] = 0.7f;   // drive

        auto a = inL;
        auto b = inR;
        engine.process (snap, a.data(), b.data(), n);

        check (allFinite (a) && allFinite (b), "loaded module keeps signal finite");
        check (anyDifferent (a, inL), "loaded LIXA alters the signal");

        const float peak = engine.readSlotPeak (0);
        check (std::isfinite (peak) && peak > 0.0f, "running slot publishes an output peak");
    }

    // 4) Bypass de slot e pass-through exato mesmo com modulo carregado.
    {
        auto snap = passthroughSnapshot();
        snap.slots[0].bypass = true;
        snap.slots[0].params[0] = 0.7f;

        auto a = inL;
        auto b = inR;
        engine.process (snap, a.data(), b.data(), n);
        check (identical (a, inL) && identical (b, inR), "bypassed slot is bit-exact pass-through");
    }

    // 5) Solo nao e uma chave de on/off: solo em gaveta bypassada nao silencia
    // as demais (e sem solo, com tudo em bypass, a cadeia tambem passa intacta).
    {
        engine.setSlotModule (0, ModuleCatalog::indexOf ("lixa"));
        engine.setSlotModule (1, ModuleCatalog::indexOf ("lixa"));

        auto allBypassed = passthroughSnapshot();
        allBypassed.slots[0].bypass = true;
        allBypassed.slots[0].solo = true;
        allBypassed.slots[1].bypass = true;

        auto a = inL;
        auto b = inR;
        engine.process (allBypassed, a.data(), b.data(), n);
        check (identical (a, inL) && identical (b, inR),
               "bypass on every slot is bit-exact pass-through");

        auto soloOnDeadSlot = passthroughSnapshot();
        soloOnDeadSlot.slots[0].bypass = true;
        soloOnDeadSlot.slots[0].solo = true;
        soloOnDeadSlot.slots[1].params[0] = 0.7f;

        auto c = inL;
        auto d = inR;
        engine.process (soloOnDeadSlot, c.data(), d.data(), n);
        check (anyDifferent (c, inL),
               "solo on a bypassed slot does not gate the live slot");
    }

    // 6) Solo: com solo numa gaveta, as demais viram pass-through.
    {
        engine.setSlotModule (1, ModuleCatalog::indexOf ("mel"));

        auto snap = passthroughSnapshot();
        snap.slots[0].params[0] = 0.7f;   // lixa, sem solo
        snap.slots[1].solo = true;        // mel, solo

        auto a = inL;
        auto b = inR;
        engine.process (snap, a.data(), b.data(), n);

        // O mel em solo pode ser near-unity com sweeten=0; o ponto aqui e que a
        // LIXA (sem solo) ficou de fora. Comparando com todos rodando (sem solo),
        // o resultado deve ser diferente.
        auto noSolo = passthroughSnapshot();
        noSolo.slots[0].params[0] = 0.7f;

        auto c = inL;
        auto d = inR;
        engine.process (noSolo, c.data(), d.data(), n);

        check (anyDifferent (a, c), "solo gates the non-solo module out of the chain");
    }

    // 7) Isolamento por instancia: dois racks identicos, mesmos inputs, mesmos
    // outputs (sem estado global compartido nem RNG no caminho).
    {
        RackEngine alpha;
        RackEngine beta;
        alpha.prepare (sr, n);
        beta.prepare (sr, n);
        alpha.setSlotModule (0, ModuleCatalog::indexOf ("lixa"));
        beta.setSlotModule (0, ModuleCatalog::indexOf ("lixa"));

        auto snap = passthroughSnapshot();
        snap.slots[0].params[0] = 0.5f;

        auto a = inL;
        auto b = inR;
        auto c = inL;
        auto d = inR;

        alpha.process (snap, a.data(), b.data(), n);
        beta.process (snap, c.data(), d.data(), n);

        check (identical (a, c) && identical (b, d), "two identically-configured racks match (no shared state)");
    }

    // 8) Trocar o modulo de um slot limpa o estado anterior (pass-through no vazio).
    {
        clearRack (engine);
        engine.setSlotModule (0, 0);   // 0 = vazio no registro

        auto snap = passthroughSnapshot();
        snap.slots[0].params[0] = 0.9f;

        auto a = inL;
        auto b = inR;
        engine.process (snap, a.data(), b.data(), n);
        check (identical (a, inL), "removing the module restores pass-through");
    }

    // 9) Expansao instrument: registrado em runtime, soma voz na cadeia e
    // recebe MIDI; sem nota, a cadeia passa intacta (soma de silencio).
    {
        const bool fresh = ModuleCatalog::registerModule (
            { "test-blink", "BLINK", ModuleCategory::instrument, 0xffe06fa8u,
              ModuleRole::instrument, 0, { { } }, createBlink });

        check (fresh, "registerModule accepts a new expansion id");
        check (! ModuleCatalog::registerModule (
                   { "test-blink", "BLINK2", ModuleCategory::instrument, 0u,
                     ModuleRole::instrument, 0, { { } }, createBlink }),
               "registerModule rejects a duplicated id");

        RackEngine box;
        box.prepare (sr, n);
        box.setSlotModule (0, ModuleCatalog::indexOf ("test-blink"));

        auto snap = passthroughSnapshot();

        // Sem nota: saida identica ao input (o instrumento escreve silencio).
        {
            auto a = inL;
            auto b = inR;
            box.process (snap, a.data(), b.data(), n);
            check (identical (a, inL) && identical (b, inR),
                   "idle instrument keeps the chain bit-exact");
        }

        // Com nota on: o engine soma a voz e a cadeia original permanece la.
        {
            MidiEvent ev { 0, 1, 69, 100, true };
            const MidiEventSpan span { &ev, 1 };

            auto a = inL;
            auto b = inR;
            box.process (snap, a.data(), b.data(), n, span);

            check (allFinite (a) && anyDifferent (a, inL),
                   "played instrument adds its voice to the chain");
            const float instrPeak = box.readSlotPeak (0);   // exchange: le uma vez
            check (std::isfinite (instrPeak) && instrPeak > 0.0f,
                   "instrument slot publishes an output peak");
        }

        // Nota off no bloco seguinte: voz cessa; cadeia volta a passar.
        {
            MidiEvent ev { 0, 1, 69, 0, false };
            const MidiEventSpan span { &ev, 1 };

            auto a = inL;
            auto b = inR;
            box.process (snap, a.data(), b.data(), n, span);   // ultimo som com release
            a = inL; b = inR;
            box.process (snap, a.data(), b.data(), n);
            check (identical (a, inL), "note off restores silence in the instrument slot");
        }
    }

    // 10) Indice fora do registro = pass-through (o engine nao cria modulo).
    {
        clearRack (engine);
        engine.setSlotModule (2, ModuleCatalog::count() + 10);

        auto snap = passthroughSnapshot();   // nenhuma chave ligada

        auto a = inL;
        auto b = inR;
        engine.process (snap, a.data(), b.data(), n);
        check (identical (a, inL) && engine.slotModuleIndex (2) == 0,
               "unknown module index falls back to pass-through");
    }

    // 11) Fumaca do catalogo: todo modulo publicado instancia, prepara, roda a
    // cadeia com seus defaults documentados, devolve amostra finita e publica
    // pico. Pegou furo de indice no wrapper (setParam) ou estado nao iniciado.
    {
        clearRack (engine);

        bool allClean = true;

        for (int index = 1; index < ModuleCatalog::count(); ++index)
        {
            engine.setSlotModule (0, index);

            auto snap = passthroughSnapshot();
            snap.slots[0].moduleIndex = index;
            snap.slots[0].params = ModuleCatalog::defaultParams (index);

            auto a = inL;
            auto b = inR;
            engine.process (snap, a.data(), b.data(), n);

            const float peak = engine.readSlotPeak (0);   // exchange: le uma vez

            if (! allFinite (a) || ! allFinite (b) || ! std::isfinite (peak))
            {
                std::printf ("  FAIL: catalogo indice %d (%s)\n", index,
                             ModuleCatalog::specByIndex (index).name);
                allClean = false;
            }

            engine.setSlotModule (0, 0);
        }

        check (allClean, "every catalog module runs its documented defaults clean");
    }

    if (failures == 0)
    {
        std::printf ("forjarack_engine: PASS\n");
        return 0;
    }

    std::printf ("forjarack_engine: %d FAIL\n", failures);
    return 1;
}
