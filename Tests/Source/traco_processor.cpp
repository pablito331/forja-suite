// Ponta a ponta do TRACO no processador REAL: o teste atravessa o caminho que
// a DAW percorre (APVTS -> paramsFromValues -> engine -> bypass crossfade ->
// medidores), coisa que o teste de engine puro nao cobre. Cobre o mapa de
// escala dos 43 parametros, o passa-tudo dos defaults, a latencia zero medida
// por impulso, o corte do HP global pela saida do plugin, o de-esser medido no
// medidor proprio, o crossfade de 10 ms do bypass sem resetar o filtro e o
// round-trip de estado. Console: nenhum editor e construido.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace
{

constexpr double testSampleRate = 48000.0;
constexpr int testBlockSize = 512;
constexpr double twoPi = 6.2831853071795864769;

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
    std::fflush (stdout);
}

using Load = std::function<float (const juce::String&)>;

void pumpMessages (int ms)
{
    if (auto* messages = juce::MessageManager::getInstanceWithoutCreating())
        messages->runDispatchLoopUntil (ms);
}

void setRaw (TracoAudioProcessor& processor, const juce::String& id, float value)
{
    processor.apvts.getParameter (id)->setValueNotifyingHost (value);
}

float rawOf (TracoAudioProcessor& processor, const juce::String& id)
{
    return processor.apvts.getParameter (id)->getValue();
}

Load loader (TracoAudioProcessor& processor)
{
    return [&processor] (const juce::String& id)
    {
        return processor.apvts.getRawParameterValue (id)->load();
    };
}

void makeVoice (std::vector<float>& l, std::vector<float>& r, int from, int to,
                double freq, double amp)
{
    for (std::size_t i = static_cast<std::size_t> (from); i < static_cast<std::size_t> (to);
         i = i + 1)
    {
        const double v = amp * std::sin (twoPi * freq * static_cast<double> (i) / testSampleRate);
        l[i] = static_cast<float> (v);
        r[i] = static_cast<float> (v);
    }
}

// Alimentar N amostras em blocos, com o buffer zerado antes de cada um.
void feed (TracoAudioProcessor& processor, const std::vector<float>& l,
           const std::vector<float>& r, std::vector<float>& outL,
           std::vector<float>& outR, int count)
{
    juce::AudioBuffer<float> buffer (2, testBlockSize);
    juce::MidiBuffer midi;

    for (int offset = 0; offset < count; offset += testBlockSize)
    {
        const int n = std::min (testBlockSize, count - offset);
        buffer.clear();

        for (int i = 0; i < n; ++i)
        {
            buffer.setSample (0, i, l[static_cast<std::size_t> (offset + i)]);
            buffer.setSample (1, i, r[static_cast<std::size_t> (offset + i)]);
        }

        processor.processBlock (buffer, midi);

        for (int i = 0; i < n; ++i)
        {
            outL[static_cast<std::size_t> (offset + i)] = buffer.getSample (0, i);
            outR[static_cast<std::size_t> (offset + i)] = buffer.getSample (1, i);
        }
    }
}

