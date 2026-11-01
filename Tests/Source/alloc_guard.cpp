#include "forja/dsp/ForjaBigorna.h"
#include "forja/dsp/ForjaBrasa.h"
#include "forja/dsp/ForjaCadinho.h"
#include "forja/dsp/ForjaCera.h"
#include "forja/dsp/ForjaChime.h"
#include "forja/dsp/ForjaCinzel.h"
#include "forja/dsp/ForjaCorpo.h"
#include "forja/dsp/ForjaCrick.h"
#include "forja/dsp/ForjaDeluxe.h"
#include "forja/dsp/ForjaDique.h"
#include "forja/dsp/ForjaEco.h"
#include "forja/dsp/ForjaEscoria.h"
#include "forja/dsp/ForjaGabinete.h"
#include "forja/dsp/ForjaGranalha.h"
#include "forja/dsp/ForjaImpeto.h"
#include "forja/dsp/ForjaLaminador.h"
#include "forja/dsp/ForjaLima.h"
#include "forja/dsp/ForjaLixa.h"
#include "forja/dsp/ForjaLobo.h"
#include "forja/dsp/ForjaMalho.h"
#include "forja/dsp/ForjaMel.h"
#include "forja/dsp/ForjaMuralha.h"
#include "forja/dsp/ForjaOxido.h"
#include "forja/dsp/ForjaPonta.h"
#include "forja/dsp/ForjaPrensa.h"
#include "forja/dsp/ForjaRebarba.h"
#include "forja/dsp/ForjaRepique.h"
#include "forja/dsp/ForjaFolego.h"
#include "forja/dsp/ForjaSalao.h"
#include "forja/dsp/ForjaTempera.h"
#include "forja/dsp/ForjaTorno.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

#if defined(_MSC_VER)
 #include <malloc.h>
#endif

namespace
{
std::atomic<bool> active { false };
std::atomic<bool> tripped { false };

void noteAllocation() noexcept
{
    if (active.load (std::memory_order_relaxed))
        tripped.store (true, std::memory_order_relaxed);
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

namespace
{

template <typename Dsp>
bool checkNoAllocations (const char* name, Dsp& dsp)
{
    constexpr int frames = 1024;
    std::vector<float> left ((std::size_t) frames, 0.0f);
    std::vector<float> right ((std::size_t) frames, 0.0f);

    for (int i = 0; i < frames; ++i)
    {
        left[(std::size_t) i] = 0.25f * (float) std::sin (0.03 * (double) i);
        right[(std::size_t) i] = left[(std::size_t) i];
    }

    dsp.prepare (48000.0);
    dsp.process (left.data(), right.data(), frames);

    tripped.store (false, std::memory_order_relaxed);
    active.store (true, std::memory_order_relaxed);
    dsp.process (left.data(), right.data(), frames);
    active.store (false, std::memory_order_relaxed);

    if (tripped.load (std::memory_order_relaxed))
    {
        std::printf ("FAIL %s allocated inside process()\n", name);
        return false;
    }

    return true;
}

} // namespace

int main()
{
    forja::dsp::TemperaDSP tempera;
    forja::dsp::PrensaDSP prensa;
    forja::dsp::MalhoDSP malho;
    forja::dsp::CinzelDSP cinzel;
    forja::dsp::TornoDSP torno;
    forja::dsp::LimaDSP lima;
    forja::dsp::EscoriaDSP escoria;
    forja::dsp::BigornaDSP bigorna;
    forja::dsp::CadinhoDSP cadinho;
    forja::dsp::LaminadorDSP laminador;
    forja::dsp::OxidoDSP oxido;
    forja::dsp::RepiqueDSP repique;
    forja::dsp::EcoDSP eco;
    forja::dsp::SalaoDSP salao;
    forja::dsp::FolegoDSP folego;
    forja::dsp::GranalhaDSP granalha;
    forja::dsp::LixaDSP lixa;
    forja::dsp::MelDSP mel;
    forja::dsp::CeraDSP cera;
    forja::dsp::CorpoDSP corpo;
    forja::dsp::ImpetoDSP impeto;
    forja::dsp::RebarbaDSP rebarba;
    forja::dsp::GabineteDSP gabinete;
    forja::dsp::PontaDSP ponta;
    forja::dsp::ChimeDSP chime;
    forja::dsp::CrickDSP crick;
    forja::dsp::LoboDSP lobo;
    forja::dsp::DeluxeDSP deluxe;
    forja::dsp::MuralhaDSP muralha;
    forja::dsp::DiqueDSP dique;

    bool ok = true;
    ok = checkNoAllocations ("Tempera", tempera) && ok;
    ok = checkNoAllocations ("Prensa", prensa) && ok;
    ok = checkNoAllocations ("Malho", malho) && ok;
    ok = checkNoAllocations ("Cinzel", cinzel) && ok;
    ok = checkNoAllocations ("Torno", torno) && ok;
    ok = checkNoAllocations ("Lima", lima) && ok;
    ok = checkNoAllocations ("Escoria", escoria) && ok;
    ok = checkNoAllocations ("Bigorna", bigorna) && ok;
    ok = checkNoAllocations ("Cadinho", cadinho) && ok;
    ok = checkNoAllocations ("Laminador", laminador) && ok;
    ok = checkNoAllocations ("Oxido", oxido) && ok;
    ok = checkNoAllocations ("Repique", repique) && ok;
    ok = checkNoAllocations ("Eco", eco) && ok;
    ok = checkNoAllocations ("Salao", salao) && ok;
    ok = checkNoAllocations ("Folego", folego) && ok;
    ok = checkNoAllocations ("Granalha", granalha) && ok;
    ok = checkNoAllocations ("Lixa", lixa) && ok;
    ok = checkNoAllocations ("Mel", mel) && ok;
    ok = checkNoAllocations ("Cera", cera) && ok;
    ok = checkNoAllocations ("Corpo", corpo) && ok;
    ok = checkNoAllocations ("Impeto", impeto) && ok;
    ok = checkNoAllocations ("Rebarba", rebarba) && ok;
    ok = checkNoAllocations ("Gabinete", gabinete) && ok;
    ok = checkNoAllocations ("Ponta", ponta) && ok;
    ok = checkNoAllocations ("Chime", chime) && ok;
    ok = checkNoAllocations ("Crick", crick) && ok;
    ok = checkNoAllocations ("Lobo", lobo) && ok;
    ok = checkNoAllocations ("Deluxe", deluxe) && ok;
    ok = checkNoAllocations ("Muralha", muralha) && ok;
    ok = checkNoAllocations ("Dique", dique) && ok;

    std::printf (ok ? "Allocation guard: PASS\n" : "Allocation guard: FAIL\n");
    return ok ? 0 : 1;
}
