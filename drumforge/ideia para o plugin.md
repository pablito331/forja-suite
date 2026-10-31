Sensacional! Essa combinação de um kit visual em destaque, o mixer retrô no estilo Ciclotron (mas vestindo a pele escura com *ember* do ecossistema Forja) e o motor de DSP do Airwindows vai dar uma personalidade incrível para o **Drumforge**.

Aqui está o **Prompt Mestre Estruturado** e a **Especificação Técnica** para você usar na IDE ou colar nas tarefas de desenvolvimento do projeto:

---

# 🥁 PROMPT MESTRE / ESPECIFICAÇÃO TÉCNICA: DRUMFORGE

## 1. Visão Geral do Produto

O **Drumforge** é um plugin de bateria virtual (Sampler) focado em workflow ágil, forte apelo visual e sonoridade orgânica. O design segue rigorosamente o **Padrão de UI Forja** (tons `iron-0` a `iron-4`, acentos `ember` `#FF7A18` e tipografia técnica), combinando uma interface moderna com um mixer inspirado nas clássicas mesas analógicas **Ciclotron**.

---

## 2. Layout & Interface Visual (UI/UX - Padrão Forja)



### Área Central Superior (>50% da janela)

* **Kit Display:** Área visual de destaque exibindo a renderização/foto do kit de bateria ativo.
* **Interatividade nos Pads:** Cliques diretos nas peças da foto disparam a nota/sample correspondente para pré-escuta com velocidade ajustável por posição vertical no clique.

### Barra de Navegação Inferior (Abaixo do Display)

Botões de alternância estilo **Forja** para chavear os painéis do plugin:

1. `[ KIT / DISPLAY ]`: Retorna à visualização principal da bateria.
2. `[ MIXER ]`: Abre a mesa de som dinâmica no estilo Ciclotron.
3. `[ GROOVE BROWSER ]`: Abre o navegador de loops MIDI.
4. `[ SETTINGS / MAP ]`: Mapeamento de notas MIDI (GM Standard, Custom) e roteamento de saídas.

---

## 3. O Mixer Estilo "Ciclotron Forja"

Mesa de som modular analógica ajustada ao ecossistema Forja. O número de strips/canais é **dinâmico** e se adapta automaticamente à quantidade de peças do kit carregado (ex.: 6 peças = 6 faders + Master; 10 peças = 10 faders + Master).

### Arquitetura de cada Canal (Strip)

Cada canal possui a seguinte cadeia de sinal sequencial:

1. **Input Gain:** Controle de ganho de entrada.
2. **Compressor "One-Knob":** Compressão estilo mesa de som analógica, usando DSP responsivo e transparente baseado no ecossistema **Airwindows** (ex.: *ButterComp* ou *OneCompressor*).
3. **EQ Analógico de 3 Bandas:**
* **High:** Agudos (Shelf 12kHz).
* **Mid:** Médios (Peaking com frequência fixa estilo mesa de show).
* **Low:** Graves (Shelf 80Hz).


4. **Envio FX 1 (Reverb):** Send para o processador de espaço interno.
5. **Envio FX 2 (Delay/Saturation):** Send para a segunda máquina de efeitos.
6. **Pan:** Balanceamento estéreo.
7. **Solo / Mute / Phase Reverse:** Toggles em padrão Forja.


8. **Fader & Level Meter:** Fader vertical acompanhado de peak meter com coloração semântica (`safe` verde, `caution` âmbar e `clip` vermelho em `iron-0` background).



### Seção Master & Racks de Efeitos (FX 1 & FX 2)

* **FX 1 (Reverb/Room):** Máquina de convolução/algoritmo interno para ambiência do kit.
* **FX 2 (Saturation/Tape/Delay):** Processamento usando algoritmos **Airwindows** (ex.: *Console/Tape*).
* **Master Strip:** EQ Master, Compressor de Bus e Limiter Final + VU Meter Estéreo principal.



---

## 4. Groove Browser (Navegador de Loops MIDI)

* **Navegação & Organização:** Sistema de pastas por estilo, tempo (BPM) e assinatura de tempo (4/4, 6/8, etc.).
* **Player Interno:** Pré-escuta de loops MIDI sincronizada ao BPM da DAW.
* **Drag-and-Drop:** Arraste direto de arquivos `.mid` do plugin diretamente para a track MIDI da DAW.
* **Arquitetura Expansível:** Estrutura de pastas pronta para adição de novos pacotes de grooves (Packs de expansão MIDI).

---

## 5. Requisitos Técnicos & Arquitetura de Áudio (C++ / JUCE)

* **Formato & Plataforma:** Plugin **VST3 Cross-Platform** (Linux, Windows, macOS 64-bit) desenvolvido em C++ com o framework JUCE.
* **Engine de Sample:**
* Carregamento assíncrono de samples WAV (24-bit / 44.1kHz - 96kHz).
* Suporte a múltiplas camadas de velocidade (*velocity layers*) e articulações (*round-robin*).


* **Roteamento Áudio/MIDI:**
* Saída Master Estéreo ou **Multi-Out** (permite rotear cada peça para canais individuais da DAW).
* Mapeamento MIDI flexível (General MIDI por padrão).


* **DSP Integrado:** Algoritmos C++ do **Airwindows** adaptados para rodar de forma leve e sem latência adicional no processamento de sinal.

