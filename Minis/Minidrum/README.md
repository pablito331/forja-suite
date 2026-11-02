# MINIDRUM — tocador de kits do Drumforge (Forja)

Mini da base `Minis/`. Formato compacto (560 × 260), fachada de instrumento, e funciona
como gaveta de instrumento dentro do FORJARACK.

## Estado

| Item | Estado |
| --- | --- |
| Paridade com vendor | Não se aplica: o DSP é o engine do DRUMFORGE, já testado |
| Teste próprio | `forja_minidrum` (carga/adoção de kit, volume, premix, bypass, medidor, gaveta) |
| Guarda de alocação | Coberta pelo teste: zero alocação no caminho de áudio |
| Latência | 0, medida por impulso no harness do Drumforge |
| Build | VST3 (Windows, macOS, Linux) |

## Parâmetros

| Nome | Tipo | Faixa | Default | Nota |
| --- | --- | --- | --- | --- |
| `volume` | float | 0–1, passo 0,01 | 0,50 | nível de saída sobre o premix do kit |
| `bypass` | bool | — | `false` | crossfade de 10 ms, vozes preservadas |

O kit não é parâmetro de automação: ele é escolhido na UI por um botão que lê a biblioteca
compartilhada e o caminho fica no estado do plugin.

## DSP

O **engine do DRUMFORGE é reutilizado como bloco** — nada de fork. O mini toca um kit da
biblioteca (`Documentos/Forja/Drumforge/Kits`) sem abrir o sampler completo: pads,
camadas de velocity, round-robin e choke continuam no engine; o mixer de 16 canais não
aparece, porque **o kit já carrega a própria mix** gravada no arquivo (`.dfkit`).

Entrada: nenhuma (instrumento). Saída: estéreo. MIDI do host toca no frame exato.

## Interface

Um botão para puxar o kit da biblioteca do Drumforge, a foto do kit e o knob de VOLUME,
sobre o chrome Forja de sempre (topo, barra de status, FUNDAMENTOS). Redimensionável com
aspecto fixo.

## Testes

`forja_minidrum` cobre carga e adoção de kit, volume, premix do kit, latência 0 por
impulso, rampa de bypass, medidor, troca de kit com reap do anterior, ausência de alocação
no áudio e o registro da gaveta de instrumento no FORJARACK.

## Licença e créditos

Tudo aqui é Forja, MIT: o engine vem do DRUMFORGE (DSP próprio da suíte), sem vendor
Airwindows neste plugin. Os blocos de efeito do master do DRUMFORGE, quando usados, carregam
seus próprios créditos MIT nos READMEs dos modelos.
