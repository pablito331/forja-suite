#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forjarack
{

// Painel FUNDAMENTOS do ForjaRack. Como o rack nao tem um unico modelo de
// origem, esta pagina documenta a cadeia, os modulos (com o nome original de
// cada adaptador Airwindows MIT) e a camada Forja de roteamento/master.
inline forja::ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return forja::ui::utf8 (text); };

    forja::ui::ForjaInfo info;
    info.title = "FORJARACK";
    info.subtitle = u ("RACK SERIE 500 \u00b7 FORJA");
    info.tagline = u ("Oito gavetas em cadeia: encaixe modulos Airwindows MIT e "
                      "monte uma fileira de timbre, dinamica, EQ, ambiente e lo-fi "
                      "sem sair de um unico plug-in.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("O ForjaRack e um roteador em cadeia estilo Serie 500. Cada um dos oito "
               "slots hospeda um modulo que envolve um adaptador Airwindows ja validado "
               "por paridade na su\u00edte. O sinal percorre os slots da esquerda para a "
               "direita; um slot vazio ou em bypass e pass-through exato."),
            u ("Nao inventamos DSP: o timbre de cada gaveta vem do modelo original. O "
               "que e explicitamente da Forja e a cadeia, as chaves de slot e a secao "
               "master, todas descritas abaixo.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O rack mantem um grafo de modulos montado na thread de mensagens; ao "
               "escolher um modulo o grafo e refeito sob suspensao do audio e os knobs "
               "assumem os defaults documentados daquele modelo. A gaveta passa a tocar "
               "sozinha quando recebe um modulo: a unica chave de on/off dela e o "
               "bypass. O controle por bloco (bypass, solo, parametros e master) chega "
               "por um snapshot POD lido no caminho de audio, que nao aloca."),
            u ("Solo em qualquer gaveta tira as demais da cadeia; o solo de uma gaveta "
               "ja em bypass nao conta, para nao silenciar o rack inteiro."),
            u ("Cada gaveta mostra um led de saida com o pico do proprio modulo; o "
               "master mede entrada e saida do barramento em estereo."),
            u ("O catalogo de modulos e um registro expans\u00edvel: cada m\u00f3dulo tem "
               "um id est\u00e1vel, knobs declarados e uma f\u00e1brica. Al\u00e9m dos efeitos "
               "Airwindows, o rack j\u00e1 aceita m\u00f3dulos de instrumento (papel 'instrument'): "
               "recebem MIDI por bloco e somam sua voz \u00e0 cadeia. \u00c9 a porta de entrada "
               "para futuras expans\u00f5es, como leitor de sf2/sf3 e uma mini drum que l\u00ea os "
               "kits do Drumforge.")
        }
    });

    info.sections.push_back ({
        u ("FX em cadeia ou instrumento"),
        {
            u ("O rack aceita os dois usos pela mesma instancia. Numa faixa com "
               "audio (entrada esterio ativa), a cadeia processa o sinal que "
               "entra e sai efeitos em serie. Numa faixa de instrumento (sem "
               "entrada de audio), o barramento de entrada e tratado como "
               "sil\u00eancio e a saida passa a ser a voz dos slots-instrumento "
               "(MINIDRUM, CANIVETE e plugins hospedados), que recebem o MIDI da "
               "faixa e somam em cima."),
            u ("\u00c9 assim que o teclado ao vivo toca o instrumento dentro do rack, "
               "como toca no plugin solto: coloque o ForjaRack numa faixa de "
               "instrumento e escolha o modulo na gaveta. Mantenha a entrada "
               "ligada apenas quando quiser encadear efeitos sobre audio."),
            u ("O led MIDI na secao MASTER acende quando um bloco chega com MIDI "
               "(nota, CC ou pitch bend) para qualquer gaveta. Ele existe porque o "
               "medidor de ENTRADA so mostra audio: numa faixa de instrumento a "
               "entrada e silencio, entao o led e a unica forma de ver se o teclado "
               "esta de fato chegando ao plug-in. Led aceso e gaveta muda = problema "
               "de som do modulo; led apagado = o MIDI nao esta entrando na faixa.")
        }
    });

    info.sections.push_back ({
        u ("Gaveta HOSPEDADA"),
        {
            u ("A ficha HOSPEDADO no seletor abre a gaveta din\u00e2mica: qualquer "
               "plug-in VST3 (Audio Unit no macOS) solto na pasta "
               "Documentos/Forja/Rack aparece no menu ARQUIVO da gaveta e roda ali "
               "dentro, sem recompilar a su\u00edte. O bot\u00e3o BUSCAR revarre a pasta."),
            u ("O rack exp\u00f5e os knobs do plugin vivo: nome, quantidade e valor "
               "v\u00eam do pr\u00f3prio plugin. Se o plugin traz editor, EDITAR abre essa "
               "UI em janela separada; enquanto ela estiver aberta o rack n\u00e3o "
               "escreve nos par\u00e2metros. Sem editor, EDITAR abre a lista gen\u00e9rica "
               "de par\u00e2metros."),
            u ("Plugins hospedados mant\u00eam a pr\u00f3pria licen\u00e7a: n\u00e3o s\u00e3o copiados, "
               "nem compilados, nem ligados \u00e0 su\u00edte; s\u00e3o carregados em tempo de "
               "execu\u00e7\u00e3o, como em qualquer DAW. Efeitos processam o barramento; "
               "instrumentos recebem o MIDI da faixa e somam sua voz; a lat\u00eancia "
               "reportada pelo plugin sobe para o host (PDC).")
        }
    });

    info.sections.push_back ({
        u ("Banda de arquivos (instrumentos)"),
        {
            u ("As gavetas de instrumento com biblioteca propria (MINIDRUM, lendo os "
               "kits .dfkit do Drumforge, e CANIVETE, lendo soundfonts .sf2/.sf3) "
               "mostram um seletor com o conteudo da pasta "
               "Documentos/Forja/Drumforge/Kits ou Documentos/Forja/Canivete. "
               "Escolher pela lista sincroniza o knob de indice e o material "
               "passa a ser comandado por ele; a escolha fica gravada no preset "
               "do rack."),
            u ("O bot\u00e3o ARQUIVO abre um kit/bank fora da biblioteca, direto do "
               "disco (FileChooser nativo). Enquanto o arquivo externo toca, o "
               "knob de indice fica mudo; a escolha pela lista devolve o comando "
               "a ele. Some o arquivo, a gaveta volta ao knob em silencio."),
            u ("A gaveta CANIVETE ganha uma segunda fileira, INSTRUMENTO, quando "
               "o bank assenta: a lista de presets do soundfont carregado, como "
               "no plugin solto. A escolha \u00e9 gravada no preset do rack pelo nome "
               "(o indice nao \u00e9 est\u00e1vel entre banks) e volta ao primeiro preset "
               "quando o bank muda. Durante a carga em segundo plano a fileira "
               "some; ela reaparece sozinha quando o bank fica toc\u00e1vel.")
        }
    });

    info.sections.push_back ({
        u ("Modulos e origem"),
        {
            u ("COR \u00b7 CERA (PurestWarm): calor suave de um knob. MEL (Sweeten): "
               "adocar os agudos. CORPO (Mojo): encorpado de um knob. IMPETO (Dyno): "
               "transiente/punch."),
            u ("DRIVE \u00b7 LIXA (PurestDrive): drive/grao. OXIDO (IronOxide5): faixa "
               "de fita com flutter e ruido. REBARBA (Slew3): suaviza as arestas. "
               "LAMINADOR (ToTape9): fita de nove knobs com vies, flutter e cabecote."),
            u ("DINAMICA \u00b7 TEMPERA (ButterComp2): glue de barramento. MURALHA "
               "(Dynamics): limitador com teto e modo. DIQUE (FinalClip): limiter de "
               "teto fixo, sem knob. MALHO (Pop2): compressor punch. PRENSA "
               "(Pressure5): compressor de barramento. ESCORIA (DeBess): "
               "de-esser/controle de graves."),
            u ("EQ \u00b7 CINZEL (Parametric): tres bandas com ganho e frequencia e "
               "mix. TORNO (Baxandall3): grave e agudo com entrada. LIMA (SmoothEQ2): "
               "quatro bandas com frequencia propria."),
            u ("LOFI \u00b7 GRANALHA (DeRez5): degrada\u00e7\u00e3o digital \u2014 taxa, bits, "
               "quadrado, brillo e graveza."),
            u ("AMBIENTE \u00b7 FOLEGO (kStation): sala curta. SALAO (kRockstar2): "
               "reverb de salao. REPIQUE (TapeDelay2): delay fita. ECO (Echojam): "
               "eco/spatial de quatro knobs.")
        }
    });

    info.sections.push_back ({
        u ("Camada Forja e desvios"),
        {
            u ("Secao master (trim de entrada, trim de saida a +-24 dB, inversao de "
               "fase do direito) e as chaves por slot (bypass e solo) sao "
               "camada Forja de roteamento, nao DSP do modelo."),
            u ("SALAO roda com room tone desligado para o master nao assobiar em "
               "silencio; REPIQUE e ECO rodam assincronos, o tempo vem do knob. DIQUE "
               "(FinalClip) nao expoe parametro no modelo original e so roteja. Bypass "
               "de slot e pass-through; o bypass global usa rampa de 10 ms."),
            u ("Latencia reportada: 0 amostras (PDC). Reverb e delay tem atraso "
               "musical, nao compensacao automatica, como nos originais da familia "
               "Space.")
        }
    });

    info.controls.push_back ({ u ("Seletor de modulo"), u ("vazio, CERA ... ECO, HOSPEDADO"),
                               u ("Escolhe o adaptador do slot e aplica seus defaults. "
                                  "HOSPEDADO carrega um plugin da pasta Rack.") });
    info.controls.push_back ({ u ("Bypass da gaveta"), u ("ligado/desligado"),
                               u ("Unica chave de on/off do slot: ligada, a gaveta e "
                                  "pass-through exato.") });
    info.controls.push_back ({ u ("Solo da gaveta"), u ("ligado/desligado"),
                               u ("Tira as demais gavetas da cadeia. Nao conta se a "
                                  "propria gaveta estiver em bypass.") });
    info.controls.push_back ({ u ("Knobs do modulo"), u ("0,00 a 1,00"),
                               u ("Escalonamento no DSP de cada modelo.") });
    info.controls.push_back ({ u ("Arquivo hospedado"), u ("nenhum / plugin listado"),
                               u ("Escolhe o plug-in carregado na gaveta HOSPEDADO.") });
    info.controls.push_back ({ u ("Arquivo da gaveta"), u ("kit / soundfont listado"),
                               u ("Escolhe o material tocado na gaveta de instrumento "
                                  "com biblioteca (MINIDRUM, CANIVETE).") });
    info.controls.push_back ({ u ("Instrumento"), u ("preset do bank"),
                               u ("Escolhe o preset (instrumento) do soundfont carregado "
                                  "na gaveta CANIVETE; some enquanto o bank carrega.") });
    info.controls.push_back ({ u ("BUSCAR"), u ("bot\u00e3o"),
                               u ("Revarre a pasta Documentos/Forja/Rack (gaveta "
                                  "HOSPEDADO).") });
    info.controls.push_back ({ u ("ARQUIVO"), u ("bot\u00e3o"),
                               u ("Abre um kit/soundfont fora da biblioteca, direto "
                                  "do disco (gavetas de instrumento).") });
    info.controls.push_back ({ u ("EDITAR"), u ("bot\u00e3o"),
                               u ("Abre a UI nativa do plugin (ou a lista gen\u00e9rica de "
                                  "par\u00e2metros) em janela separada.") });
    info.controls.push_back ({ u ("Entrada / Saida"), u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Trim de +-24 dB do barramento.") });
    info.controls.push_back ({ u ("Inverter fase \u00d8 R"), u ("ligado/desligado"),
                               u ("Inverte o canal direito na saida do rack.") });
    info.controls.push_back ({ "MIDI", u ("led"),
                               u ("Acende quando um bloco chega com MIDI da faixa para "
                                  "qualquer gaveta. O medidor de ENTRADA so mostra audio, "
                                  "entao ele e a pista de que o teclado esta entrando.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Compara\u00e7\u00e3o A/B com rampa de 10 ms.") });

    info.provenance.source = u ("Airwindows \u00b7 PurestWarm, Sweeten, PurestDrive, "
                                "IronOxide5, ButterComp2, Dynamics, Parametric, kStation, "
                                "kRockstar2, TapeDelay2, Mojo, Dyno, Slew3, FinalClip, "
                                "Baxandall3, SmoothEQ2, Pop2, Pressure5, DeBess, ToTape9, "
                                "DeRez5, Echojam");
    info.provenance.author = "Chris Johnson (Airwindows)";
    info.provenance.license = u ("MIT. O c\u00f3digo original pode ser usado, copiado, "
                                 "modificado e distribu\u00eddo mantendo o aviso de copyright.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("Adaptadores reutilizados sem alteracao de timbre. O rack "
                               "e a camada Forja de cadeia, chaves de slot e master; nao ha "
                               "paridade do rack em si, apenas dos modulos individuais.");

    return info;
}

} // namespace forjarack
