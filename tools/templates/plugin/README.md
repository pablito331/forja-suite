# @@PRODUCT@@

Plugin VST3 + Standalone da suíte **Forja**. DSP adaptado do **Airwindows
@@AIRWINDOWS@@** (MIT, Chris Johnson), com interface nativa Forja e paridade
amostra a amostra contra o original.

## Papel

TODO: uma ou duas frases sobre onde usar e o que ele faz.

## Parâmetros

| Parâmetro | Faixa | Padrão | Função |
| --- | --- | --- | --- |
| Quantidade | 0,00–1,00 | 0,50 | TODO |
| Bypass | ligado/desligado | desligado | Comparação A/B com rampa de 10 ms |

## DSP

TODO: descreva o laço do original e o que foi transcrito. O adaptador fica em
`Forja/Source/forja/dsp/Forja@@MODEL@@.h`; o original vendorizado fica em
`Forja/Source/forja/vendor/airwindows/@@AIRWINDOWS@@/` e não é compilado no plugin.

## Latência

TODO: informe a latência **medida por impulso** no harness (44,1/48/96/192 kHz).

## Desvios do adaptador

- Estado por instância em vez de `static`.
- Bypass Forja com rampa de 10 ms.
- Medidores de entrada/saída e CLIP.
- TODO: liste qualquer camada Forja.

## Validação

- `Tests/Source/parity_@@SLUG@@.cpp` compara o original com o adaptador.
- `forja_alloc_guard` cobre o adapter.
- TODO: registre o resultado (casos, erro máximo, latência).

## Licença

DSP original: Airwindows @@AIRWINDOWS@@, © Chris Johnson, MIT. Interface e
adaptador: Forja, MIT.
