#pragma once

// TRACO - engine do EQ grafico Forja. DSP proprio da casa: biquadrios do
// Audio EQ Cookbook de Robert Bristow-Johnson (formulas publicas) em cascata
// HP -> LP -> 5 bandas -> de-esser. Nao e adaptacao Airwindows: nao herda
// codigo da vendor e nao tem teste de paridade MIT; o comportamento e coberto
// por forja_traco. O de-esser usa o conceito ja documentado da casa (detector
// por diferenca filtrada, como o ESCORIA) em implementacao propria: envelope
// rapido contra envelope lento da propria banda.
//
// Invariantes: header-only, sem JUCE, todo estado por instancia (nunca
// static), zero alocao depois de prepare(), coeficientes trocados com rampa
// linear de 10 ms (sem zipper), latencia zero (nada de lookahead).

#include <algorithm>
#include <array>
#include <cmath>

namespace forja::traco
{

enum BandType : int
{
    bell = 0,
    lowShelf = 1,
    highShelf = 2,
    highPass = 3,
    lowPass = 4,
    notch = 5,
    numBandTypes = 6
};

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kButterworthQ = 0.7071067811865476;

inline double clampd (double v, double lo, double hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}

struct Coeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; // ja normalizados por a0

    bool operator== (const Coeffs& o) const noexcept
    {
        return b0 == o.b0 && b1 == o.b1 && b2 == o.b2 && a1 == o.a1 && a2 == o.a2;
    }
};

