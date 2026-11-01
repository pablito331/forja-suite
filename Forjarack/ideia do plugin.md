Aqui está a compilação completa de todo o projeto do **ForjaRack**, reunindo a visão geral, a arquitetura de roteamento e a biblioteca expandida com todos os módulos de áudio (incluindo processadores de sinal geral, mixagem e masterização).

---

# PROJETO FORJARACK (Série 500 Modular DSP)

## 1. Conceito Geral

O **ForjaRack** é um ecossistema modular no formato rack estilo Série 500 virtual. Ele possui uma estrutura com 8 slots verticais intercambiáveis onde o usuário pode carregar, reordenar e combinar diversos módulos digitais baseados em algoritmos open-source (Airwindows, SAFE Project, Invada Audio, DISTRHO, Freeverb3, Cabbage e Pure Data).

A interface visual segue o padrão da linha **Forja**, com gavetas verticais dedicadas para cada slot, seletores físicos estilizados, knobs responsivos e meters LED.

---

## 2. Arquitetura de Roteamento e Funcionamento dos Slots

### Slots Intercambiáveis e Livres

* **Livre Escolha de Slots:** O rack pode rodar com 1 a 8 módulos carregados. Slots vazios utilizam um painel cego (*blank panel*) com circuito de *pass-through* transparente, garantindo consumo zero de CPU e latência zero.
* **Drag & Drop Reordenável:** Permite mover qualquer módulo de uma posição para outra em tempo real (ex: arrastar do Slot 2 para o Slot 6) utilizando crossfade curto de 5ms sem gerar estalos (*pop/click*).

### Modos da Matriz de Roteamento

* **Série (Padrão 500):** O sinal entra no Slot 1 e passa sequencialmente pelos slots ocupados até a saída principal.
* **Paralelo / Split (Dual Bus):** Divisão do sinal de entrada em dois caminhos paralelos (ex: Slots 1 a 4 processam o sinal limpo/Bus A e Slots 5 a 8 processam saturação/Bus B), somados no final com controle de blend.
* **Mid/Side Split:** Codificação M/S na entrada. Slots da esquerda processam o canal Mid (centro) e os da direita o canal Side (laterais), aplicando a decodificação M/S no barramento final.
* **Sidechain Cross-Routing:** Capacidade de rotear a saída de áudio de qualquer slot para servir como gatilho de *Sidechain* em outro slot da cadeia.

### Recursos Globais no Rack

* **Master Section:** Knobs de Input/Output Trim, Phase Invert, bypass global e medidor estéreo RMS/Peak.
* **Mini Signal Graph:** Painel retrátil no topo do rack exibindo o fluxo atual do áudio em blocos de 1 a 8 com LEDs de presença de sinal.
* **Controles por Slot:** Cada gaveta possui botões individuais de Power/Bypass, Mute e Solo.

---

## 3. Biblioteca Completa de Módulos (ForjaRack DSP)

### Preamp, Saturação e Coloração Analógica

* **Forja Warmth:** Baseado no Airwindows Console/Desk. Adiciona calor analógico, compressão de transientes e saturação sibilante suave.
* **Forja Tape Drive:** Baseado no Airwindows IronOxide / DISTRHO. Emulação de saturação de fita magnética com controle de velocidade da fita (IPS) e bias.
* **Forja Tube Amp:** Baseado no Airwindows Tube / Cabbage DSP. Saturação harmônica por válvulas para aquecer vocais e instrumentos acústicos.
* **Forja Clip Master:** Baseado no Airwindows Logical / DISTRHO. Soft-clipping analógico para controlar picos agressivos antes da compressão.
* **Forja Transformer:** Baseado no Airwindows Mojo. Emulação da resposta de frequência e saturação de transformadores de áudio vintage.

### Saturação, Excitadores e Coloração de Master

