// Testes do wrapper MiniSat: o mapeamento documentado do knob DRIVE sobre o
// pad de entrada do Tube2, identidade amostra a amostra com o adaptador bruto
// forja::BrasaDSP, proxy de timbre (clamp cresce com o drive), bypass com
// rampa de 10 ms, latencia zero medida por impulso, ausencia de alocacao no
// caminho de audio (esterio e mono) e a gaveta MINISAT no ModuleCatalog.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include "minisat/MiniSat.h"
#include "minisat/MiniSatRackModule.h"
#include "rack/ModuleCatalog.h"

#if defined(_MSC_VER)
 #include <malloc.h>
#endif

namespace
{

constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 256;

std::atomic<bool> trackAllocations { false };
std::atomic<bool> allocationTripped { false };

void noteAllocation() noexcept
{
    if (trackAllocations.load (std::memory_order_relaxed))
        allocationTripped.store (true, std::memory_order_relaxed);
}

void* allocateAligned (std::size_t size, std::size_t alignment) noexcept
{
#if defined(_MSC_VER)
    return _aligned_malloc (size == 0 ? 1 : size, alignment);
#else
    return std::aligned_alloc (alignment, size == 0 ? 1 : size);
#endif
}

void freeAligned (void* pointer) noexcept
{
#if defined(_MSC_VER)
    _aligned_free (pointer);
#else
    std::free (pointer);
#endif
}

int failures = 0;

void check (bool condition, const char* name, const char* info = "")
{
    if (condition)
    {
        std::printf ("  PASS  %s\n", name);
        return;
    }

    std::printf ("  FAIL  %s - %s\n", name, info);
    ++failures;
}

// Sine em float na amplitude pedida, fase diferenca fixa no canal direito
// (a valvula tem estado por canal; entrada identica L/R escondendo assimetria
// seria teste mais fraco do que parece).
void fillSine (std::vector<float>& left, std::vector<float>& right, double amplitude)
{
    for (int i = 0; i < testBlockSize; ++i)
    {
        const auto phase = 2.0 * M_PI * 440.0 * (double) i / testSampleRate;
        left[(std::size_t) i] = (float) (amplitude * std::sin (phase));
        right[(std::size_t) i] = (float) (amplitude * std::sin (phase + 0.37));
    }
}

// Buffer plano [L | R] de 2*testBlockSize com sine em L/R (fase deslocada no
// direito: a valvula tem estado por canal; entrada identica L/R escondendo
// assimetria seria um teste mais fraco do que parece).
std::vector<float> makeFlatStereo (double amplitude)
{
    std::vector<float> flat ((std::size_t) (2 * testBlockSize));
    std::vector<float> left (flat.begin(), flat.begin() + testBlockSize);
    std::vector<float> right (flat.begin() + testBlockSize, flat.end());
    fillSine (left, right, amplitude);
    std::copy (left.begin(), left.end(), flat.begin());
    std::copy (right.begin(), right.end(), flat.begin() + testBlockSize);
    return flat;
}

// ---------------------------------------------------------------------------

void testDriveMapping()
{
    using forja::minisat::MiniSat;

    const auto zero = MiniSat::parametersForDrive (0.0f, false);
    const auto half = MiniSat::parametersForDrive (0.5f, false);
    const auto full = MiniSat::parametersForDrive (1.0f, false);
    const auto over = MiniSat::parametersForDrive (1.7f, false);
    const auto under = MiniSat::parametersForDrive (-0.4f, false);

    check (std::abs (zero.input - 0.5f) < 1.0e-6f, "drive 0 = pad default do BRASA");
    check (std::abs (full.input - 1.0f) < 1.0e-6f, "drive 1 = sem pad");
    check (std::abs (half.input - 0.75f) < 1.0e-6f, "drive 0.5 = 0.75");
    check (std::abs (over.input - 1.0f) < 1.0e-6f && std::abs (under.input - 0.5f) < 1.0e-6f,
           "drive fora de 0..1 e clampado");
    check (std::abs (zero.tube - 0.5f) < 1.0e-6f && std::abs (full.tube - 0.5f) < 1.0e-6f,
           "valvula fixa no default documentado");
    check (zero.outputDb == 0.0f && full.outputDb == 0.0f, "sem makeup gain (trim 0 dB)");
    check (MiniSat::parametersForDrive (0.2f, true).bypass, "bypass passa para o adaptador");

    float previous = -1.0f;
    bool monotonic = true;
    for (int i = 0; i <= 100; ++i)
    {
        const auto p = MiniSat::parametersForDrive ((float) i / 100.0f, false);
        if (p.input < previous)
            monotonic = false;
        previous = p.input;
    }
    check (monotonic, "mapeamento monotono crescente");
}

void testBitIdentityWithAdapter()
{
    // O wrapper nao pode alterar uma amostra do adaptador: com o mesmo
    // Parameters e mesmo fpd (novos em ambos), a saida deve ser identica bit a
    // bit bloco a bloco.
    for (int driveIndex = 0; driveIndex <= 4; ++driveIndex)
    {
        const float drive = (float) driveIndex / 4.0f;

        forja::minisat::MiniSat sat;
        sat.prepare (testSampleRate, testBlockSize);
        sat.setDrive (drive);

        forja::BrasaDSP raw;
        raw.prepare (testSampleRate);
        raw.setParameters (forja::minisat::MiniSat::parametersForDrive (drive, false));

        auto viaSat = makeFlatStereo (0.9);
        auto viaRaw = viaSat;

        bool identical = true;
        for (int b = 0; b < 10; ++b)
        {
            sat.process (viaSat.data(), viaSat.data() + testBlockSize, testBlockSize, false);
            raw.process (viaRaw.data(), viaRaw.data() + testBlockSize, testBlockSize);

            for (int i = 0; i < 2 * testBlockSize; ++i)
                if (viaSat[(std::size_t) i] != viaRaw[(std::size_t) i])
                    identical = false;
        }

        char name[80];
        std::snprintf (name, sizeof (name), "wrapper identico ao adaptador (drive %.2f)", drive);
        check (identical, name);
    }
}

void testTimbreProxy()
{
    // Proxy de saturacao: crest factor (peak / RMS) de uma senoide em 0.9.
    // Valvula conduindo achata o topo da onda e derruba o crest em direcao a
    // 1,0 (quadrada). O pad de 0,5 do drive 0 entrega 0,45 a valvula e ainda
    // achata pouco; drive 1 chega sem pad e satura de verdade.
    auto crestFor = [] (float drive)
    {
        const auto input = makeFlatStereo (0.9);
        auto flat = input;

        forja::minisat::MiniSat sat;
        sat.prepare (testSampleRate, testBlockSize);
        sat.setDrive (drive);

        // Cada bloco reabastece a senoide (o process e in place) e o estado
        // da valvula assenta; o crest e medido no ultimo bloco ja em regime.
        float* out = flat.data();
        for (int b = 0; b < 64; ++b)
        {
            std::copy (input.begin(), input.end(), flat.begin());
            sat.process (out, out + testBlockSize, testBlockSize, false);
        }

        double peak = 0.0;
        double sumSquares = 0.0;
        for (int i = 0; i < 2 * testBlockSize; ++i)
        {
            const auto value = (double) std::abs (out[i]);
            peak = std::max (peak, value);
            sumSquares += (double) out[i] * (double) out[i];
        }

        const int count = 2 * testBlockSize;
        const double rms = std::sqrt (sumSquares / (double) count);
        return rms > 0.0 ? peak / rms : 0.0;
    };

    const auto soft = crestFor (0.0f);
    const auto hard = crestFor (1.0f);

    char info[96];
    std::snprintf (info, sizeof (info), "crest drive0=%.3f drive1=%.3f", soft, hard);
    check (hard < soft, "saturacao achata o topo (crest cai com o drive)", info);
}

void testBypassRamp()
{
    auto input = makeFlatStereo (0.5);

    forja::minisat::MiniSat sat;
    sat.prepare (testSampleRate, testBlockSize);
    sat.setDrive (1.0f);

    // Deixa a rampa fechar (10 ms = 480 amostras) renderizando em bypass. O
    // process e in place, entao cada bloco reabastece o dry original: sem
    // isso a saido anterior entraria como entrada e a conta nao fechava.
    auto out = input;
    float* samples = out.data();
    for (int b = 0; b < 30; ++b)
    {
        std::copy (input.begin(), input.end(), out.begin());
        sat.process (samples, samples + testBlockSize, testBlockSize, true);
    }

    bool dry = true;
    double worst = 0.0;
    for (int i = 0; i < 2 * testBlockSize; ++i)
    {
        const auto error = (double) std::abs (out[i] - input[i]);
        worst = std::max (worst, error);
        if (error > 1.0e-6f)
            dry = false;
    }

    char dryInfo[96];
    std::snprintf (dryInfo, sizeof (dryInfo), "pior erro %.3e", worst);
    check (dry, "bypass assenta na copia seca do sinal", dryInfo);

    // Saindo do bypass com entrada constante 0.5: a mistura dry->wet anda na
    // rampa de 10 ms (480 amostras em 48 kHz). Contamos quantas amostras o
    // primeiro bloco leva para se descolar de 0.5: tem que espalhar pela
    // rampa inteira, nao num corte subito nem ficar travado no dry.
    std::vector<float> flat ((std::size_t) (2 * testBlockSize), 0.5f);
    sat.process (flat.data(), flat.data() + testBlockSize, testBlockSize, false);

    int movedFrames = 0;
    for (int i = 0; i < testBlockSize; ++i)
        if (std::abs (flat[i] - 0.5f) > 1.0e-4f)
            ++movedFrames;

    char rampInfo[96];
    std::snprintf (rampInfo, sizeof (rampInfo), "amostras que se moveram=%d", movedFrames);
    check (movedFrames > 32 && movedFrames <= testBlockSize,
           "transicao dry->wet anda na rampa", rampInfo);
}

void testZeroLatency()
{
    // Impulso no frame 0: o Tube nao atrasa o bloco, entao a primeira amostra
    // ja contem o shape (mais o dither/DC offset do modelo).
    std::vector<float> left ((std::size_t) testBlockSize, 0.0f);
    std::vector<float> right ((std::size_t) testBlockSize, 0.0f);
    left[0] = 0.9f;
    right[0] = 0.9f;

    forja::minisat::MiniSat sat;
    sat.prepare (testSampleRate, testBlockSize);
    sat.setDrive (0.0f);
    sat.process (left.data(), right.data(), testBlockSize, false);

    check (std::abs (left[0]) > 1.0e-4f, "impulse responde no frame 0 (latencia 0)");
    check (std::abs (left[1]) < std::abs (left[0]), "decaimento comeca no frame seguinte");
}

void testNoAllocations()
{
    std::vector<float> stereo ((std::size_t) (2 * testBlockSize), 0.0f);
    std::vector<float> mono ((std::size_t) testBlockSize, 0.0f);
    for (int i = 0; i < testBlockSize; ++i)
    {
        const auto value = 0.3f * (float) std::sin (0.03 * (double) i);
        stereo[(std::size_t) i] = value;
        stereo[(std::size_t) testBlockSize + i] = value;
        mono[(std::size_t) i] = value;
    }

    forja::minisat::MiniSat sat;
    sat.prepare (testSampleRate, testBlockSize);

    // Warm-up fora da guarda.
    sat.process (stereo.data(), stereo.data() + testBlockSize, testBlockSize, false);
    sat.process (mono.data(), nullptr, testBlockSize, false);

    // Buffer do caso oversize tambem nasce fora da guarda: alocar aqui e da
    // thread de mensagens (prepareToPlay), nao do caminho de audio.
    std::vector<float> big ((std::size_t) forja::minisat::MiniSat::kCapacity * 2, 0.0f);

    allocationTripped.store (false, std::memory_order_relaxed);
    trackAllocations.store (true, std::memory_order_relaxed);
    for (int b = 0; b < 64; ++b)
    {
        sat.setDrive ((float) b / 64.0f);
        sat.process (stereo.data(), stereo.data() + testBlockSize, testBlockSize, false);
        sat.process (mono.data(), nullptr, testBlockSize, false);
        sat.readMeters (true);
    }

    // Bloco mono maior que a capacidade: pass-through, tambem sem alocar.
    sat.process (big.data(), nullptr, (int) big.size(), false);
    trackAllocations.store (false, std::memory_order_relaxed);

    check (! allocationTripped.load (std::memory_order_relaxed),
           "nenhuma alocacao em process() (estereo, mono e oversize)");
}

void testRackModuleAndRegistration()
{
    using namespace forjarack;

    // Idempotente: registrar duas vezes nao duplica a gaveta.
    forja::minisat::registerMinisatModule();
    const int firstIndex = ModuleCatalog::indexOf ("minisat");
    forja::minisat::registerMinisatModule();

    check (firstIndex >= 0, "minisat registrado no catalogo");
    check (ModuleCatalog::indexOf ("minisat") == firstIndex, "registro idempotente");
    check (std::strcmp (ModuleCatalog::specByIndex (firstIndex).id, "minisat") == 0,
           "id estavel 'minisat'");

    const auto& spec = ModuleCatalog::specByIndex (firstIndex);
    check (std::strcmp (spec.name, "MINISAT") == 0, "rotulo MINISAT");
    check (spec.category == ModuleCategory::drive, "categoria drive");
    check (spec.role == ModuleRole::effect, "papel efeito");
    check (spec.face == ModuleFace::valve, "face valve (primeira dona da face)");
    check (spec.numParams == 1 && std::strcmp (spec.knobs[0].label, "DRIVE") == 0
              && spec.knobs[0].defaultValue == 0.0f,
           "knob unico DRIVE com default 0");
    check (! spec.showsGainReduction, "sem medidor de GR inventado");

    auto module = spec.factory ();
    check (module != nullptr && ! module->isInstrument(), "fabrica devolve efeito");

    module->prepare (testSampleRate);
    module->setParam (0, 1.0f);
    module->setBypass (false);

    std::vector<float> left ((std::size_t) testBlockSize);
    std::vector<float> right ((std::size_t) testBlockSize);
    fillSine (left, right, 0.9);
    module->process (left.data(), right.data(), testBlockSize);
    check (module->readOutputPeak() > 0.0f, "modulo do rack processa e mede");
    check (module->latencySamples() == 0, "modulo do rack com latencia 0");

    module->setBypass (true);
    for (int b = 0; b < 30; ++b)
    {
        fillSine (left, right, 0.5);
        module->process (left.data(), right.data(), testBlockSize);
    }
    check (module->readOutputPeak() > 0.01f, "bypass no rack mantem o sinal seco");
}

} // namespace