// Coeficientes do cookbook de RBJ (w3.org/TR/audio-eq-cookbook), com beta na
// forma Q-based para as prateleiras: beta = sqrt(A)/Q.
inline Coeffs coeffsFor (int type, double sampleRate, double freqHz,
                         double gainDb, double q) noexcept
{
    const double f = clampd (freqHz, 5.0, std::min (0.48 * sampleRate, 20000.0));
    const double qq = clampd (q, 0.05, 30.0);
    const double w = 2.0 * kPi * f / sampleRate;
    const double sinW = std::sin (w);
    const double cosW = std::cos (w);
    const double alpha = sinW / (2.0 * qq);

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a0 = 1.0, a1 = 0.0, a2 = 0.0;

    switch (type)
    {
        case lowPass:
            b0 = (1.0 - cosW) / 2.0; b1 = 1.0 - cosW; b2 = (1.0 - cosW) / 2.0;
            a0 = 1.0 + alpha; a1 = -2.0 * cosW; a2 = 1.0 - alpha;
            break;

        case highPass:
            b0 = (1.0 + cosW) / 2.0; b1 = -(1.0 + cosW); b2 = (1.0 + cosW) / 2.0;
            a0 = 1.0 + alpha; a1 = -2.0 * cosW; a2 = 1.0 - alpha;
            break;

        case lowShelf:
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double beta = std::sqrt (A) / qq;
            b0 = A * ((A + 1.0) - (A - 1.0) * cosW + beta * sinW);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW);
            b2 = A * ((A + 1.0) - (A - 1.0) * cosW - beta * sinW);
            a0 = (A + 1.0) + (A - 1.0) * cosW + beta * sinW;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosW);
            a2 = (A + 1.0) + (A - 1.0) * cosW - beta * sinW;
            break;
        }

        case highShelf:
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double beta = std::sqrt (A) / qq;
            b0 = A * ((A + 1.0) + (A - 1.0) * cosW + beta * sinW);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW);
            b2 = A * ((A + 1.0) + (A - 1.0) * cosW - beta * sinW);
            a0 = (A + 1.0) - (A - 1.0) * cosW + beta * sinW;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW);
            a2 = (A + 1.0) - (A - 1.0) * cosW - beta * sinW;
            break;
        }

        case notch:
            b0 = 1.0; b1 = -2.0 * cosW; b2 = 1.0;
            a0 = 1.0 + alpha; a1 = -2.0 * cosW; a2 = 1.0 - alpha;
            break;

        case bell:
        default:
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            b0 = 1.0 + alpha * A; b1 = -2.0 * cosW; b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A; a1 = -2.0 * cosW; a2 = 1.0 - alpha / A;
            break;
        }
    }

    return Coeffs { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

// Banda-passa de ganho unitario no centro (BPF peak gain do cookbook); e o
// split do de-esser.
inline Coeffs coeffsBandpass (double sampleRate, double freqHz, double q) noexcept
{
    const double f = clampd (freqHz, 5.0, std::min (0.48 * sampleRate, 20000.0));
    const double qq = clampd (q, 0.1, 20.0);
    const double w = 2.0 * kPi * f / sampleRate;
    const double sinW = std::sin (w);
    const double cosW = std::cos (w);
    const double alpha = sinW / (2.0 * qq);
    const double a0 = 1.0 + alpha;

    return Coeffs { alpha / a0, 0.0, -alpha / a0, -2.0 * cosW / a0, (1.0 - alpha) / a0 };
}

inline double magnitudeDb (const Coeffs& c, double freqHz, double sampleRate) noexcept
{
    const double w = 2.0 * kPi * clampd (freqHz, 1.0, 0.49 * sampleRate) / sampleRate;
    const double cosW = std::cos (w);
    const double cos2W = std::cos (2.0 * w);

    const double num = c.b0 * c.b0 + c.b1 * c.b1 + c.b2 * c.b2
                     + 2.0 * (c.b0 * c.b1 + c.b1 * c.b2) * cosW
                     + 2.0 * c.b0 * c.b2 * cos2W;
    const double den = 1.0 + c.a1 * c.a1 + c.a2 * c.a2
                     + 2.0 * (c.a1 + c.a1 * c.a2) * cosW
                     + 2.0 * c.a2 * cos2W;

    return 10.0 * std::log10 ((num + 1e-30) / (den + 1e-30));
}

// Biquad Direct Form I com rampa linear de coeficientes: setTarget nao corta
// o som, ele atravessa cur para target em rampSamples amostras.
class Biquad
{
public:
    void setRampSamples (int n) noexcept { rampSamples = std::max (1, n); }

    void snap (const Coeffs& c) noexcept
    {
        cur = target = c;
        delta = Coeffs {};
        stepsLeft = 0;
    }

    void setTarget (const Coeffs& c) noexcept
    {
        if (c == target && stepsLeft == 0)
            return;

        target = c;
        const double n = static_cast<double> (rampSamples);
        delta = Coeffs { (c.b0 - cur.b0) / n, (c.b1 - cur.b1) / n, (c.b2 - cur.b2) / n,
                         (c.a1 - cur.a1) / n, (c.a2 - cur.a2) / n };
        stepsLeft = rampSamples;
    }

    double process (double x) noexcept
    {
        if (stepsLeft > 0)
        {
            cur.b0 += delta.b0; cur.b1 += delta.b1; cur.b2 += delta.b2;
            cur.a1 += delta.a1; cur.a2 += delta.a2;
            if (--stepsLeft == 0)
                cur = target;
        }

        const double y = cur.b0 * x + cur.b1 * x1 + cur.b2 * x2
                       - cur.a1 * y1 - cur.a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }

    void resetState() noexcept { x1 = x2 = y1 = y2 = 0.0; }

    const Coeffs& targetCoeffs() const noexcept { return target; }

private:
    Coeffs cur {}, target {}, delta {};
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    int rampSamples = 480;
    int stepsLeft = 0;
};

class TracoEngine
{
public:
    static constexpr int numBands = 5;

    struct BandParams
    {
        bool on = false;
        int type = bell;
        double freq = 1000.0;
        double gainDb = 0.0;
        double q = 0.71;
        bool listen = false;
    };

    struct Params
    {
        bool hpOn = false;
        double hpFreq = 60.0;
        bool hpSlope24 = false;

        bool lpOn = false;
        double lpFreq = 16000.0;
        bool lpSlope24 = false;

        std::array<BandParams, numBands> bands {};

        bool essOn = false;
        double essFreq = 6500.0;   // centro do split (4..12 kHz na UI)
        double essWidthQ = 0.7;    // largura do split (menor Q = mais largo)
        double essRangeDb = 9.0;   // teto da reducao (0..24 na UI)
        bool essListen = false;
    };

    void prepare (double sampleRateIn) noexcept
    {
        sampleRate = sampleRateIn;
        const int ramp = std::max (1, static_cast<int> (0.01 * sampleRate));

        for (auto& s : { &hp, &lp })
            for (int st = 0; st < 2; ++st)
                for (int ch = 0; ch < 2; ++ch)
                {
                    s->stage[st][ch].setRampSamples (ramp);
                    s->stage[st][ch].resetState();
                }

        for (auto& band : bands)
            for (auto& bq : band.stage)
            {
                bq.setRampSamples (ramp);
                bq.resetState();
            }

        for (auto& bq : essSplit)
        {
            bq.setRampSamples (ramp);
            bq.resetState();
        }

        essFast = {}; essSlow = {};
        essGrDb = 0.0;
        blockGrPeak = 0.0;

        // Constantes do detector (documentadas no README e no FUNDAMENTOS):
        // envelope rapido 2 ms / 40 ms, lento 400 ms, follow da reducao
        // 5 ms / 120 ms. Ataque mais rapido que release para segurar s.
        const auto tau = [this] (double seconds) noexcept
        {
            return std::exp (-1.0 / (seconds * sampleRate));
        };
        kFastAt = tau (0.002); kFastRel = tau (0.04);
        kSlow   = tau (0.4);
        kGrAt   = tau (0.005); kGrRel = tau (0.12);

        rebuildAll();
    }

    void setParams (const Params& p) noexcept
    {
        if (p.hpOn != par.hpOn || p.hpFreq != par.hpFreq || p.hpSlope24 != par.hpSlope24)
        {
            par.hpOn = p.hpOn; par.hpFreq = p.hpFreq; par.hpSlope24 = p.hpSlope24;
            rebuildSlope (hp, highPass, p.hpFreq, p.hpSlope24);
        }
        if (p.lpOn != par.lpOn || p.lpFreq != par.lpFreq || p.lpSlope24 != par.lpSlope24)
        {
            par.lpOn = p.lpOn; par.lpFreq = p.lpFreq; par.lpSlope24 = p.lpSlope24;
            rebuildSlope (lp, lowPass, p.lpFreq, p.lpSlope24);
        }

        for (int i = 0; i < numBands; ++i)
        {
            const auto& np = p.bands[i];
            auto& op = par.bands[i];
            if (np.type != op.type || np.freq != op.freq
                || np.gainDb != op.gainDb || np.q != op.q)
            {
                op = np;
                bands[i].stage[0].setTarget (
                    coeffsFor (np.type, sampleRate, np.freq, np.gainDb, np.q));
            }
            op.on = np.on;
            op.listen = np.listen;
        }

        if (p.essFreq != par.essFreq || p.essWidthQ != par.essWidthQ)
        {
            par.essFreq = p.essFreq;
            par.essWidthQ = p.essWidthQ;
            const auto c = coeffsBandpass (sampleRate, p.essFreq, p.essWidthQ);
            essSplit[0].setTarget (c);
            essSplit[1].setTarget (c);
        }
        par.essOn = p.essOn;
        par.essRangeDb = p.essRangeDb;
        par.essListen = p.essListen;

        soloBand = -1;
        for (int i = 0; i < numBands; ++i)
            if (par.bands[i].on && par.bands[i].listen)
            {
                soloBand = i;
                break;
            }
    }

    const Params& params() const noexcept { return par; }

    void process (float* left, float* right, int numSamples) noexcept
    {
        blockGrPeak = 0.0;
        const float* mono = right == nullptr ? left : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float yL = 0.0f, yR = 0.0f;
            processFrame (static_cast<double> (left[i]),
                          static_cast<double> (mono != nullptr ? left[i] : right[i]),
                          yL, yR);
            left[i] = yL;
            if (mono == nullptr)
                right[i] = yR;
        }
    }

    // Curva composta HP + LP + bandas (ignora o de-esser, que e dinamico) nos
    // coeficientes-alvo: e a curva estatica que o grafico desenha por cima
    // do RTA. Secao desligada nao entra no produto.
    double responseDbAt (double freqHz) const noexcept
    {
        double db = 0.0;
        if (par.hpOn)
            db += slopeDb (hp, freqHz);
        if (par.lpOn)
            db += slopeDb (lp, freqHz);
        for (int i = 0; i < numBands; ++i)
            if (par.bands[i].on)
                db += magnitudeDb (bands[i].stage[0].targetCoeffs(), freqHz, sampleRate);
        return db;
    }

    // Resposta de uma secao isolada: 0 = HP, 1 = LP, 2..6 = bandas 0..4,
    // 7 = split do de-esser. Usada pelo inspector e pelo no da banda ativa.
    double sectionDbAt (int section, double freqHz) const noexcept
    {
        if (section == 0)
            return slopeDb (hp, freqHz);
        if (section == 1)
            return slopeDb (lp, freqHz);
        if (section >= 2 && section < 2 + numBands)
        {
            const auto& b = par.bands[section - 2];
            return magnitudeDb (bands[section - 2].stage[0].targetCoeffs(), freqHz, sampleRate)
                 * (b.on ? 1.0 : 0.0);
        }
        if (section == 2 + numBands)
            return magnitudeDb (essSplit[0].targetCoeffs(), freqHz, sampleRate);
        return 0.0;
    }

    // Reducao maxima (dB positivos) observada no ultimo bloco processado.
    double gainReductionDb() const noexcept { return blockGrPeak; }

    static int latencySamples() noexcept { return 0; }

private:
    struct SlopeSection
    {
        std::array<std::array<Biquad, 2>, 2> stage {}; // [estagio][canal]
        int stages = 1;
    };

    struct BandSection
    {
        std::array<Biquad, 2> stage {}; // [canal]
    };

    void rebuildSlope (SlopeSection& s, int type, double freq, bool slope24) noexcept
    {
        s.stages = slope24 ? 2 : 1;
        const auto c = coeffsFor (type, sampleRate, freq, 0.0, kButterworthQ);
        for (int st = 0; st < 2; ++st)
            for (int ch = 0; ch < 2; ++ch)
            {
                if (st < s.stages)
                    s.stage[st][ch].setTarget (c);
                else
                    s.stage[st][ch].setTarget (Coeffs {}); // passa-tudo
            }
    }

    void rebuildAll() noexcept
    {
        hp.stages = par.hpSlope24 ? 2 : 1;
        lp.stages = par.lpSlope24 ? 2 : 1;
        const auto hpc = coeffsFor (highPass, sampleRate, par.hpFreq, 0.0, kButterworthQ);
        const auto lpc = coeffsFor (lowPass, sampleRate, par.lpFreq, 0.0, kButterworthQ);
        for (int st = 0; st < 2; ++st)
            for (int ch = 0; ch < 2; ++ch)
            {
                hp.stage[st][ch].snap (st < hp.stages ? hpc : Coeffs {});
                lp.stage[st][ch].snap (st < lp.stages ? lpc : Coeffs {});
            }

        for (int i = 0; i < numBands; ++i)
        {
            const auto& b = par.bands[i];
            const auto c = coeffsFor (b.type, sampleRate, b.freq, b.gainDb, b.q);
            bands[i].stage[0].snap (c);
            bands[i].stage[1].snap (c);
        }

        const auto ec = coeffsBandpass (sampleRate, par.essFreq, par.essWidthQ);
        essSplit[0].snap (ec);
        essSplit[1].snap (ec);

        soloBand = -1;
        for (int i = 0; i < numBands; ++i)
            if (par.bands[i].on && par.bands[i].listen)
            {
                soloBand = i;
                break;
            }
    }

    double slopeDb (const SlopeSection& s, double f) const noexcept
    {
        double db = 0.0;
        for (int st = 0; st < s.stages; ++st)
            db += magnitudeDb (s.stage[st][0].targetCoeffs(), f, sampleRate);
        return db;
    }

    double essDetect (double splitL, double splitR) noexcept
    {
        // Envelope rapido da banda contra sua media lenta, somado nos dois
        // canais (detector linkado estereo). Sibilo = banda sobe rapido sem
        // que a media acompanhe -> excesso em dB acima do limiar de 2 dB,
        // razao 3:1, cortada no range. O follow de 5/120 ms evita flutter.
        const auto follow = [] (double& env, double v, double at, double rel) noexcept
        {
            env = v > env ? at * env + (1.0 - at) * v
                          : rel * env + (1.0 - rel) * v;
        };

        follow (essFast[0], std::fabs (splitL), kFastAt, kFastRel);
        follow (essFast[1], std::fabs (splitR), kFastAt, kFastRel);
        essSlow[0] = kSlow * essSlow[0] + (1.0 - kSlow) * essFast[0];
        essSlow[1] = kSlow * essSlow[1] + (1.0 - kSlow) * essFast[1];

        const double fast = essFast[0] + essFast[1];
        const double slow = essSlow[0] + essSlow[1];

        double grTarget = 0.0;
        if (slow > 1e-5)
        {
            const double excess = 10.0 * std::log10 ((fast + 1e-9) / (slow + 1e-9));
            grTarget = clampd ((excess - 2.0) * 3.0, 0.0, std::max (0.0, par.essRangeDb));
        }

        essGrDb = grTarget > essGrDb ? kGrAt * essGrDb + (1.0 - kGrAt) * grTarget
                                     : kGrRel * essGrDb + (1.0 - kGrRel) * grTarget;
        if (essGrDb > blockGrPeak)
            blockGrPeak = essGrDb;
        return std::pow (10.0, -essGrDb / 20.0);
    }

    static void dryWet (double x, double split, double gain, double listen,
                        double& out) noexcept
    {
        if (listen > 0.5)
            out = split * gain;
        else
            out = (x - split) + split * gain;
    }

    void processFrame (double inL, double inR, float& outL, float& outR) noexcept
    {
        double xL = inL, xR = inR;

        if (soloBand >= 0)
        {
            // LISTEN de banda: diferenca entre a saida da banda e a entrada
            // dela. Auditoria de quanto aquela secao esta mudando o sinal.
            auto& b = bands[soloBand];
            xL = b.stage[0].process (inL) - inL;
            xR = b.stage[1].process (inR) - inR;
        }
        else
        {
            if (par.hpOn)
                for (int st = 0; st < hp.stages; ++st)
                {
                    xL = hp.stage[st][0].process (xL);
                    xR = hp.stage[st][1].process (xR);
                }
            if (par.lpOn)
                for (int st = 0; st < lp.stages; ++st)
                {
                    xL = lp.stage[st][0].process (xL);
                    xR = lp.stage[st][1].process (xR);
                }
            for (int i = 0; i < numBands; ++i)
            {
                if (! par.bands[i].on)
                    continue;
                xL = bands[i].stage[0].process (xL);
                xR = bands[i].stage[1].process (xR);
            }
        }

        if (par.essOn)
        {
            const double splitL = essSplit[0].process (xL);
            const double splitR = essSplit[1].process (xR);
            const double gain = essDetect (splitL, splitR);
            const double listen = par.essListen ? 1.0 : 0.0;
            double oL = 0.0, oR = 0.0;
            dryWet (xL, splitL, gain, listen, oL);
            dryWet (xR, splitR, gain, listen, oR);
            xL = oL; xR = oR;
        }

        outL = static_cast<float> (xL);
        outR = static_cast<float> (xR);
    }

    double sampleRate = 48000.0;
    Params par {};
    SlopeSection hp {}, lp {};
    std::array<BandSection, numBands> bands {};
    std::array<Biquad, 2> essSplit {};
    int soloBand = -1;

    std::array<double, 2> essFast {}, essSlow {};
    double essGrDb = 0.0, blockGrPeak = 0.0;
    double kFastAt = 0.0, kFastRel = 0.0, kSlow = 0.0, kGrAt = 0.0, kGrRel = 0.0;
};