* **Forja Master Tape:** Baseado no Airwindows IronOxide / Tape. Emulação de gravador de fita de rolo estilo 1/2" e 2". Traz o clássico "glue", compressão de graves (tape head bump) e suavização do topo de agudos.
* **Forja Harmonic Exciter:** Baseado no Airwindows Excite / Pure Data DSP. Gerador de harmônicos pares e ímpares selecionáveis (válvula/transistor). Adiciona presença, clareza e brilho em frequências altas sem elevar o ganho de pico do áudio.
* **Forja Air Exciter:** Baseado no Airwindows Atmosphere / Slew. Focado exclusivamente na região acima de 10 kHz (Air band), gerando brilho limpo e tridimensionalidade para vocais e a mistura completa.
* **Forja Sub Transformer:** Baseado no Airwindows BassDrive / SubTight. Processador de sub-graves que gera ressonância harmônica em oitavas inferiores, dando a sensação de grave profundo e encorpado mesmo em sistemas de som menores.
* **Forja Vinyl/Console Dust:** Baseado no Airwindows Vinyl / Console. Adiciona saturação de console analógico de grande porte (estilo SSL 4000/Neve) para criar interação não-linear e diafonia (crosstalk) sutil nos canais.

### Compressão e Controle de Dinâmica

* **Forja VCA Comp:** Baseado no SAFE Compressor / Airwindows Pressure5. Compressor rápido estilo VCA (dbx/SSL), excelente para bumbo, caixa e barramentos.
* **Forja Opto Leveler:** Baseado no Invada Compressor / Airwindows ButterComp. Compressor óptico com ataque e soltura suaves, ideal para vocais e baixo.
* **Forja FET Monster:** Baseado no Airwindows Pyewacket / Freeverb3 DSP. Resposta ultra-rápida estilo 1176, perfeito para adicionar punch e atitude.
* **Forja Bus Glue:** Baseado no Airwindows Consolidated. Compressor de barramento de grupo (Bus) projetado para colar a mistura sem perder clareza.
* **Forja Peak Limiter:** Baseado no Airwindows Dynamics / DISTRHO. Brickwall limiter transparente para controle final de transientes no slot.

### Compressores de Master e Barramento (Bus Comp)

* **Forja Master Bus Comp:** Baseado no SAFE Compressor / Airwindows ButterComp. Compressor de barramento estilo SSL G-Master Bus. Entrega o clássico efeito de "cola" na mistura, com resposta de ataque selecionável, filtro de sidechain interno para corte de graves (HPF Sidechain) e controle de Blend (Mix paralelo).
* **Forja Vari-Mu Leveler:** Baseado no Airwindows Pressure / Pyewacket. Compressor estilo Fairchild / Manley Vari-Mu com ganho por válvulas. Comprime suavemente com taxa (ratio) variável de acordo com a intensidade do sinal de entrada.
* **Forja Parallel Bus Comp:** Baseado no Airwindows Logical. Compressor otimizado para processamento paralelo (New York Compression), permitindo esmagar o sinal sem destruir a dinâmica da faixa principal.

### Expansion, Noise Gate e De-Esser

* **Forja De-S:** Baseado no Airwindows DeEss / SAFE DeEsser. Atenuador sibilante inteligente que isola altas frequências sem achatar o agudo geral.
* **Forja Gate & Exp:** Baseado no Invada NoiseGate. Expander/Gate com filtros de sidechain internos para limpar vazamentos de bateria e ruídos de amp.
* **Forja Transient Shaper:** Baseado no Airwindows Density / Cabbage DSP. Ajustador de Attack e Sustain para esculpir o impacto de peças de percussão.

### Equalização e Filtros

* **Forja Parametric EQ:** Baseado no SAFE EQ / Invada ER-EQ. EQ paramétrico de 4 bandas completas com filtros High/Low Pass e Q ajustável.
* **Forja Air Filter:** Baseado no Airwindows Slew / ResEQ. EQ estilo High Shelf focado em adicionar presença de ar nos agudos sem trazer aspereza.
* **Forja Low Punch:** Baseado no Airwindows SubTight. Filtro ressonante focado na limpeza de sub-graves e reforço da região de peso (60Hz - 100Hz).
* **Forja Tilt EQ:** Baseado no Airwindows Tone / DISTRHO. Equalizador de balanço (Tilt) simples e musical para mudar o tom de escuro para brilhante com apenas um controle.
* **Forja Formant Filter:** Baseado em algoritmos do Pure Data / plugdata. Filtro de ressonância de formantes vocais para efeitos criativos.

### Equalização de Masterização e Correção Tonal

* **Forja Master Passive EQ:** Baseado no SAFE EQ / Invada ER-EQ. Equalizador de curvas passivas (estilo Pultec EQP-1A), ideal para dar "boost & attenuate" simultâneo nas frequências graves (30Hz/60Hz) e agudos (10kHz/12kHz) da master.
* **Forja Linear Phase EQ:** Baseado em algoritmos de Freeverb3 DSP / Cabbage. EQ paramétrico de fase linear que evita desvios de fase em frequências cruzadas, garantindo fidelidade e clareza perfeita no barramento principal.
* **Forja Mid/Side EQ:** Baseado no Airwindows M/S / Invada EQ. EQ de 3 bandas configurado internamente para processar o canal Mid (centro) e Side (laterais) de forma independente no mesmo slot.

