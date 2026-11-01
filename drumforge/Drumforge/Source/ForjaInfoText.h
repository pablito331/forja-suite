#pragma once

#include "forja/ui/ForjaInfoContent.h"
#include "forja/ui/ForjaTheme.h"

namespace forja::drumforge
{

inline ui::ForjaInfo makeForjaInfo()
{
    const auto u = [] (const char* text) { return ui::utf8 (text); };

    ui::ForjaInfo info;
    info.title = "DRUMFORGE";
    info.subtitle = u ("SAMPLER DE BATERIA \u00b7 FORJA");
    info.tagline = u ("Dezesseis pads, camadas de velocidade, round-robin e uma mesa "
                      "completa: o kit entra, o groove sai. Sem lat\u00eancia e sem "
                      "depender de biblioteca externa.");

    info.sections.push_back ({
        u ("O que faz"),
        {
            u ("Drumforge \u00e9 um sampler de bateria de 16 pads fixos. Cada pad aceita "
               "camadas de velocidade e v\u00e1rias tomadas em round-robin, escolhidas "
               "pelo toque e giradas a cada golpe. O kit pode vir de um kit de texto do "
               "drumlabooh (drumkit.txt ou drumkit.labooh, que j\u00e1 trazem o mapa MIDI e "
               "as camadas), de uma pasta de "
               "samples (o plugin varre a pasta e as subpastas e classifica pelo nome "
               "do arquivo) ou de um arquivo .dfkit, texto no estilo drumlabooh que a "
               "pr\u00f3pria importa\u00e7\u00e3o grava na biblioteca."),
            u ("O \u00e1udio sai em cinco barramentos est\u00e9reo: MAIN e AUX 1 a 4. Cada pad "
               "escolhe para onde vai, o que permite gravar a bateria em canais "
               "separados na DAW mantendo o mix do plugin."),
            u ("A mesa tem um canal por pad: ganho de entrada, compressor de um "
               "controle, EQ de tr\u00eas bandas, pan, mute, solo, invers\u00e3o de fase, dois "
               "envios e fader com medidor. O master soma os retornos dos dois efeitos, "
               "aquece, comprime e limita."),
            u ("A aba LOOPS MIDI l\u00ea grooves .mid da biblioteca, desenha os golpes numa "
               "grade de 1/16 e toca o preview no BPM do host ou num BPM manual, sem "
               "depender da DAW para ouvir o loop.")
        }
    });

    info.sections.push_back ({
        u ("Como funciona"),
        {
            u ("O motor mant\u00e9m 64 vozes. Cada golpe pega uma camada pela velocity, "
               "avan\u00e7a o round-robin daquela camada e toca o sample com interpola\u00e7\u00e3o "
               "linear, corrigindo a taxa de amostragem do arquivo para a da sess\u00e3o. "
               "Quando todas as vozes est\u00e3o ocupadas, a mais antiga cede o lugar, "
               "ent\u00e3o toques r\u00e1pidos nunca ficam mudos."),
            u ("Grupos de choke desligam uma pe\u00e7a quando outra come\u00e7a: o chimbal aberto "
               "corta na mesma hora em que o fechado soa, com rampa de 5 ms. O "
               "fechamento usa um envelope curto por voz, sem alocar mem\u00f3ria."),
            u ("O MIDI \u00e9 dividido nos pontos exatos dos eventos, ent\u00e3o a pe\u00e7a come\u00e7a "
               "na amostra certa e n\u00e3o no in\u00edcio do bloco. O mapeamento padr\u00e3o segue "
               "a General MIDI (36 bumbo, 38 caixa, 42 chimbal fechado, 46 aberto, "
               "49 prato e assim por diante); pads sem nota ficam reservados para os "
               "sons de percuss\u00e3o importados."),
            u ("Os samples s\u00e3o lidos em uma thread de fundo e entregues prontos. O motor "
               "s\u00f3 v\u00ea o kit por um ponteiro somente leitura trocado por token, e o kit "
               "anterior s\u00f3 \u00e9 liberado depois que o \u00e1udio confirma a troca. Nada aloca "
               "no caminho do \u00e1udio.")
        }
    });

    info.sections.push_back ({
        u ("Loops MIDI"),
        {
            u ("A aba LOOPS l\u00ea arquivos .mid e .midi de Documentos/Forja/Drumforge/"
               "Loops; cada pasta ou subpasta vira categoria no filtro PACOTE, ent\u00e3o "
               "d\u00e1 para separar ritmos, fills e refr\u00f5es em subpastas (o filtro de "
               "uma m\u00e3e traz a \u00e1rvore inteira). O arquivo \u00e9 convertido "
               "para beats na leitura, ent\u00e3o o mesmo groove toca em qualquer BPM sem "
               "reescrever nada. O tempo do meta-evento define o BPM de origem e o loop "
               "fecha no compasso inteiro seguinte ao \u00faltimo golpe."),
            u ("O preview enfileira cada golpe com o deslocamento exato em amostras, no "
               "mesmo caminho do MIDI ao vivo. Com SYNC DAW ligado ele segue o BPM do host; "
               "desligado, usa o BPM manual de 40 a 240. No fim do loop o cursor volta ao "
               "in\u00edcio sem cortar as vozes que ainda est\u00e3o soando."),
            u ("A grade mostra uma faixa por pad que o arquivo realmente toca e uma coluna "
               "por 1/16, at\u00e9 quatro compassos. Golpes em notas fora do mapa do kit atual "
               "aparecem como aviso em vez de sumirem em sil\u00eancio; ARRASTAR MIDI entrega o "
               "arquivo selecionado para a DAW.")
        }
    });

    info.sections.push_back ({
        u ("Canal e master"),
        {
            u ("A cadeia de cada pad \u00e9 trim (ganho de entrada) \u2192 passa-altos \u2192 "
               "compressor \u2192 EQ de tr\u00eas bandas \u2192 pan \u2192 fader. O compressor "
               "\u00e9 est\u00e9reo-linkado, com joelho suave de 6 dB; o EQ usa um shelf de "
               "80 Hz, um sino em 1,2 kHz e um shelf de 8 kHz, todos fixos, cada um com "
               "ganho de \u221215 a +15 dB. Os envios s\u00e3o p\u00f3s-fader, como em uma mesa; "
               "como o SAL\u00c3O devolve 100% molhado, o SEND 1 dosa com curva quadrada "
               "e teto de \u221212 dB \u2014 um dedo no bot\u00e3o mal injeta sala, a sala cresce "
               "conforme o giro. O SEND 2 (satura\u00e7\u00e3o) segue linear porque ali o n\u00edvel "
               "\u00e9 drive, n\u00e3o dose de retorno."),
            u ("O master soma os envios, aplica o trim, o compressor ButterComp (bloco "
               "T\u00caMPERA, adaptado do Airwindows) e o limitador. O limitador \u00e9 camada "
               "Forja: teto de \u22120,3 dBFS, ataque instant\u00e2neo e retorno de 80 ms, sem "
               "lookahead e portanto sem lat\u00eancia adicional."),
            u ("FX 1 \u00e9 o SAL\u00c3O (kRockstar2), um espa\u00e7o est\u00e9reo com regen e posi\u00e7\u00e3o; "
               "FX 2 \u00e9 a LIXA (PurestDrive), satura\u00e7\u00e3o para dar corpo ao kit. Os dois "
               "s\u00e3o compartilhados: o quanto cada pad manda \u00e9 definido em SEND 1 e SEND 2. "
               "O SAL\u00c3O do master roda sem o piso de ru\u00eddo de sala do original, ent\u00e3o a "
               "mesa fica em sil\u00eancio com nada tocando. Todos os blocos de DSP t\u00eam "
               "lat\u00eancia 0.")
        }
    });

    info.sections.push_back ({
        u ("Os controles"),
        {
            u ("No canal: TRIM move a entrada de \u221224 a +24 dB; HPF abre um passa-altos "
               "de 20 a 400 Hz (em 0 fica desligado); LO, MID e HI s\u00e3o os tr\u00eas ganhos do "
               "EQ; THR e RATIO definem o compressor (threshold de \u221240 a 0 dB, raz\u00e3o de "
               "1:1 a 20:1); SEND 1 dosa o ambiente com curva quadrada (topo em \u221212 dB) "
               "e SEND 2 dosa a satura\u00e7\u00e3o; M, S e \u00d8 s\u00e3o mute, solo e "
               "fase; e o chip de sa\u00edda alterna entre MAIN e AUX 1 a 4."),
            u ("No master: TRIM, COMP (ButterComp, desligado em 0), LIMITER, FX 1 REGEN e "
               "FX 1 POS (tamanho e posi\u00e7\u00e3o do espa\u00e7o) e FX 2 DRIVE (satura\u00e7\u00e3o). "
               "O medidor est\u00e9reo mostra o pico de sa\u00edda; ao lado dele, dois rails "
               "verticais separam a atua\u00e7\u00e3o: C \u00e9 a redu\u00e7\u00e3o do compressor e L, a do "
               "limitador (cada um de 0 a \u221224 dB), com o total em dB abaixo."),
            u ("Os knobs da mesa t\u00eam cor por fun\u00e7\u00e3o, sempre dentro da paleta: "
               "ganho e din\u00e2mica em brasa, HPF em \u00e2mbar, o EQ em gradiente por faixa "
               "(graves verdes, medios cinza, agudos laranja do medidor), envios em "
               "branco e, no master, FX 1 em \u00e2mbar e FX 2 em laranja. Ao lado de cada "
               "fader, um rail vertical mostra a redu\u00e7\u00e3o de ganho do compressor do "
               "canal (0 a \u221224 dB)."),
            u ("Na aba KITS ficam a biblioteca, a importa\u00e7\u00e3o e o mapa dos 16 pads: "
               "clicar em um pad dispara o sample com velocity conforme a posi\u00e7\u00e3o "
               "vertical do clique. A aba MIXER mostra os 16 canais com medidor e "
               "redu\u00e7\u00e3o de ganho e, \u00e0 direita, o master com o medidor est\u00e9reo. A aba "
               "LOOPS traz a biblioteca de grooves, a grade e o transporte do preview.")
        }
    });

    info.sections.push_back ({
        u ("Opera\u00e7\u00e3o"),
        {
            u ("Para montar um kit: arraste uma pasta de samples para a aba KITS ou use "
               "IMPORTAR PASTA. O plugin procura .wav, .aif, .aiff e .flac, classifica "
               "pelo nome (kick, snare, tom, floor, hat, open, crash, ride, rim, clap) e "
               "l\u00ea os marcadores vl/v, vel, layer para camada de velocity e rr, rt, take "
               "para round-robin. Exemplo: Kick_vl2_rr1.wav e Kick_vl2_rr2.wav formam a "
               "camada 2 com duas tomadas."),
            u ("Kit de texto do drumlabooh: se a pasta tem um drumkit.txt (o formato do "
               "drumlabooh e dos kits do drum_sklad), ele manda. A linha "
               "[36]>Bass Drum=bass/b01.wav,bass/b02.wav declara a nota MIDI, o modo e as "
               "tomadas. Sem nota, o nome da pe\u00e7a escolhe o pad; a lista separada por "
               "v\u00edrgula \u00e9 lida da camada mais suave para a mais forte, e '>' e '*' "
               "viram round-robin. Um item pode apontar uma subpasta ou um .txt com um "
               "arquivo por linha, e o info.txt da pasta vira a descri\u00e7\u00e3o do kit. "
               "O drumkit.labooh (XML do drumlabooh, com um <sample> por pe\u00e7a) \u00e9 "
               "convertido nas mesmas linhas e lido igual; s\u00f3 o SFZ n\u00e3o \u00e9 lido."),
            u ("A importa\u00e7\u00e3o tamb\u00e9m grava um .dfkit na pasta Documentos/Forja/"
               "Drumforge/Kits, que passa a aparecer na biblioteca. O .dfkit \u00e9 texto no "
               "estilo drumlabooh, uma pe\u00e7a por linha com os marcadores Forja '@' (pad) "
               "e '~' (choke); um .dfkit antigo em XML ainda abre. ABRIR KIT carrega "
               "qualquer .dfkit, drumkit.txt ou drumkit.labooh de outra pasta; ATUALIZAR "
               "rel\u00ea a biblioteca. Kits do drumlabooh guardados nas pastas drum_sklad, "
               "drumlabooh-kits ou drumrox-kits da sua conta aparecem na lista sem "
               "precisar copiar nada. Se a pasta do kit tem a foto de conven\u00e7\u00e3o "
               "(image.png ou image.jpg), ela aparece como preview no card KIT."),
            u ("Kit premixado: o .dfkit pode trazer a mix junto, em linhas de coment\u00e1rio "
               "#mix@N e #master com os mesmos valores 0\u20131 dos knobs (trim, fader, pan, "
               "HPF, os tr\u00eas EQs, o compressor e os dois envios de cada pad, mais o "
               "master). Como s\u00e3o coment\u00e1rios '#', um leitor de kit do drumlabooh ignora "
               "essas linhas e continua lendo o kit normal; mute, solo, fase e a sa\u00edda "
               "n\u00e3o entram, porque s\u00e3o decis\u00e3o de sess\u00e3o. A mix s\u00f3 se aplica na carga "
               "manual: kit com mix assume os knobs, kit sem mix devolve a mesa ao neutro. "
               "Ao reabrir a sess\u00e3o, o estado que a DAW salvou vence a mix do arquivo, "
               "para n\u00e3o brigar com o preset e a automa\u00e7\u00e3o do projeto. O bot\u00e3o SALVAR "
               "KIT fecha o ciclo: com qualquer kit aberto, inclusive um vindo do "
               "drumlabooh ou de uma pasta de samples, ele captura a mix atual do mixer e "
               "cria um pacote port\u00e1vel \u2014 uma pasta com o nome do kit levando o .dfkit "
               "premixado, a subpasta samples/ com uma c\u00f3pia de cada \u00e1udio (sem "
               "duplicar quem se repete entre pads) e a foto normalizada como image.png ou "
               "image.jpg. Os caminhos ficam relativos \u00e0 pr\u00f3pria pasta, ent\u00e3o o pacote "
               "\u00e9 aut\u00f4nomo: pode ser movido ou enviado para outra m\u00e1quina que o kit abre "
               "sozinho, sem depender dos WAVs originais."),
            u ("Para o multi-out, habilite as sa\u00eddas AUX no canal do plugin na DAW e "
               "escolha o barramento no chip de sa\u00edda de cada pad. Duplo clique em "
               "qualquer controle devolve o padr\u00e3o; o bypass do host usa rampa de 10 ms "
               "e n\u00e3o corta as vozes em andamento."),
            u ("Para os loops: arraste .mid ou pastas inteiras para a aba LOOPS, ou use "
               "IMPORTAR MIDI \u2014 pastas entram preservando as subpastas de categoria. "
               "A busca e o filtro PACOTE reduzem a lista, e escolher uma linha carrega o "
               "loop no preview. ABRIR PASTA revela a biblioteca no sistema e ATUALIZAR "
               "rel\u00ea a pasta depois de copiar arquivos por fora.")
        }
    });

    info.controls.push_back ({ "Trim", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganho de entrada do canal, de \u221224 a +24 dB.") });
    info.controls.push_back ({ "Fader", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,91"),
                               u ("N\u00edvel de sa\u00edda, de \u221260 a +6 dB; 0,91 fica em 0 dB.") });
    info.controls.push_back ({ "Pan", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Posi\u00e7\u00e3o est\u00e9reo com lei de pot\u00eancia constante.") });
    info.controls.push_back ({ "HPF", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Passa-altos de 20 a 400 Hz; em 0 o filtro fica aberto.") });
    info.controls.push_back ({ "Lo, Mid, Hi", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50"),
                               u ("Ganhos do EQ em 80 Hz, 1,2 kHz e 8 kHz, de \u221215 a +15 dB.") });
    info.controls.push_back ({ "Thr / Ratio", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,70 / 0,10"),
                               u ("Threshold de \u221240 a 0 dB e raz\u00e3o de 1:1 a 20:1.") });
    info.controls.push_back ({ "Send 1 / Send 2", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("Envios p\u00f3s-fader para o espa\u00e7o (FX 1) e a satura\u00e7\u00e3o (FX 2).") });
    info.controls.push_back ({ u ("Sa\u00edda"), u ("MAIN, AUX 1 a 4 \u00b7 padr\u00e3o MAIN"),
                               u ("Barramento de destino do canal no multi-out.") });
    info.controls.push_back ({ "Comp do master", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,00"),
                               u ("ButterComp no bus; em 0 o compressor n\u00e3o processa.") });
    info.controls.push_back ({ "Limiter", u ("ligado/desligado \u00b7 padr\u00e3o ligado"),
                               u ("Teto de \u22120,3 dBFS sem lat\u00eancia.") });
    info.controls.push_back ({ "FX 1 Regen / Pos", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,50 / 1,00"),
                               u ("Quantidade e posi\u00e7\u00e3o do espa\u00e7o SAL\u00c3O.") });
    info.controls.push_back ({ "FX 2 Drive", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,20"),
                               u ("Satura\u00e7\u00e3o LIXA aplicada aos envios do FX 2.") });
    info.controls.push_back ({ "Loop Sync", u ("ligado/desligado \u00b7 padr\u00e3o ligado"),
                               u ("Faz o preview do loop seguir o BPM do host; desligado usa o BPM manual.") });
    info.controls.push_back ({ "Loop BPM", u ("0,00 a 1,00 \u00b7 padr\u00e3o 0,40 \u00b7 40 a 240 BPM"),
                               u ("BPM do preview quando o sync com a DAW est\u00e1 desligado.") });
    info.controls.push_back ({ "Bypass", u ("ligado/desligado \u00b7 padr\u00e3o desligado"),
                               u ("Silencia a sa\u00edda com rampa de 10 ms; as vozes continuam.") });

    info.provenance.source = "Forja DSP (engine de sampler) + blocos Airwindows adaptados "
                             "+ formato de kit do drumlabooh";
    info.provenance.author = "Forja; blocos de Chris Johnson (Airwindows); especifica\u00e7\u00e3o "
                             "de kit de Peter Semiletov (drumlabooh / drum_sklad)";
    info.provenance.license = u ("MIT nos blocos Airwindows: o c\u00f3digo original pode "
                                 "ser usado, copiado, modificado e distribu\u00eddo mantendo "
                                 "o aviso de copyright. O engine de sampler, a interface e "
                                 "os leitores de kit s\u00e3o da Forja.");
    info.provenance.commit = "airwindows master b110fac3991fff7c4655cc2d25d6a3e221f9799c";
    info.provenance.notes = u ("O engine de sampler, o mixer, o limiter e a interface "
                               "s\u00e3o originais da Forja. Os efeitos reutilizam adaptadores "
                               "j\u00e1 validados da su\u00edte: T\u00caMPERA (ButterComp2), SAL\u00c3O "
                               "(kRockstar2) e LIXA (PurestDrive). Os leitores de "
                               "drumkit.txt e drumkit.labooh seguem a especifica\u00e7\u00e3o de "
                               "formato do projeto drumlabooh / drum_sklad, de Peter "
                               "Semiletov; nenhum c\u00f3digo de l\u00e1 entra no plugin, s\u00f3 lemos "
                               "o formato. Nenhum sample ou biblioteca acompanha o plugin.");

    return info;
}

} // namespace forja::drumforge