void* operator new (std::size_t size)
{
    noteAllocation();
    if (void* pointer = std::malloc (size == 0 ? 1 : size))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void operator delete (void* pointer) noexcept
{
    std::free (pointer);
}

void operator delete[] (void* pointer) noexcept
{
    std::free (pointer);
}

void operator delete (void* pointer, std::size_t) noexcept
{
    std::free (pointer);
}

void operator delete[] (void* pointer, std::size_t) noexcept
{
    std::free (pointer);
}

void* operator new (std::size_t size, std::align_val_t alignment)
{
    noteAllocation();
    if (void* pointer = allocateAligned (size, (std::size_t) alignment))
        return pointer;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    return ::operator new (size, alignment);
}

void operator delete (void* pointer, std::align_val_t) noexcept
{
    freeAligned (pointer);
}

void operator delete[] (void* pointer, std::align_val_t) noexcept
{
    freeAligned (pointer);
}

int main()
{
    std::printf ("MiniSat tests\n");

    testDriveMapping();
    testBitIdentityWithAdapter();
    testTimbreProxy();
    testBypassRamp();
    testZeroLatency();
    testNoAllocations();
    testRackModuleAndRegistration();

    if (failures == 0)
    {
        std::printf ("\nPASS: wrapper MiniSat\n");
        return 0;
    }

    std::printf ("\nFAIL: %d verificacao(oes)\n", failures);
    return 1;
}
