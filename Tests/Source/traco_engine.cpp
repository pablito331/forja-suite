// Testes do engine TRACO (DSP Forja proprio, sem paridade MIT): resposta por
// tipo de secao (bell/shelf/HP/LP/notch medida por tom puro), inclinacoes
// 12/24 dB, ordem e cascata, banda desligada = passa-tudo, curva analitica
// consistente com a resposta medida, LISTEN como diferenca, latencia zero por
// impulso, rampa de coeficientes sem artefato, de-esser (sibilo atenuado,
// tom fora da banda intocado, LISTEN, range monotonicamente crescente,
// medidor de reducao) e ausencia de alocacao no caminho de audio.
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include "engine/TracoEngine.h"

namespace
{

using Params = forja::traco::TracoEngine::Params;
using Band = forja::traco::TracoEngine::BandParams;

constexpr double testSampleRate = 48000.0;
constexpr int blockSize = 256;

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

std::atomic<bool> trackAllocations { false };
std::atomic<bool> allocationTripped { false };

void noteAllocation() noexcept
{
    if (trackAllocations.load (std::memory_order_relaxed))
        allocationTripped.store (true, std::memory_order_relaxed);
}

double peakDbOf (const std::vector<float>& v, int from, int to)
{
    float peak = 0.0f;
    for (int i = from; i < to && i < static_cast<int> (v.size()); ++i)
        peak = std::max (peak, std::fabs (v[static_cast<std::size_t> (i)]));
    return 20.0 * std::log10 (static_cast<double> (peak) + 1e-12);
}

double rmsDbOf (const std::vector<float>& v, int from, int to)
{
    double sum = 0.0;
    const int n = std::min (to, static_cast<int> (v.size())) - from;
    for (int i = from; i < from + n; ++i)
    {
        const double s = v[static_cast<std::size_t> (i)];
        sum += s * s;
    }
    return 10.0 * std::log10 (sum / std::max (1, n) + 1e-12);
}

// Nivel de um tom especifico na janela, por correlacao com a exponencial
// complexa. Imune ao alias de pico (tons em fs/6 amostram em |sin| = 0,866)
// e ao alinhamento de fase da janela: extrai a amplitude exata daquela
// frequencia mesmo quando o material tem outros tons.
double toneDbOf (const std::vector<float>& v, int from, int to, double freq)
{
    const double w = 2.0 * forja::traco::kPi * freq / testSampleRate;
    double ci = 0.0, cs = 0.0;
    for (int i = from; i < to; ++i)
    {
        const double s = v[static_cast<std::size_t> (i)];
        ci += s * std::cos (w * static_cast<double> (i));
        cs += s * std::sin (w * static_cast<double> (i));
    }
    const double magnitude = 2.0 * std::sqrt (ci * ci + cs * cs)
                           / static_cast<double> (std::max (1, to - from));
    return 20.0 * std::log10 (magnitude + 1e-12);
}

// Resposta em regime permanente a um tom: senoide de amplitude 0.5. A janela
// de medida fecha 24 ciclos exatos da frequencia (o termo de dupla frequencia
// da correlacao se anula por inteiro) depois de 2048 amostras de assentamento,
// que ficam alem da rampa de coeficientes de 480.
double toneGainDb (const Params& p, double typeFreq)
{
    constexpr double amp = 0.5;
    const int win = std::max (96, static_cast<int> (std::lround (24.0 * testSampleRate / typeFreq)));
    const int total = 2048 + win;

    forja::traco::TracoEngine e;
    e.prepare (testSampleRate);
    e.setParams (p);

    std::vector<float> buf (static_cast<std::size_t> (total));
    const double w = 2.0 * forja::traco::kPi * typeFreq / testSampleRate;
    for (int i = 0; i < total; ++i)
        buf[static_cast<std::size_t> (i)] =
            static_cast<float> (amp * std::sin (w * static_cast<double> (i)));

    for (int off = 0; off < total; off += blockSize)
        e.process (buf.data() + off, nullptr, std::min (blockSize, total - off));

    return toneDbOf (buf, total - win, total, typeFreq)
         - 20.0 * std::log10 (amp);
}

Params withBand (int index, int type, double freq, double gain, double q,
                 bool listen = false)
{
    Params p {};
    auto& b = p.bands[static_cast<std::size_t> (index)];
    b = Band { true, type, freq, gain, q, listen };
    return p;
}

std::vector<float> makeVoice (double seconds, bool withBurst)
{
    // Tom em 1 kHz o tempo todo; rajada sibilante em 7 kHz entre 0,8 s e
    // 1,2 s (meio do arquivo de 2 s).
    const int total = static_cast<int> (seconds * testSampleRate);
    std::vector<float> v (static_cast<std::size_t> (total), 0.0f);
    const double w1 = 2.0 * forja::traco::kPi * 1000.0 / testSampleRate;
    const double w2 = 2.0 * forja::traco::kPi * 7000.0 / testSampleRate;
    const int burstA = static_cast<int> (0.8 * testSampleRate);
    const int burstB = static_cast<int> (1.2 * testSampleRate);

    for (int i = 0; i < total; ++i)
    {
        double s = 0.4 * std::sin (w1 * static_cast<double> (i));
        if (withBurst && i >= burstA && i < burstB)
            s += 0.45 * std::sin (w2 * static_cast<double> (i));
        v[static_cast<std::size_t> (i)] = static_cast<float> (s);
    }
    return v;
}

struct EssRun
{
    std::vector<float> out;
    double meterPeakDb = 0.0;
};

EssRun runEss (const Params& p, const std::vector<float>& in)
{
    forja::traco::TracoEngine e;
    e.prepare (testSampleRate);
    e.setParams (p);

    EssRun run;
    run.out = in;
    const int total = static_cast<int> (in.size());
    for (int off = 0; off < total; off += blockSize)
    {
        e.process (run.out.data() + off, nullptr, std::min (blockSize, total - off));
        run.meterPeakDb = std::max (run.meterPeakDb, e.gainReductionDb());
    }
    return run;
}

Params essParams (bool on, double rangeDb, bool listen = false, double widthQ = 0.5)
{
    Params p {};
    p.essOn = on;
    p.essFreq = 7000.0;
    p.essWidthQ = widthQ;
    p.essRangeDb = rangeDb;
    p.essListen = listen;
    return p;
}

void testPassThrough()
{
    std::printf ("\npassa-tudo e ordem\n");

    Params p {};
    std::vector<float> dry (600), wet (600);
    unsigned int seed = 12345;
    for (int i = 0; i < 600; ++i)
    {
        seed = seed * 1103515245u + 12345u;
        const float v = static_cast<float> ((static_cast<int> (seed >> 16) % 20000) / 20000.0 - 1.0);
        dry[static_cast<std::size_t> (i)] = v;
        wet[static_cast<std::size_t> (i)] = v;
    }

    forja::traco::TracoEngine e;
    e.prepare (testSampleRate);
    e.setParams (p);
    e.process (wet.data(), nullptr, 600);

    bool identical = true;
    for (int i = 0; i < 600; ++i)
        if (wet[static_cast<std::size_t> (i)] != dry[static_cast<std::size_t> (i)])
            identical = false;
    check (identical, "tudo desligado = bit a bit", "sinal mudou com todas as secoes off");

    // Duas bandas ligadas ao mesmo tempo: a ordem do produto importa; as
    // curvas analiticas devem bater com a cascata medida por tom.
    const auto bell = withBand (0, forja::traco::bell, 500.0, 6.0, 1.0);
    const auto shelf = withBand (1, forja::traco::lowShelf, 2000.0, -4.0, 0.707);
    Params both {};
    both.bands[0] = bell.bands[0];
    both.bands[1] = shelf.bands[1];

    const double at1k = toneGainDb (both, 1000.0);
    const double expect = toneGainDb (bell, 1000.0) + toneGainDb (shelf, 1000.0);
    check (std::fabs (at1k - expect) < 0.7, "cascata soma as respostas",
           "medido difere da soma das secoes");
}

void testBandTypes()
{
    std::printf ("\ntipos de banda (RBJ)\n");

    {
        char info[96];
        const auto p = withBand (0, forja::traco::bell, 1000.0, 6.0, 1.0);
        const double at = toneGainDb (p, 1000.0);
        std::snprintf (info, sizeof (info), "bell +6 em 1k medido %.2f dB", at);
        check (std::fabs (at - 6.0) <= 0.5, "campainha no pico", info);
        const double far = std::max (toneGainDb (p, 100.0), toneGainDb (p, 10000.0));
        check (far <= 1.0, "campainha volta ao unity longe do centro",
               "cauda alta demais");
    }

    {
        char info[96];
        const auto flat = withBand (0, forja::traco::bell, 1000.0, 0.0, 1.0);
        const double worst = std::max (std::fabs (toneGainDb (flat, 100.0)),
                                       std::max (std::fabs (toneGainDb (flat, 1000.0)),
                                                 std::fabs (toneGainDb (flat, 8000.0))));
        std::snprintf (info, sizeof (info), "pior desvio %.3f dB", worst);
        check (worst <= 0.2, "campainha com gain 0 = passa-tudo", info);
    }

    {
        char info[96];
        const auto p = withBand (0, forja::traco::lowShelf, 200.0, 4.0, 0.707);
        const double low = toneGainDb (p, 30.0);
        const double high = toneGainDb (p, 4000.0);
        std::snprintf (info, sizeof (info), "graves %.2f dB, agudos %.2f dB", low, high);
        check (std::fabs (low - 4.0) <= 0.6 && std::fabs (high) <= 0.8,
               "prateleira baixa segura +4 nos graves", info);
    }

    {
        char info[96];
        const auto p = withBand (0, forja::traco::highShelf, 3000.0, -5.0, 0.707);
        const double low = toneGainDb (p, 300.0);
        const double high = toneGainDb (p, 15000.0);
        std::snprintf (info, sizeof (info), "graves %.2f dB, agudos %.2f dB", low, high);
        check (std::fabs (low) <= 0.8 && std::fabs (high + 5.0) <= 0.8,
               "prateleira alta corta -5 nos agudos", info);
    }

    {
        char info[96];
        const auto p = withBand (0, forja::traco::notch, 1000.0, 0.0, 2.0);
        const double at = toneGainDb (p, 1000.0);
        const double side = toneGainDb (p, 300.0);
        std::snprintf (info, sizeof (info), "fenda %.1f dB, lado %.2f dB", at, side);
        check (at <= -40.0 && side >= -2.0, "notch profundo e localizado", info);
    }

    {
        char info[96];
        const auto narrow = withBand (0, forja::traco::bell, 1000.0, 6.0, 4.0);
        const auto wide = withBand (0, forja::traco::bell, 1000.0, 6.0, 0.7);
        const double a = toneGainDb (narrow, 1500.0);
        const double b = toneGainDb (wide, 1500.0);
        std::snprintf (info, sizeof (info), "Q4 %.2f dB vs Q0,7 %.2f dB em 1,5k", a, b);
        check (a < b - 1.0, "Q maior = banda mais estreita", info);
        check (std::fabs (toneGainDb (narrow, 1000.0) - 6.0) <= 0.5
               && std::fabs (toneGainDb (wide, 1000.0) - 6.0) <= 0.5,
               "pico independe do Q", "pico fugiu de +6");
    }
}

void testSlopes()
{
    std::printf ("\nHP/LP globais (12 e 24 dB/oitava)\n");

    auto hpParams = [] (bool on24)
    {
        Params p {};
        p.hpOn = true;
        p.hpFreq = 200.0;
        p.hpSlope24 = on24;
        return p;
    };
    auto lpParams = [] (bool on24)
    {
        Params p {};
        p.lpOn = true;
        p.lpFreq = 8000.0;
        p.lpSlope24 = on24;
        return p;
    };

    {
        char info[96];
        const double at = toneGainDb (hpParams (false), 200.0);
        const double half = toneGainDb (hpParams (false), 100.0);
        std::snprintf (info, sizeof (info), "em fc %.2f dB, em fc/2 %.2f dB", at, half);
        check (std::fabs (at + 3.0) <= 0.6 && half <= -8.5, "HP 12 dB: -3 em fc, slope abaixo", info);
        check (toneGainDb (hpParams (false), 4000.0) >= -0.4, "HP 12 dB: passa acima", "nao abriu");
    }

    {
        char info[96];
        const double at = toneGainDb (hpParams (true), 200.0);
        const double half = toneGainDb (hpParams (true), 100.0);
        std::snprintf (info, sizeof (info), "em fc %.2f dB, em fc/2 %.2f dB", at, half);
        check (std::fabs (at + 6.0) <= 1.0 && half <= -19.0, "HP 24 dB: -6 em fc, dobra o slope", info);
    }

    {
        char info[96];
        const double at = toneGainDb (lpParams (false), 8000.0);
        const double hi = toneGainDb (lpParams (false), 18000.0);
        std::snprintf (info, sizeof (info), "em fc %.2f dB, em 18k %.2f dB", at, hi);
        check (std::fabs (at + 3.0) <= 0.6 && hi <= -12.0, "LP 12 dB: -3 em fc, corte acima", info);
        check (toneGainDb (lpParams (false), 1000.0) >= -0.4, "LP 12 dB: passa abaixo", "nao abriu");
    }

    {
        char info[96];
        const double at = toneGainDb (lpParams (true), 8000.0);
        std::snprintf (info, sizeof (info), "em fc %.2f dB", at);
        check (std::fabs (at + 6.0) <= 1.0, "LP 24 dB: -6 em fc", info);
    }
}

void testCurveAndLatency()
{
    std::printf ("\ncurva analitica, latencia e rampa\n");

    {
        const auto p = withBand (0, forja::traco::bell, 2000.0, 6.0, 1.2);
        forja::traco::TracoEngine e;
        e.prepare (testSampleRate);
        e.setParams (p);
        const double analytic = e.responseDbAt (2000.0);
        const double measured = toneGainDb (p, 2000.0);
        char info[96];
        std::snprintf (info, sizeof (info), "curva %.2f dB vs tom %.2f dB", analytic, measured);
        check (std::fabs (analytic - measured) <= 0.7, "curva do grafico bate com o som", info);

        // Fora do centro a curva e a autoridade do grafico: a saia da campainha
        // em 500 Hz deve bater com o som medido (pega erro de sinal na formula
        // de |H|, que no centro e cego porque num ~ den la).
        const double skirtA = e.responseDbAt (500.0);
        const double skirtM = toneGainDb (p, 500.0);
        std::snprintf (info, sizeof (info), "saia curva %.2f dB vs tom %.2f dB", skirtA, skirtM);
        check (std::fabs (skirtA - skirtM) <= 0.7, "curva bate com o som na saia", info);

        // HP global: a curva em fc de um Butterworth de 2ª ordem e -3.01 dB
        // fixos; o som tem que bater (mesmo com a1/a2 de sinal invertido no
        // Coeffs, a formula precisa convir com o biquad real).
        Params hpP {};
        hpP.hpOn = true;
        hpP.hpFreq = 1000.0;
        hpP.hpSlope24 = true;
        forja::traco::TracoEngine e2;
        e2.prepare (testSampleRate);
        e2.setParams (hpP);
        const double atFc = e2.responseDbAt (1000.0);
        std::snprintf (info, sizeof (info), "HP24 em fc: curva %.2f dB", atFc);
        check (std::fabs (atFc + 6.0) <= 0.5, "curva do HP 24 em fc vale -6 dB", info);
        const double hpTone = toneGainDb (hpP, 1000.0);
        std::snprintf (info, sizeof (info), "curva %.2f vs tom %.2f", atFc, hpTone);
        check (std::fabs (atFc - hpTone) <= 0.7, "curva do HP bate com o som em fc", info);

        Params none {};
        e.setParams (none);
        check (std::fabs (e.responseDbAt (999.0)) < 1e-9, "curva zerada sem secoes", "produto != 0");
    }

    {
        const auto p = withBand (0, forja::traco::lowPass, 1000.0, 0.0, 0.707);
        forja::traco::TracoEngine e;
        e.prepare (testSampleRate);
        e.setParams (p);

        std::vector<float> buf (512, 0.0f);
        buf[64] = 0.9f;
        bool silentBefore = true;
        for (int off = 0; off < 512; off += blockSize)
        {
            const int n = std::min (blockSize, 512 - off);
            if (off == 0)
            {
                e.process (buf.data(), nullptr, n);
                for (int i = 0; i < 64; ++i)
                    if (buf[static_cast<std::size_t> (i)] != 0.0f)
                        silentBefore = false;
                if (buf[64] == 0.0f)
                    silentBefore = false; // amostra do impulso tem que sair no instante 0
            }
            else
            {
                e.process (buf.data() + off, nullptr, n);
            }
        }
        check (silentBefore, "latencia zero medida por impulso", "saiu atrasado ou antes");
        check (forja::traco::TracoEngine::latencySamples() == 0, "engine declara latencia 0", "");
    }

    {
        // Rampa: saltar +0 para +12 dB no meio do bloco nao pode explodir nem
        // deixar degrau; regime novo deve assentar no alvo.
        auto p = withBand (0, forja::traco::bell, 1000.0, 0.0, 1.0);
        forja::traco::TracoEngine e;
        e.prepare (testSampleRate);
        e.setParams (p);

        std::vector<float> buf (2048);
        const double w = 2.0 * forja::traco::kPi * 1000.0 / testSampleRate;
        for (int i = 0; i < 2048; ++i)
            buf[static_cast<std::size_t> (i)] = static_cast<float> (0.5 * std::sin (w * i));

        e.process (buf.data(), nullptr, 512);
        p.bands[0].gainDb = 12.0;
        e.setParams (p);
        e.process (buf.data() + 512, nullptr, 1536);

        bool finite = true;
        float maxAbs = 0.0f;
        for (int i = 512; i < 2048; ++i)
        {
            const float v = buf[static_cast<std::size_t> (i)];
            if (! std::isfinite (v))
                finite = false;
            maxAbs = std::max (maxAbs, std::fabs (v));
        }
        char info[96];
        std::snprintf (info, sizeof (info), "pico apos salto %.2f", static_cast<double> (maxAbs));
        check (finite && maxAbs < 2.0f, "salto de coeficiente nao explode", info);

        const double settled = peakDbOf (buf, 1600, 2048) - 20.0 * std::log10 (0.5);
        std::snprintf (info, sizeof (info), "regime novo %.2f dB", settled);
        check (std::fabs (settled - 12.0) <= 0.8, "rampa assenta no alvo", info);
    }
}

void testListen()
{
    std::printf ("\nlisten (audicao por diferenca)\n");

    const auto p = withBand (0, forja::traco::bell, 1000.0, 6.0, 1.0, true);

    // Fora da banda, a diferenca e ~zero: o LISTEN abafa tudo que a banda
    // nao esta tocando. Em fc, com fase zero, a diferenca de +6 dB vale
    // (2 - 1) = unity: a banda aparece a 0 dB e o resto morre.
    const double outside = toneGainDb (p, 100.0);
    char info[96];
    std::snprintf (info, sizeof (info), "em 100 Hz: %.1f dB", outside);
    check (outside <= -18.0, "listen corta o que a banda nao mexe", info);

    const double inside = toneGainDb (p, 1000.0);
    std::snprintf (info, sizeof (info), "em 1 kHz: %.1f dB", inside);
    check (inside >= -2.0 && inside <= 1.5, "listen mostra a acao da banda", info);
    check (inside > outside + 15.0, "diferenca audivel dentro vs fora da banda", info);
}

void testDeEsser()
{
    std::printf ("\nde-esser\n");

    const auto voice = makeVoice (2.0, true);
    const int burstA = static_cast<int> (0.95 * testSampleRate);
    const int burstB = static_cast<int> (1.15 * testSampleRate);
    const int calmA = static_cast<int> (0.35 * testSampleRate);
    const int calmB = static_cast<int> (0.55 * testSampleRate);
    // Janela do ataque: entre 30 e 120 ms depois do inicio da rajada, quando
    // o envelope lento ainda nao comeu o excesso. A medida e so nos 7 kHz
    // (correlacao) para o tom de 1 kHz nao diluir o resultado.
    const int buzzA = static_cast<int> (0.83 * testSampleRate);
    const int buzzB = static_cast<int> (0.92 * testSampleRate);

    const EssRun off = runEss (essParams (false, 12.0), voice);
    const EssRun on = runEss (essParams (true, 12.0), voice);

    const double burstIn = toneDbOf (off.out, buzzA, buzzB, 7000.0);
    const double burstOut = toneDbOf (on.out, buzzA, buzzB, 7000.0);
    const double calmIn = rmsDbOf (off.out, calmA, calmB);
    const double calmOut = rmsDbOf (on.out, calmA, calmB);

    char info[128];
    std::snprintf (info, sizeof (info), "7k na rajada: %.2f -> %.2f dB; calmo: %.2f -> %.2f dB",
                   burstIn, burstOut, calmIn, calmOut);
    check (burstOut <= burstIn - 6.0, "sibilo em 7k e atenuado", info);
    check (std::fabs (calmOut - calmIn) <= 0.7, "tom fora da banda intocado", info);

    check (on.meterPeakDb >= 1.0 && off.meterPeakDb < 0.01,
           "medidor de reducao so anda com o esser ligado",
           "GR do medidor inesperada");

    const EssRun zeroRange = runEss (essParams (true, 0.0), voice);
    std::snprintf (info, sizeof (info), "range 0: %.2f -> %.2f dB", burstIn,
                   toneDbOf (zeroRange.out, buzzA, buzzB, 7000.0));
    check (std::fabs (toneDbOf (zeroRange.out, buzzA, buzzB, 7000.0) - burstIn) <= 1.0,
           "range 0 = nada cortado", info);

    const EssRun soft = runEss (essParams (true, 6.0), voice);
    const double softCut = burstIn - toneDbOf (soft.out, buzzA, buzzB, 7000.0);
    const double hardCut = burstIn - burstOut;
    std::snprintf (info, sizeof (info), "range 6 corta %.2f dB, range 12 corta %.2f dB",
                   softCut, hardCut);
    check (hardCut > softCut + 2.0 && softCut >= 2.0, "range maior corta mais", info);

    // LISTEN do esser: so a banda tratada sai. Com split estreito (Q 2) o
    // tom de 1k desaparece; a rajada em 7k continua (reduzida pelo GR).
    const EssRun listen = runEss (essParams (true, 12.0, true, 2.0), voice);
    const double calmListen = rmsDbOf (listen.out, calmA, calmB);
    const double burstListen = rmsDbOf (listen.out, burstA, burstB);
    std::snprintf (info, sizeof (info), "listen: calmo %.1f dB, rajada %.1f dB",
                   calmListen, burstListen);
    check (calmListen <= calmIn - 15.0 && burstListen >= burstIn - 20.0,
           "listen so deixa a banda do esser", info);
}

void testAllocationAndState()
{
    std::printf ("\nseguranca de RT e estado\n");

    auto p = withBand (0, forja::traco::bell, 1000.0, 6.0, 1.0);
    p.hpOn = true;
    p.essOn = true;

    forja::traco::TracoEngine e;
    e.prepare (testSampleRate);
    e.setParams (p);

    std::array<float, static_cast<std::size_t> (blockSize)> l {};
    std::array<float, static_cast<std::size_t> (blockSize)> r {};
    for (int i = 0; i < blockSize; ++i)
    {
        l[static_cast<std::size_t> (i)] = static_cast<float> (std::sin (0.1 * i));
        r[static_cast<std::size_t> (i)] = static_cast<float> (std::cos (0.07 * i));
    }

    allocationTripped.store (false, std::memory_order_relaxed);
    trackAllocations.store (true, std::memory_order_relaxed);
    for (int n = 0; n < 400; ++n)
    {
        p.bands[0].freq = 400.0 + 800.0 * (static_cast<double> (n % 8) / 8.0);
        p.essRangeDb = 3.0 * static_cast<double> (n % 5);
        e.setParams (p);
        e.process (l.data(), r.data(), blockSize);
        e.responseDbAt (1000.0);
        e.sectionDbAt (4, 500.0);
    }
    trackAllocations.store (false, std::memory_order_relaxed);
    check (! allocationTripped.load (std::memory_order_relaxed),
           "zero alocacao em process/setParams/curva", "alocou no caminho de audio");

    // prepare() zera o estado: rodar o mesmo material duas vezes do zero
    // deve dar saida identica amostra a amostra.
    std::vector<float> first (1024), second (1024);
    const double w = 2.0 * forja::traco::kPi * 3333.0 / testSampleRate;
    for (int i = 0; i < 1024; ++i)
        first[static_cast<std::size_t> (i)] = second[static_cast<std::size_t> (i)] =
            static_cast<float> (0.6 * std::sin (w * i));

    e.prepare (testSampleRate);
    e.setParams (p);
    for (int off = 0; off < 1024; off += blockSize)
        e.process (first.data() + off, nullptr, std::min (blockSize, 1024 - off));

    e.prepare (testSampleRate);
    e.setParams (p);
    for (int off = 0; off < 1024; off += blockSize)
        e.process (second.data() + off, nullptr, std::min (blockSize, 1024 - off));

    bool same = true;
    for (int i = 0; i < 1024; ++i)
        if (first[static_cast<std::size_t> (i)] != second[static_cast<std::size_t> (i)])
            same = false;
    check (same, "prepare zera estado (determinismo)", "saida divergiu apos re-prepare");
}

} // namespace

void* operator new (std::size_t size)
{
    noteAllocation();
    void* p = std::malloc (size == 0 ? 1 : size);
    if (p == nullptr)
        throw std::bad_alloc();
    return p;
}

void operator delete (void* p) noexcept
{
    std::free (p);
}

void operator delete (void* p, std::size_t) noexcept
{
    std::free (p);
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void operator delete[] (void* p) noexcept
{
    std::free (p);
}

void operator delete[] (void* p, std::size_t) noexcept
{
    std::free (p);
}

int main()
{
    std::printf ("== TRACO engine ==\n");

    testPassThrough();
    testBandTypes();
    testSlopes();
    testCurveAndLatency();
    testListen();
    testDeEsser();
    testAllocationAndState();

    std::printf ("\n%s (%d falhas)\n", failures == 0 ? "TRACO: PASS" : "TRACO: FAIL", failures);
    return failures == 0 ? 0 : 1;
}
