# Airwindows Tube2 — código de referência

Arquivos originais do plugin **Tube2**, de Chris Johnson (Airwindows), mantidos
aqui sem modificações para rastrear a proveniência do DSP do BRASA.

- Repositório: https://github.com/airwindows/airwindows
- Caminho: `plugins/LinuxVST/src/Tube2/`
- Commit consultado: `b110fac3991fff7c4655cc2d25d6a3e221f9799c`
- Data da consulta: 2026-10-29
- Licença: MIT (ver `LICENSE` nesta pasta)

## Como o BRASA usa este código

O BRASA **não compila** estes arquivos. `Tube2.cpp` inclui `audioeffectx.h`
(SDK VST2), que não faz parte deste projeto. O adaptador
`Forja/Source/forja/dsp/ForjaBrasa.h` porta o laço de `processDoubleReplacing` para
`forja::BrasaDSP`, mantendo:

- o pad de entrada e o cálculo de `powerfactor` a partir de `Tube`;
- o modelador assimétrico (`sharpen`);
- as iterações de potência;
- a média simples em sample rates altos (`overallscale > 1.9`);
- a histerese/slew, o clamp em ±0,52 e o ganho de 1,923…;
- o dither de 32 bits com `frexpf` e os mesmos valores iniciais de `fpdL/fpdR`.

Diferenças conscientes em relação ao original:

- o estágio de trim de saída (−24…+24 dB) e o crossfade de bypass de 10 ms são
  estágios Forja posteriores à válvula, não fazem parte do Tube2;
- o adaptador mede peaks de entrada/saída e latches de clip para a UI;
- o adaptador processa buffers em vez de usar ponteiros VST2 e não faz parte da
  interface gráfica (o renderer é independente do DSP).