// Nivel de um tom puro medido por correlacao complexa sobre um numero inteiro
// de ciclos (mesma tecnica do teste de engine: imune a fase e a janela parcial).
double toneDb (const std::vector<float>& v, int start, double freq, int cycles)
{
    const int win = static_cast<int> (std::lround (static_cast<double> (cycles)
                                                   * testSampleRate / freq));
    double ci = 0.0;
    double cs = 0.0;

    for (int i = 0; i < win; ++i)
    {
        const double angle = twoPi * freq * static_cast<double> (i) / testSampleRate;
        ci += v[static_cast<std::size_t> (start + i)] * std::cos (angle);
        cs += v[static_cast<std::size_t> (start + i)] * std::sin (angle);
    }

    const double amp = 2.0 * std::sqrt (ci * ci + cs * cs) / static_cast<double> (win);
    return 20.0 * std::log10 (std::max (amp, 1e-12));
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceSetup;

    std::printf ("Traco ponta a ponta (processador real, SR %.0f Hz, bloco %d)\n",
                 testSampleRate, testBlockSize);

    // --- Mapa APVTS: os 43 parametros e as escalas documentadas -------------
    {
        TracoAudioProcessor processor;
        processor.prepareToPlay (testSampleRate, testBlockSize);

        // getParameters() expoe os 42 parametrizaveis; o bypass mora no APVTS e
        // sai por getBypassParameter(), exatamente como o host o ve.
        check (processor.getParameters().size() == 42
               && processor.apvts.getParameter ("bypass") != nullptr,
               "layout tem os 43 parametros documentados");

        const auto params = processor.paramsFromValues (loader (processor));

        check (params.bands[0].on && ! params.bands[1].on && ! params.bands[4].on,
               "default: so a banda 1 ligada");
        check (params.bands[0].type == forja::traco::bell
               && std::abs (params.bands[0].gainDb) < 1e-6,
               "default: banda 1 campainha em 0 dB");
        check (! params.hpOn && ! params.lpOn && ! params.essOn,
               "default: HP, LP e de-esser desligados");
        check (std::abs (params.hpFreq - 20.0 * std::pow (10.0, 0.18)) < 0.5,
               "default: hpFreq 0,06 na escala log 20-20k");
        check (std::abs (params.lpFreq - 20.0 * std::pow (10.0, 2.85)) < 1.0,
               "default: lpFreq 0,95 na escala log 20-20k");

        setRaw (processor, "b1Freq", 0.5f);
        setRaw (processor, "b1Gain", 0.75f);
        setRaw (processor, "b1Q", 0.5f);
        setRaw (processor, "b1Type", 0.4f);
        setRaw (processor, "hpSlope", 1.0f);
        setRaw (processor, "essFreq", 0.5f);
        setRaw (processor, "essWidth", 0.0f);
        setRaw (processor, "essRange", 1.0f);

        const auto scaled = processor.paramsFromValues (loader (processor));

        check (std::abs (scaled.bands[0].freq - 632.4555) < 0.5,
               "b1Freq 0,5 vira o meio da escala log (632,46 Hz)");
        check (std::abs (scaled.bands[0].gainDb - 9.0) < 1e-3,
               "b1Gain 0,75 vira +9 dB");
        check (std::abs (scaled.bands[0].q - 1.0) < 1e-3,
               "b1Q 0,5 vira Q 1,0 log");
        check (scaled.bands[0].type == forja::traco::highShelf,
               "b1Type 0,4 cai na prateleira alta (passo 0,2)");
        check (scaled.hpSlope24, "hpSlope na posicao 24 dB/oitava");
        check (std::abs (scaled.essFreq - 8000.0) < 1e-3
               && std::abs (scaled.essWidthQ - 0.3) < 1e-4
               && std::abs (scaled.essRangeDb - 24.0) < 1e-3,
               "de-esser: centro 8k, largura 0,3 e alcance 24 dB nos topos");

        setRaw (processor, "b1Type", 1.0f);
        check (processor.paramsFromValues (loader (processor)).bands[0].type
               == forja::traco::notch,
               "b1Type 1,0 fecha em notch sem passar do indice");
    }

    // --- Passa-tudo e latencia medida por impulso ---------------------------
    {
        TracoAudioProcessor processor;
        processor.prepareToPlay (testSampleRate, testBlockSize);

        const int n = 1024;
        std::vector<float> l (static_cast<std::size_t> (n), 0.0f);
        std::vector<float> r (static_cast<std::size_t> (n), 0.0f);
        std::vector<float> outL (static_cast<std::size_t> (n), 0.0f);
        std::vector<float> outR (static_cast<std::size_t> (n), 0.0f);

        // Rampa de ancora: o primeiro setParams ancora os coeficientes no alvo
        // com os novos valores (defaults do engine != defaults do APVTS). Deixa
        // a ancora passar antes de cobrar passa-tudo.
        {
            const int anchorN = 2048;
            std::vector<float> idle (static_cast<std::size_t> (anchorN), 0.0f);
            std::vector<float> sinkL (static_cast<std::size_t> (anchorN), 0.0f);
            std::vector<float> sinkR (static_cast<std::size_t> (anchorN), 0.0f);
            feed (processor, idle, idle, sinkL, sinkR, anchorN);
        }

        for (int i = 0; i < n; ++i)
        {
            const double v = 0.5 * std::sin (twoPi * 1000.0 * i / testSampleRate);
            l[static_cast<std::size_t> (i)] = static_cast<float> (v);
            r[static_cast<std::size_t> (i)] = static_cast<float> (v);
        }

        feed (processor, l, r, outL, outR, n);

        bool passThrough = true;
        for (int i = 0; i < n; ++i)
        {
            const float d = std::abs (outL[static_cast<std::size_t> (i)]
                                      - l[static_cast<std::size_t> (i)]);
            if (d > 1e-5f)
                passThrough = false;
        }
        check (passThrough, "defaults em 0 dB sao passa-tudo (max diff < 1e-5)");

        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        l[0] = 1.0f;
        r[0] = 1.0f;
        feed (processor, l, r, outL, outR, n);

        int peakIndex = 0;
        float peak = -1.0f;
        for (int i = 0; i < n; ++i)
        {
            const float v = std::abs (outL[static_cast<std::size_t> (i)]);
            if (v > peak)
            {
                peak = v;
                peakIndex = i;
            }
        }
        check (peakIndex == 0 && processor.getLatencySamples() == 0,
               "impulso sai no mesmo amostra: latencia 0 medida");

        // --- HP global pela saida do plugin ---------------------------------
        // Medir a uma oitava abaixo de fc: HP 12 dB/oitava atenua -12 dB la;
        // HP 24 dB/oitava, -24 dB. fc ~ 2047 Hz (raw 0.67, escala log 20-20k).
        setRaw (processor, "hpOn", 1.0f);
        setRaw (processor, "hpFreq", 0.67f);  // fc ~ 2047 Hz
        setRaw (processor, "hpSlope", 0.0f);  // 12 dB/oitava

        const int winHp = static_cast<int> (24.0 * testSampleRate / 1000.0); // 24 ciclos
        const int totalHp = 2048 + winHp;
        std::vector<float> lhp (static_cast<std::size_t> (totalHp), 0.0f);
        std::vector<float> rhp (static_cast<std::size_t> (totalHp), 0.0f);
        std::vector<float> outLhp (static_cast<std::size_t> (totalHp), 0.0f);
        std::vector<float> outRhp (static_cast<std::size_t> (totalHp), 0.0f);
        makeVoice (lhp, rhp, 0, totalHp, 1000.0, 0.5);

        feed (processor, lhp, rhp, outLhp, outRhp, totalHp);
        const double cut12 = toneDb (outLhp, 2048, 1000.0, 24)
                           - toneDb (lhp, 0, 1000.0, 24);
        std::printf ("  (HP12: %.2f dB)\n", cut12);
        check (std::abs (cut12 + 12.0) < 1.5, "HP 12: uma oitava abaixo corta ~12 dB");

        setRaw (processor, "hpSlope", 1.0f);  // 24 dB/oitava
        feed (processor, lhp, rhp, outLhp, outRhp, totalHp);
        const double cut24 = toneDb (outLhp, 2048, 1000.0, 24)
                           - toneDb (lhp, 0, 1000.0, 24);
        std::printf ("  (HP24: %.2f dB)\n", cut24);
        check (std::abs (cut24 + 24.0) < 2.0, "HP 24: uma oitava abaixo corta ~24 dB");

        // --- De-esser pelo medidor do processador ----------------------------
        setRaw (processor, "hpOn", 0.0f);
        setRaw (processor, "essOn", 1.0f);
        setRaw (processor, "essFreq", 0.375f);                    // ~7,1 kHz
        setRaw (processor, "essWidth", 0.9f);                      // Q ~2,5
        setRaw (processor, "essRange", 1.0f);                      // 24 dB

        const int totalEss = static_cast<int> (1.4 * testSampleRate);
        std::vector<float> le (static_cast<std::size_t> (totalEss), 0.0f);
        std::vector<float> re (static_cast<std::size_t> (totalEss), 0.0f);
        makeVoice (le, re, 0, totalEss, 1000.0, 0.4);
        makeVoice (le, re, static_cast<int> (0.8 * testSampleRate),
                   static_cast<int> (1.2 * testSampleRate), 7000.0, 0.45);

        std::vector<float> refL (static_cast<std::size_t> (totalEss), 0.0f);
        std::vector<float> refR (static_cast<std::size_t> (totalEss), 0.0f);
        setRaw (processor, "essRange", 0.0f);                      // alcance zero: passa
        feed (processor, le, re, refL, refR, totalEss);
        const double ref7k = toneDb (refL, static_cast<int> (0.83 * testSampleRate), 7000.0, 24);

        for (int b = 0; b < 4; ++b)
        {
            std::vector<float> idleL (static_cast<std::size_t> (2048), 0.0f);
            std::vector<float> idleR (static_cast<std::size_t> (2048), 0.0f);
            std::vector<float> sinkL (static_cast<std::size_t> (2048), 0.0f);
            std::vector<float> sinkR (static_cast<std::size_t> (2048), 0.0f);
            feed (processor, idleL, idleR, sinkL, sinkR, 2048);     // limpa medidor
        }

        std::vector<float> essL (static_cast<std::size_t> (totalEss), 0.0f);
        std::vector<float> essR (static_cast<std::size_t> (totalEss), 0.0f);
        setRaw (processor, "essRange", 1.0f);
        feed (processor, le, re, essL, essR, totalEss);
        const auto snapshot = processor.readMeters();

        const double cut7k = toneDb (essL, static_cast<int> (0.83 * testSampleRate), 7000.0, 24)
            - ref7k;
        check (cut7k <= -3.0, "rajada de 7 kHz sai atenuada pela saida do plugin");
        check (snapshot.essGainReductionDb >= 1.0f,
               "medidor de-esser registra reducao nos blocos da rajada");
        check (snapshot.inPeakL > 0.3f && snapshot.outPeakL > 0.3f,
               "medidores de entrada e saida acompanham o sinal");
    }

    // --- Bypass: crossfade de 10 ms sem resetar o filtro --------------------
    // Usa prateleira baixa (lowShelf) em vez de campainha porque a campainha
    // tem ganho unitario em DC (0 Hz). A prateleira boosta DC por A = 10^(g/40),
    // tornando a diferenca wet/dry visivel na saida.
    {
        TracoAudioProcessor processor;
        processor.prepareToPlay (testSampleRate, testBlockSize);

        setRaw (processor, "b1On", 1.0f);
        setRaw (processor, "b1Type", 0.2f);    // lowShelf
        setRaw (processor, "b1Freq", 0.4164f); // ~1 kHz
        setRaw (processor, "b1Gain", 0.8333f); // +12 dB
        setRaw (processor, "b1Q", 0.5f);

        const int n = 4096;
        std::vector<float> dc (static_cast<std::size_t> (n), 0.2f);
        std::vector<float> outL (static_cast<std::size_t> (n), 0.0f);
        std::vector<float> outR (static_cast<std::size_t> (n), 0.0f);

        feed (processor, dc, dc, outL, outR, 2048);                 // regime molhado
        const float wet = outL[static_cast<std::size_t> (2047)];
        std::printf ("  (wet DC: %.4f)\n", wet);
        check (std::abs (wet - 0.2f) > 0.05f, "prateleira baixa +12 dB mexe no regime DC");

        setRaw (processor, "bypass", 1.0f);
        feed (processor, dc, dc, outL, outR, 512);
        const float earlyFade = std::abs (outL[256] - 0.2f);
        check (earlyFade > 0.005f, "entrada do bypass e rampa, nao degrau");

        feed (processor, dc, dc, outL, outR, 1024);
        check (std::abs (outL[static_cast<std::size_t> (1023)] - 0.2f) < 1e-3,
               "bypass assenta no seco depois da rampa de 10 ms");

        setRaw (processor, "bypass", 0.0f);
        feed (processor, dc, dc, outL, outR, 16);
        const float rampBack = std::abs (outL[8] - wet);
        check (rampBack > 0.01f && std::abs (outL[8] - 0.2f) > 0.002f,
               "voltada do bypass re-crossfada em direcao ao wet");

        feed (processor, dc, dc, outL, outR, 1024);
        check (std::abs (outL[static_cast<std::size_t> (1023)] - wet) < 1e-3,
               "estado do filtro sobrevive ao bypass: wet volta identico");
    }

    // --- Round-trip de estado ------------------------------------------------
    {
        TracoAudioProcessor processor;

        setRaw (processor, "b3On", 1.0f);
        setRaw (processor, "b3Freq", 0.3f);
        setRaw (processor, "hpOn", 1.0f);
        setRaw (processor, "essListen", 1.0f);

        pumpMessages (200);  // flush APVTS -> ValueTree antes de serializar

        juce::MemoryBlock state;
        processor.getStateInformation (state);

        TracoAudioProcessor revived;
        revived.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

        const float gotB3Freq = rawOf (revived, "b3Freq");
        const float gotB3On = rawOf (revived, "b3On");
        const float gotHpOn = rawOf (revived, "hpOn");
        const float gotEssListen = rawOf (revived, "essListen");
        const float gotB1On = rawOf (revived, "b1On");
        std::printf ("  (round-trip: b3Freq=%.4f b3On=%.4f hpOn=%.4f essListen=%.4f b1On=%.4f)\n",
                     gotB3Freq, gotB3On, gotHpOn, gotEssListen, gotB1On);
        check (std::abs (gotB3Freq - 0.3f) < 1e-3, "round-trip: b3Freq sobrevive");
        check (gotB3On > 0.5f, "round-trip: b3On sobrevive");
        check (gotHpOn > 0.5f, "round-trip: hpOn sobrevive");
        check (gotEssListen > 0.5f, "round-trip: essListen sobrevive");
        check (gotB1On > 0.5f, "round-trip: b1On default (liga) sobrevive");
    }

    // --- Mono pela API do host ------------------------------------------------
    {
        TracoAudioProcessor processor;

        juce::AudioProcessor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());

        check (processor.isBusesLayoutSupported (mono),
               "layout mono aceito pelo processador");

        check (processor.setBusesLayout (mono), "setBusesLayout aplica o mono");
        processor.prepareToPlay (testSampleRate, testBlockSize);

        const int n = 1024;
        std::vector<float> l (static_cast<std::size_t> (n), 0.0f);
        std::vector<float> r (static_cast<std::size_t> (n), 0.0f);
        std::vector<float> outL (static_cast<std::size_t> (n), 0.0f);
        std::vector<float> outR (static_cast<std::size_t> (n), 0.0f);

        for (int i = 0; i < n; ++i)
            l[static_cast<std::size_t> (i)] = static_cast<float> (
                0.5 * std::sin (twoPi * 1000.0 * i / testSampleRate));

        feed (processor, l, r, outL, outR, n);
        check (std::abs (outL[static_cast<std::size_t> (n - 1)]) > 0.3f,
               "layout mono processa e mede pelo canal 0");

        const auto snapshot = processor.readMeters();
        check (snapshot.inPeakR > 0.3f, "mono publica o mesmo pico nos dois medidores");
    }

    if (failures == 0)
    {
        std::printf ("\nPASS: Traco ponta a ponta\n");
        return 0;
    }

    std::printf ("\nFAIL: %d verificacao(oes)\n", failures);
    return 1;
}