// Escalonamento APVTS(0..1) -> grandeza fisica, e o inverso para mostrar no
// inspector. Fica junto do engine para processor, editor e testes lerem a
// mesma curva.
inline double scaleFreq (double t) noexcept
{
    return 20.0 * std::pow (10.0, clampd (t, 0.0, 1.0) * 3.0); // 20 Hz .. 20 kHz
}

inline double scaleGain (double t) noexcept
{
    return clampd (t, 0.0, 1.0) * 36.0 - 18.0; // -18 .. +18 dB
}

inline double scaleQ (double t) noexcept
{
    return 0.1 * std::pow (10.0, clampd (t, 0.0, 1.0) * 2.0); // 0,1 .. 10
}

inline double scaleEssFreq (double t) noexcept
{
    return 4000.0 + clampd (t, 0.0, 1.0) * 8000.0; // 4 .. 12 kHz
}

inline double scaleEssWidth (double t) noexcept
{
    return 0.3 * std::pow (10.0, clampd (t, 0.0, 1.0) * 1.0); // Q 0,3 .. 3
}

inline double scaleEssRange (double t) noexcept
{
    return clampd (t, 0.0, 1.0) * 24.0; // 0 .. 24 dB de teto
}

inline int bandTypeFromNorm (double t) noexcept
{
    return static_cast<int> (std::lround (clampd (t, 0.0, 1.0) * 5.0));
}

inline double freqToNorm (double hz) noexcept
{
    return clampd (std::log10 (hz / 20.0) / 3.0, 0.0, 1.0);
}

} // namespace forja::traco
