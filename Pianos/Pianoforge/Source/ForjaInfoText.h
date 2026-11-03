#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::pianoforge
{

// FUNDAMENTOS do PIANOFORGE, no padrao da suite. O texto todo em escapes
// \uXXXX: a regra manda codigo e comentarios em ASCII puro.
inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "PIANOFORGE";
    info.subtitle = u ("PIANO DE AMOSTRAS + MODELO F\u00cdSICO \u00b7 FORJA");
    info.tagline = u ("PIANOFORGE pega um soundfont de piano comum e acrescenta o "
                      "comportamento f\u00edsico do instrumento: o transiente do "
                      "martelo, a resson\u00e2ncia das cordas, a tampa, o corpo e o "
                      "espa\u00e7o \u2014 o que um player s\u00f3 de amostras n\u00e3o tem.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Abre um banco .sf2/.sf3 solto ou um .pforge (o container do "
               "projeto-irm\u00e3o) e o toca como piano de amostras, sobrepondo uma "
               "cadeia de modelagem f\u00edsica na ordem fixa: transiente de martelo "
               "(click do feltro + thump da caixa, calibrado por nota, mais brilhante "
               "e mais seco no fortissimo), resson\u00e2ncia simp\u00e1tica por banco de "
               "ressonadores, resposta do tampo (a curva de radia\u00e7\u00e3o do corpo de "
               "um piano de cauda: peito no baixo-m\u00e9dio, articula\u00e7\u00e3o no bridge, "
               "ar, sub domado), modos da madeira (o anel que continua vibrando depois "
               "da nota), EQ de corpo em 3 bandas, filtro da tampa, chorus, reverb FDN "
               "e est\u00e1gio master com largura, trim, volume e soft-clip."),
            u ("A curva de din\u00e2mica \u00e9 aplicada antes do sampler e o pedal de "
               "sustain segue a sem\u00e2ntica medida do projeto antigo: n\u00e3o furta "
               "voz que o envelope ainda segura. Cada nota soa no instante certo: o "
               "bloco \u00e9 fatiado nos tempos dos eventos MIDI."),
            u ("Por cima da cadeia f\u00edsica, duas camadas de 'molho' de worship, "
               "ambas opt-in: um Compressor de um knob \u2014 o MESMO compressor de "
               "canal do mixer do Drumforge, DSP original da Forja, com attack finito "
               "e ganho suavizado (nada modula dentro do ciclo, por "
               "isso n\u00e3o degrada), ratio fixa em 5:1, que ACHATA a macro-din\u00e2mica enquanto um "
               "makeup de la\u00e7o fechado segura o n\u00edvel percebido (girar n\u00e3o "
               "derruba o volume) \u2014, e um Molho "
               "que \u00e9 um shimmer espectral \u2014 n\u00e3o um transpositor no tempo, "
               "mas um resonador no dom\u00ednio da frequ\u00eancia: o sinal \u00e9 cortado "
               "em gr\u00e3os (janela de Hann, sobreposi\u00e7\u00e3o 4x), a energia "
               "(magnitude) de cada bin \u00e9 guardada num espectro que decai "
               "exponencialmente e reconstru\u00edda com FASE ALEAT\u00d3RIA \u2014 \u00e9 a "
               "fase aleat\u00f3ria que dissolve formantes e transientes e mata a voz "
               "robotizada do shimmer granular. Sobre esse espectro roda o SHIFT de "
               "intervalo FIXO: uma c\u00f3pia da energia difusa \u00e9 reposicionada uma "
               "oitava acima (bin k \u2192 2k, exatamente +12) e, na dose da quinta, uma "
               "d\u00e9cima-segunda acima (k \u2192 3k, oitava + 5\u00aa). A c\u00f3pia "
               "n\u00e3o \u00e9 realimentada no espectro, ent\u00e3o o intervalo n\u00e3o "
               "comp\u00f5e nem sobe escada \u2014 halo sempre harm\u00f4nico, com salto "
               "previs\u00edvel (resposta a \"quantas +12 eu pulo?\": exatamente uma, "
               "mais a 12\u00aa da Quinta). T\u00e9cnica reimplementada do Atlantis "
               "Reverb (Geraint Luff), sem copiar c\u00f3digo. Tr\u00eas controles: MOLHO "
               "= n\u00edvel/intensidade do halo (0 = off; no talo dosado a 30% do "
               "ganho antigo), QUINTA = dose da 12\u00aa por cima da oitava (0 = s\u00f3 "
               "oitava), CAUDA = tempo (RT) do resonador. Auto-pan lento no halo. "
               "Roda no barramento depois do reverb. Em 0% cada camada "
               "\u00e9 bypass honesto, amostra a amostra.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("Por baixo das amostras toca o TinySoundFont v0.9 (MIT, Bernhard "
               "Schelling), um sintetizador SoundFont2 de um \u00fanico header. O "
               "banco \u00e9 lido em thread de background e publicado no \u00e1udio por "
               "token; o preset \u00fanico e as vozes s\u00e3o pr\u00e9-alocados na carga "
               "para o processBlock nunca alocar mem\u00f3ria."),
            u ("A cadeia f\u00edsica \u00e9 portada do projeto-irm\u00e3o pianoforge (n\u00e3o "
               "\u00e9 paridade MIT como os adaptadores Airwindows): martelo, "
               "ressonadores, EQ, tampa, chorus, FDN e master em classes pr\u00f3prias "
               "no espa\u00e7o forja::pianoforge, com smoothing por frames (o conserto "
               "documentado do repo antigo) e buffers pr\u00e9-dimensionados para 192 "
               "kHz sem aloca\u00e7\u00e3o em tempo real."),
            u ("O formato .pforge continua leg\u00edvel aqui: o leitor confere o CRC32 "
               "de cada entrada e entrega o SF2 dentro; bundle corrompido vira erro "
               "leg\u00edvel, nunca o banco no ar derrubado. Lat\u00eancia medida por "
               "impulso nos testes da su\u00edte.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Deposite os .sf2/.sf3/.pforge em Documentos/Forja/Pianoforge, toque "
               "ATUALIZAR e escolha o banco. A carga roda em segundo plano; o status "
               "mostra o resultado. Cinco cards trazem os controles aud\u00edveis "
               "(Din\u00e2mica, Corpo, Equaliza\u00e7\u00e3o, Molho e Espa\u00e7o). Os cards "
               "Corpo e Molho ficam achatados para ceder palco ao piano de cauda "
               "desenhado \u2014 agora em VISTA DE FRENTE (perspectiva de quem toca): "
               "teclado embaixo, a tampa levantando atr\u00e1s conforme o par\u00e2metro "
               "Tampa, e a partitura no atril mostrando o NOME do banco carregado. "
               "A aba MOTOR abre os dez discos de "
               "ajuste fino (tampa, curva de velocity, sala, trim, largura, abafar "
               "soltura, brilho do martelo, afina\u00e7\u00e3o da resson\u00e2ncia, afina\u00e7\u00e3o "
               "global e a dose do molho). BYPASS silencia com rampa de 10 ms sem "
               "parar as vozes.")
        }
    });

    info.controls.push_back ({ u ("Volume"), u ("0% a 100% \u00b7 padr\u00e3o 82%"),
                               u ("N\u00edvel de sa\u00edda, aplicado no est\u00e1gio master "
                                  "antes do soft-clip.") });
    info.controls.push_back ({ u ("Martelo"), u ("0% a 100% \u00b7 padr\u00e3o 16%"),
                               u ("N\u00edvel do transiente f\u00edsico do ataque (click do "
                                  "feltro + thump da caixa), calibrado por nota.") });
    info.controls.push_back ({ u ("Din\u00e2mica"), u ("0% a 100% \u00b7 padr\u00e3o 50%"),
                               u ("Curva de velocity aplicada antes do sampler; 50% "
                                  "\u00e9 linear.") });
    info.controls.push_back ({ u ("Resson\u00e2ncia"), u ("0% a 100% \u00b7 padr\u00e3o 45%"),
                               u ("Ganho do banco de ressonadores simp\u00e1ticos ligados "
                                  "\u00e0s teclas ativas e ao pedal.") });
    info.controls.push_back ({ u ("Tampa"), u ("0% a 100% \u00b7 padr\u00e3o 52%"),
                               u ("Abertura da tampa: lowpass de 2,5 kHz (fechada) a "
                                  "16 kHz (aberta); transparente em 100%. \u00c9 o mesmo "
                                  "valor que levanta a tampa do piano desenhado na "
                                  "tela PIANO.") });
    info.controls.push_back ({ u ("Corpo"), u ("0% a 100% \u00b7 padr\u00e3o 62%"),
                               u ("Resposta do tampo: a curva de radia\u00e7\u00e3o do corpo "
                                  "de um piano de cauda (peito +5,5 dB em 92 Hz, "
                                  "articula\u00e7\u00e3o no bridge, ar, sub domado). Unity em "
                                  "0%, ent\u00e3o n\u00e3o destr\u00f3i a amostra.") });
    info.controls.push_back ({ u ("Madeira"), u ("0% a 100% \u00b7 padr\u00e3o 50%"),
                               u ("Modos do tampo: dez ressonadores de alto Q nas "
                                  "frequ\u00eancias modais da madeira, que continuam "
                                  "vibrando depois da nota. Aditivo puro, bypass em 0%.") });
    info.controls.push_back ({ u ("Graves/M\u00e9dios/Agudos"), u ("\u00b18 dB \u00b7 padr\u00e3o 0 dB"),
                               u ("EQ de corpo: low shelf 120 Hz, peak 1 kHz Q0,8, "
                                  "high shelf 5 kHz.") });
    info.controls.push_back ({ u ("Chorus"), u ("0% a 100% \u00b7 padr\u00e3o 0%"),
                               u ("Chorus de 2 vozes, LFO 0,8 Hz.") });
    info.controls.push_back ({ u ("Reverb"), u ("0% a 100% \u00b7 padr\u00e3o 12%"),
                               u ("Mistura do reverb FDN de concerto.") });
    info.controls.push_back ({ u ("Compressor"), u ("achatar 0% a 100% \u00b7 padr\u00e3o 0% (off, opt-in)"),
                               u ("Um knob \u2014 compressor ORIGINAL da Forja "
                                  "(engine/dsp/Compressor.h), n\u00e3o o Dyno/\u00cdmpeto: "
                                  "\u00e9 o MESMO compressor de canal do mixer do "
                                  "Drumforge (mesmo algoritmo) \u2014 detector de pico "
                                  "est\u00e9reo-linkado com decaimento, joelho de 6 dB e a "
                                  "redu\u00e7\u00e3o suavizada em dB por dois tempos (attack "
                                  "~3 ms / release ~120 ms), ent\u00e3o o ganho n\u00e3o "
                                  "modula dentro do ciclo e n\u00e3o degrada. Ratio fixa em "
                                  "5:1 (cada 1 dB acima do limiar vira 5 dB de redu\u00e7\u00e3o); "
                                  "o knob "
                                  "s\u00f3 aprofunda o threshold (de 0 a \u221240 dB). Um "
                                  "makeup de la\u00e7o fechado (camada Forja) segura o "
                                  "n\u00edvel percebido, ent\u00e3o girar ACHATA a "
                                  "macro-din\u00e2mica sem derrubar o volume. Gesto "
                                  "invertido na UI: gire para a ESQUERDA para achatar mais; "
                                  "a direita \u00e9 o off "
                                  "(0%, bypass honesto \u2014 sa\u00edda = entrada).") });
    info.controls.push_back ({ u ("Molho"), u ("0% a 100% \u00b7 padr\u00e3o 0% (opt-in)"),
                               u ("N\u00edvel/intensidade do halo espectral: o "
                                  "resonador corta o sinal em gr\u00e3os (Hann, sobreposi\u00e7\u00e3o "
                                  "4x), guarda a energia por bin e reconstr\u00f3i com fase "
                                  "aleat\u00f3ria (dissolve formantes, mata a voz robotizada); "
                                  "uma c\u00f3pia dessa energia difusa \u00e9 deslocada "
                                  "exatamente uma oitava (+12) e somada por cima do seco "
                                  "\u2014 o salto \u00e9 fixo, n\u00e3o sobe escada, sempre "
                                  "harm\u00f4nico. T\u00e9cnica reimplementada do Atlantis Reverb "
                                  "(Geraint Luff), sem copiar c\u00f3digo. No talo dosado a "
                                  "30% do ganho antigo. Auto-pan lento abre o halo no "
                                  "campo. Roda no barramento depois do reverb. "
                                  "Em 0% \u00e9 bypass honesto.") });
    info.controls.push_back ({ u ("Quinta"), u ("0% a 100% \u00b7 padr\u00e3o 0% (opt-in)"),
                               u ("Dose da d\u00e9cima-segunda (oitava + 5\u00aa) somada por "
                                  "cima da oitava no shift (bin k \u2192 3k, al\u00e9m do "
                                  "k \u2192 2k). Como o shift n\u00e3o \u00e9 realimentado, a "
                                  "12\u00aa nunca recomp\u00f5e (nada de 9\u00aa/27\u00aa "
                                  "dissonante). 0% = s\u00f3 oitava (+12 limpo); 100% = 12"
                                  "\u00aa t\u00e3o presente quanto a oitava, som mais denso. "
                                  "S\u00f3 age com o molho aberto.") });
    info.controls.push_back ({ u ("Cauda"), u ("0% a 100% \u00b7 padr\u00e3o 50%"),
                               u ("Tempo (RT) do resonador espectral por onde o shimmer "
                                  "passa: 0 = halo de decay curto, 1 = pad de cauda longa. "
                                  "\u00c9 o comprimento do halo, n\u00e3o eco que repete a tecla. "
                                  "S\u00f3 age com o molho aberto; mora no MOTOR.") });
    info.controls.push_back ({ u ("MOTOR"), u ("10 discos"),
                               u ("Tampa, Curva de velocity, Sala (4 tipos), Trim "
                                  "(\u00b112 dB), Largura (0\u20132), Abafar soltura, Brilho do "
                                  "martelo, Afin. da resson\u00e2ncia, Afin. global "
                                  "(\u00b1100 cents) e a dose do molho (Cauda).") });

    info.provenance.source = "TinySoundFont v0.9 (schellingb/TinySoundFont); tecnica de shimmer espectral reimplementada do Atlantis Reverb (Geraint Luff), sem copiar codigo";
    info.provenance.author = u ("Bernhard Schelling (tsf); Forja (cadeia f\u00edsica, "
                                "compressor de um knob, shimmer espectral e UI); "
                                "Geraint Luff (inspiracao de tecnica do Atlantis)");
    info.provenance.license = u ("MIT (tsf). O compressor de um knob e o shimmer "
                                 "espectral sao DSP original da Forja. O shimmer foi "
                                 "reimplementado a partir da tecnica publica do "
                                 "Atlantis (nenhum codigo copiado; a licen\u00e7a do "
                                 "Atlantis nao e MIT, entao nada dele e vendorizado). "
                                 "SF3 usa stb_vorbis, dom\u00ednio "
                                 "p\u00fablico. Sem FluidSynth (LGPL) por regra da su\u00edte.");
    info.provenance.commit = u ("vendor byte-exato em "
                                "Forja/Source/forja/vendor/soundfont (SHA-256 em "
                                "PROVENANCE.md); cadeia portada do projeto-irm\u00e3o "
                                "pianoforge; compressor \u00e9 DSP original da Forja e o "
                                "MESMO compressor de canal do mixer do Drumforge (mesmo "
                                "algoritmo) em "
                                "Pianos/Pianoforge/Source/engine/dsp/Compressor.h "
                                "(detector de pico com decaimento, joelho 6 dB e redu\u00e7\u00e3o "
                                "suavizada em dB por dois tempos \u2014 ganho que n\u00e3o degrada \u2014, "
                                "ratio fixa em 5:1, threshold \u221240\u21920 dB \u2014 faixa do canal \u2014, mais "
                                "makeup de la\u00e7o "
                                "fechado (camada Forja); sem vendor e sem paridade de disco); shimmer \u00e9 engine "
                                "espectral proprio em "
                                "Pianos/Pianoforge/Source/engine/dsp/Shimmer.h "
                                "(FFT radix-2 + energia por bin com decaimento + fase "
                                "aleatoria + shift fixo de oitava/quinta), tecnica "
                                "reimplementada do Atlantis, sem vendor e sem "
                                "paridade de disco");
    info.provenance.notes = u ("O PIANOFORGE \u00e9 o produto da su\u00edte; o reposit\u00f3rio "
                               "irm\u00e3o pianoforge fica como refer\u00eancia hist\u00f3rica de "
                               "DSP. N\u00e3o h\u00e1 paridade com a Airwindows \u2014 o "
                               "compressor de um knob e o shimmer s\u00e3o DSP original da "
                               "Forja (o shimmer nao e mais o reverb ClearCoat: e um resonador "
                               "espectral original). "
                               "Aqui os "
                               "testes cobrem ado\u00e7\u00e3o de banco com reap, sustain, "
                               "fatiamento de eventos, monotonia de tampa/martelo, o "
                               "mapeamento bit-exato dos 23 par\u00e2metros, a prova de "
                               "energia no peito (banda 80\u2013500 Hz) para corpo e "
                               "madeira, a prova de que o compressor n\u00e3o degrada "
                               "(THD baixo num seno puro, attack finito e ganho "
                               "suavizado) e ainda assim ACHATA a macro-din\u00e2mica "
                               "segurando o n\u00edvel (range alto/baixo cai, RMS no "
                               "lugar) e "
                               "que a CAUDA dosar o RT do halo espectral (halo "
                               "que floresce, n\u00e3o eco que repete), a "
                               "rejei\u00e7\u00e3o "
                               "de .pforge com CRC errado e a aus\u00eancia de aloca\u00e7\u00e3o "
                               "no \u00e1udio.");

    return info;
}

} // namespace forja::pianoforge