### Delay e Efeitos de Tempo

* **Forja Tape Delay:** Baseado no Invada Delay. Delay estilo fita/analógico com filtro de feedback (damp) e saturação na repetição.
* **Forja Analog Echo:** Baseado no Airwindows BBD / DISTRHO. Delay estilo Bucket Brigade (BBD) com escurecimento progressivo do sinal atrasado.
* **Forja PingPong Delay:** Baseado no Freeverb3 core / Invada. Delay estéreo com alternância entre canais esquerdo e direito e sync de BPM.

### Reverb e Espacialização

* **Forja Space Plate:** Baseado no Dragonfly Reverb (Plate engine). Algoritmo de placa denso e brilhante, focado em caixas de bateria e vocais.
* **Forja Hall Acoustic:** Baseado no Dragonfly Hall / Freeverb3. Reverb de sala ampla com simulação de reflexões iniciais e decaimento natural.
* **Forja Ambience:** Baseado no Airwindows Galactic / Dragonfly Room. Reverb curto de espaço/sala para dar tridimensionalidade a instrumentos secos.
* **Forja Stereo Width:** Baseado no Airwindows Srsly / Invada Stereo. Processador M/S (Mid/Side) para ajuste de largura da imagem estéreo no rack.

### Imagem Estéreo e Espaciamento

* **Forja Stereo Field / Imager:** Baseado no Airwindows Srsly / Invada Stereo. Processador de largura estéreo com controle de convergência de graves (Monofrequência abaixo de 100 Hz para evitar problemas de fase em vinil ou sistemas mono).
* **Forja Center Recover:** Baseado no Airwindows MidSide. Ajusta o equilíbrio de energia entre o fantasma central (vocal principal, bumbo, baixo) e a ambientação lateral da música.

### Utilitários e Modulação

* **Forja Chorus / Flanger:** Baseado no Invada Chorus. Modulação analógica clássica de dupla voz para dar corpo a guitarras, violões e teclados.
* **Forja Auto-Wah / Envelope:** Baseado no Airwindows AutoWah / Pure Data core. Filtro envelope follower para baixo e guitarra funk.
* **Forja Phase Alignment:** Baseado no Airwindows Infrasonic / Phase. Ajuste fino de fase contínuo de 0 a 180 graus para alinhar microfones duplos.

### Limiting, Clipping e Finalização de Master

* **Forja Master Peak Clipper:** Baseado no Airwindows Natural / Clip. Soft-clipper transparente para raspar 1 a 2 dB de picos transientes ultra-rápidos antes de atingir o limiter, mantendo a dinâmica perceptível intacta.
* **Forja Brickwall Limiter:** Baseado no Airwindows Dynamics / Freeverb3 Limiter. Limiter de pico real (True Peak Limiter) para garantir que a master não ultrapasse o teto digital (CEILING de -0.1 dB a -1.0 dBFS) e evitar distorção de interamostra (ISPs).
* **Forja Dither & Wordlength:** Baseado no Airwindows Dither (DeRez / StudioDither). Algoritmo de redução de bits (24-bit para 16-bit) com dither mascarado para exportação final de áudio.

---

## 4. Exemplo de Cadeia Recomendada para Masterização

1. **Slot 1:** Forja Master Passive EQ *(Ajuste tonal geral de graves e agudos)*
2. **Slot 2:** Forja Master Bus Comp *(Compressão de cola suave 1.5:1 a 2:1 com HPF Sidechain)*
3. **Slot 3:** Forja Master Tape *(Saturação de fita para colar e aquecer a resposta)*
4. **Slot 4:** Forja Harmonic Exciter *(Presença e brilho harmônico no topo)*
5. **Slot 5:** Forja Stereo Field *(Mono nos graves + abertura estéreo nos médios/agudos)*
6. **Slot 6:** Forja Master Peak Clipper *(Controle dos picos mais agressivos)*
7. **Slot 7:** Forja Brickwall Limiter *(Ganho final e teto True Peak)*
8. **Slot 8:** Forja Dither *(Tratamento final de quantização de bits)*