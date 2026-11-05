# Trens de release da Forja

A suíte inteira e cada produto saem do **mesmo repositório**, por tags diferentes. Isso
existe para você gravar um vídeo de um produto e publicar aquele instalador sem ter de
republicar a suíte inteira — e sem que uma mexida no `forja_ui` quebre um produto escondido.

## Os dois trens

| Tag | Trem | O que empacota |
| --- | --- | --- |
| `v0.5.2` | suíte | os 36 plugins, num pacote só |
| `vrack-1.0.0` | produto | só o ForjaRack |
| `vminis-1.0.0` | produto | os três Minis (CANIVETE, MINIDRUM, MINISAT) |
| `vcanivete-1.0.0`, `vminidrum-1.0.0`, `vminisat-1.0.0` | produto | um mini específico |
| `vdrumforge-1.0.0` | produto | só o Drumforge |
| `vairwindows-1.0.0` | produto | os 31 adaptadores |

**Regra**: tag de produto **não** bumpa a versão da suíte. `vrack-1.0.0` não mexe em
`v0.5.x`; cada linha tem sua própria série numérica. E o contrário também vale: a suíte
pode sair várias vezes enquanto o Rack continua na 1.0.0.

O que **não** muda entre os trens: o `.github/workflows/release.yml` configura, compila e
roda os 37 testes inteiros no **Windows e no Linux**, em todo tag. Só o *staging* e o
empacotamento são filtrados. Um trem de produto é uma seleção de payload, nunca um subset de
build. A única exceção é o macOS, por limite físico do runner — veja
[O teto do runner de macOS](#o-teto-do-runner-de-macos).

## Registro único: `ci/products.json`

Fonte de verdade para o job `resolve` da CI, para o coletor de payload e para as URLs de
download do site. Campos por produto:

- `display` — nome do produto (aparece no instalador e na Release).
- `kind` — `"suite"` ou `"product"`. Só a suíte é `"suite"`.
- `artifactBase` — prefixo do nome do artefato (`ForjaRack`, `ForjaMinis`, …).
- `tagPrefix` — o prefixo que ativa o trem (`vrack-`). **É o contrato público**: é
  *append-only*, nunca se reordena nem se renomeia um prefixo já publicado.
- `versionFrom` — versão em que a linha está hoje. Informativo: a versão real vem do tag.
- `pathFilters` — prefixos de caminho passados ao `collect_payload.ps1 -OnlyPath`.
- `testRegex` — quais testes pertencem à linha. No Windows e no Linux é documentação (a CI
  roda todos); no macOS é **contrato**: o job compila só os alvos `forja_<nome do teste>`
  que esse regex seleciona. O `ci/test_resolve.ps1` confere se cada nome selecionado tem
  alvo correspondente no `Tests/CMakeLists.txt`.
- `readme` — o LEIA-ME que vai dentro do pacote do produto.

Três armadilhas registradas no cabeçalho do arquivo:

1. `testRegex` casa com o **nome do teste no ctest** (`canivete`, `forjarack_engine`,
   `parity_cinzel`, `alloc_guard`), não com o target CMake (`forja_canivete`).
2. `pathFilters` de três segmentos (`Minis/Canivete/Canivete`) fazem o coletor agrupar o
   payload pelo **nome do plugin**. Esse grupo precisa de um bloco
   `#if defined(ForjaColhido_Canivete)` no `installer/forja.iss` (nos dois: `[Components]`
   e `[Files]`), senão o bundle é coletado mas não instalado. O `ci/test_resolve.ps1` pega.
3. Toda chave de `ci/products.json` precisa ser um **objeto**, exceto as de comentário,
   que começam com `"_"` e carregam **texto**. O `jq` do job `resolve` indexa
   `.value.kind` em toda chave e morre num valor não-objeto — com `set -e`, o job inteiro
   cai. O `ci/test_resolve.ps1` confere a forma antes.

## Como taggar um produto

```powershell
git tag vrack-1.0.0
git push origin vrack-1.0.0
```

A CI resolve o trem, compila tudo, testa tudo e publica a Release do tag com os artefatos.
Para a suíte, o procedimento é o mesmo com `vX.Y.Z`.

**Um trem de cada vez.** O `release.yml` declara `concurrency: group: release-trains` com
`cancel-in-progress: false`: se duas ou mais tags de produto saem no mesmo minuto, o segundo
trem entra na **fila** atrás do primeiro em vez de rodar em paralelo. Motivo medido: em
04/11/2026 três tags de produto (minis, drumforge, piano) saíram com 30 s de diferença, os
três jobs de macOS foram cancelados juntos aos ~15 min (o resto do trem estava verde) e
nenhum Release publicou. Tags que apontam para commits **anteriores** à serialização ainda
rodam em paralelo se re-triggeradas — ao retaggar, use sempre o `main` atual.

Antes de taggar, confira o resolve e o instalador localmente, com os mesmos comandos da CI:

```powershell
powershell -ExecutionPolicy Bypass -File ci/test_resolve.ps1

# trem da suíte
powershell -ExecutionPolicy Bypass -File installer/collect_payload.ps1 -BuildRoot build -Clean

# trem de um produto: os pathFilters e o readme vêm de ci/products.json
powershell -ExecutionPolicy Bypass -File installer/collect_payload.ps1 -BuildRoot build `
  -OnlyPath Forjarack -LeiamE Forjarack/Forjarack/README.md -Clean

# o winget instala em escopo de usuário; o caminho de escopo de máquina é o outro
$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
          "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") |
  Where-Object { Test-Path $_ } | Select-Object -First 1
& $iscc /DAppVersion=vrack-1.0.0 /DProductName=ForjaRack /DProductVersion=1.0.0 installer/forja.iss
```

O `ISCC` **não** está na máquina por padrão (`winget install --id JRSoftware.InnoSetup`) e a
CI não avisa antes de 20 minutos: foi assim que um `Flags:` inexistente em `[Types]` derrubou
tanto o run de `v0.5.1` quanto o primeiro `vrack-1.0.0`. O mesmo vale para o compilador
cruzado: o workflow **Smoke** (`.github/workflows/smoke.yml`) compila e **liga**, com o GCC do
mesmo runner da CI de release, o Cinzel (puxa o `forja_ui` inteiro), os cinco produtos Forja
e o `forja_alloc_guard` (que inclui os 31 adaptadores num TU só). Dispara em push em
`Forja/Source/**`, `Forja/CMakeLists.txt`, `Airwindows/Eq/Cinzel/**`, `Forjarack/**`,
`Minis/**`, `drumforge/**`, `cmake/**` e `CMakeLists.txt`.

## Empacotamento por plugin dentro de um trem (`packages`)

Um trem de linha (ex.: `vminis-1.0.0`) normalmente empacota **um** `.zip` por SO e **um**
instalador Windows, todos agregados. Quando a linha tem plugins independentes e o público
pede download granular, o trem pode declarar `packages` em `ci/products.json`:

```json
"minis": {
  "kind": "product",
  "pathFilters": ["Minis"],
  "testRegex": "^(canivete|minidrum|minisat)$",
  "packages": [
    { "slug": "canivete", "display": "CANIVETE", "artifactBase": "ForjaCanivete",
      "pathFilter": "Minis/Canivete/Canivete", "testRegex": "^canivete$",
      "readme": "Minis/Canivete/README.md" },
    { "slug": "minidrum", "display": "MINIDRUM", "artifactBase": "ForjaMinidrum",
      "pathFilter": "Minis/Minidrum/Minidrum", "testRegex": "^minidrum$",
      "readme": "Minis/Minidrum/README.md" },
    { "slug": "minisat",  "display": "MINISAT",  "artifactBase": "ForjaMinisat",
      "pathFilter": "Minis/Minisat/Minisat",    "testRegex": "^minisat$",
      "readme": "Minis/Minisat/README.md" }
  ]
}
```

Com `packages` presente:

- **Build e testes continuam integrais** no Windows e no Linux (`pathFilters`/`testRegex`
  do trem-pai), e o macOS só compila os `pathFilters` do trem-pai — nada muda na regra do
  teto.
- O `resolve` emite o output `packages` como JSON array. Vazio ou ausente é `[]`, e nesse
  caso o comportamento antigo (aggregate) continua intacto.
- O Windows faz **um `collect_payload` por package** (com o `pathFilter` e o `readme` do
  package), congela cada payload em `installer/staging/<slug>/payload`, roda **um ISCC por
  package** e renomeia o `.exe` para `<pkg.artifactBase>-<versão>-windows-x64.exe`. Depois
  gera **um `.zip` por package** com o `VST3/` achatado daquele staging e o `LEIA-ME.txt`
  correspondente. **Sem aggregate** quando `packages` está presente.
- Linux e macOS espelham: um zip `<pkg.artifactBase>-<versão>-<so>.zip` por package.
- O job `release` anexa **todos os assets na mesma tag**. A página da Release fica com N
  pares `.zip`/`.exe` em vez de um pacote grande; os cards do site continuam apontando para
  `/releases/tag/vminis-<v>` e o usuário escolhe o arquivo do SO dele.

Contrato por package (o `ci/test_resolve.ps1` reprova antes de taggear):

- `slug` precisa existir como item em `site/catalogo.json` (é o plugin que o card mostra).
- `pathFilter` é **um** caminho de três segmentos (`Base/Família/Plugin`) coberto por um dos
  `pathFilters` do trem-pai; o coletor agrupa pelo último segmento e o `installer/forja.iss`
  precisa ter o bloco `#if defined(ForjaColhido_<Plugin>)` correspondente nos dois lados
  (`[Components]` e `[Files]`).
- `testRegex` casa com pelo menos um nome de teste do ctest, e cada nome tem
  `forja_<nome>` em `Tests/CMakeLists.txt` (regra do macOS).
- `artifactBase` é ASCII sem espaços e começa com `Forja` (o glob do upload é `Forja*.zip` /
  `Forja*.exe`).
- `readme` aponta para um arquivo existente no repo (o `.md` do próprio plugin é o normal).

Quando **não** usar `packages`:

- O produto é um só (Rack, Drumforge, Traco, Pianoforge): um zip já é granular o bastante.
- A família Airwindows inteira: são 31 plugins que evoluem juntos; empacotar um por um
  multiplicaria os assets por SO sem ganho real pro usuário (o instalador da suíte já tem
  `[Components]`). Se um dia a linha quiser fatiar por família, dá pra adicionar `packages`
  com granularidade de família em vez de plugin — a mesma machinery serve.

Adicionar um package novo é append-only dentro do array do trem-pai (a ordem não é contrato
público, mas manter a mesma ordem do `site/catalogo.json` ajuda leitura). Não precisa mexer
no `forja.iss` se o grupo `<Plugin>` do `pathFilter` já tiver bloco — que é o caso dos três
minis hoje.

## O teto do runner de macOS

Medido em sonda (`workflow_dispatch` de cronômetro, runner gratuito de macOS, build
universal `arm64;x86_64`, `FORJA_LTO=OFF`):

| Etapa | Tempo |
| --- | --- |
| Configure (FetchContent do JUCE incluso) | 71 s |
| ForjaRack (subtree `build/Forjarack`) | 37 min |
| Três plugins pequenos (Cinzel, Torno, Lima) | 24 min (~8 min por plugin) |
| Os quatro testes do trem do rack (compilar + rodar) | 19 min |

Ou seja: **um trem de produto fecha em ~1 h**, mas os 36 plugins + os 37 testes passam folgado
das 6 h de teto de job do runner — e era isso que derrubava o trem inteiro, com o job
aparecendo como `cancelled` e sem dizer nada (tentativas 3, 5 e 6). Por isso o job de macOS
compila apenas os `pathFilters` do trem e roda apenas os testes do `testRegex`. Enquanto o
fatiamento não chegar, **um tag de suíte `vX.Y.Z` para no step de build do macOS com a
mensagem explicando o teto**; os pacotes de Windows e Linux continuam completos.

Faltas registradas:

1. **Fatiar o job de macOS** em shards por família (matriz de build + um job de
   empacotamento que junta os artefatos). Só com isso o tag de suíte `vX.Y.Z` e o
   `vairwindows-` voltam a sair para macOS.
2. A sonda que produziu os números acima foi removida depois de medir. Precisando de número
   novo, recrie um workflow de `workflow_dispatch` com cronômetro em vez de adivinhar.

## Ver um trem falhar

O log bruto de job de repositório público exige sessão (a página diz "Sign in to view logs"
e a API de log responde 403), então quem está fora da sessão só enxerga as **anotações** do
check-run. Os jobs de release publicam nelas o trecho do log que falhou, pelo action
compartilhado `.github/actions/diagnostico` (`logs:` com os arquivos de log candidatos);
`build-smoke.log`, `build-linux.log`, `test-linux.log`, `build-macos.log` e
`test-macos.log` são gravados por `tee` exatamente para isso. No Windows, o coletor e o ISCC
publicam a exceção e a linha de erro do `.iss` no mesmo canal.

As etapas de Windows rodam em `shell: powershell` (Windows PowerShell 5.1) e **não** em
`pwsh`: é o shell com que o `collect_payload.ps1` foi validado localmente, o script declara
`#Requires -Version 5.1`, e o pwsh 7 do runner reprovou a etapa de staging uma vez.

## O que sai em cada artefato

Nomes (`artifactBase` + versão resolvida do tag):

| Trem | Windows | macOS / Linux |
| --- | --- | --- |
| suíte | `Forja-<tag>-windows-x64.exe` + `.zip` | `Forja-<tag>-macos-universal.zip`, `Forja-<tag>-linux-x64.zip` |
| produto (sem `packages`) | `Forja<Produto>-<x.y.z>-windows-x64.exe` + `.zip` | `Forja<Produto>-<x.y.z>-<so>.zip` |
| produto (com `packages`) | um par `.exe` + `.zip` **por package**, todos na mesma tag (`ForjaCanivete-<x.y.z>-...`, `ForjaMinidrum-<x.y.z>-...`, `ForjaMinisat-<x.y.z>-...`) | um `.zip` **por package** por SO (`ForjaCanivete-<x.y.z>-macos-universal.zip`, etc.) |

O `.exe` é o instalador Inno Setup e só existe no Windows; o `.zip` é a pasta `VST3/`
solta com o LEIA-ME. No produto, o LEIA-ME é o `readme` do registro (ex.:
`Forjarack/Forjarack/README.md`); na suíte, são os `docs/instalar/INSTALAR-*.txt`.

O instalador de produto traz **um** componente só (a única família coletada) porque os
`[Components]`/`[Files]` são montados a partir do manifest que o coletor escreve:
`installer/payload/forja-payload.txt` (uma família por linha) e o espelho ISPP
`installer/payload/forja-payload.isl` (um `#define ForjaColhido_<Família>` por família, por
linha). Família sem bundle coletado não vira componente: componente vazio é impossível por
construção. O `.isl` existe porque o ISPP não tem como iterar um arquivo de texto —
`FileRead` só lê a primeira linha, `#if` não aceita `IsDefined("..." + F)` e `#sub` não
aceita `\N` dentro de chamada de macro.

## Reempacotar sem retagar

`Actions → Release suite → Run workflow` e informe `ref` (ex.: `vrack-1.0.0`). O job
`release` só publica quando o evento é um push de tag, então um `workflow_dispatch` valida
os artefatos no aba de artifacts da execução sem tocar na Release existente.

Empacotamento local, para conferir antes de subir:

```powershell
powershell -ExecutionPolicy Bypass -File installer/collect_payload.ps1 -BuildRoot build -Clean -OnlyPath Forjarack -LeiamE Forjarack/Forjarack/README.md
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /DProductName=ForjaRack /DProductVersion=1.0.0 installer/forja.iss
```

(o `collect_payload.ps1` precisa ser chamado **antes** do `ISCC`: sem o `forja-payload.isl`
o `.iss` aborta com `#error` explícito).

## Liberar aos poucos no site

O `site/catalogo.json` controla o que o público vê, sem tocar no HTML:

- `"estado": "publicado"` — card com captura e botão de download.
- `"estado": "video"` — igual, mais o link "Ver vídeo" (`"video": "<url>"`).
- `"estado": "breve"` — sem download e sem link de release.

Ao publicar um produto, mude o `estado` (e preencha `video` quando o vídeo existir), rode
`tools/build_site.ps1` e `tools/validate_site.ps1`, e commite o `site/index.html` gerado. O
`pages.yml` publica no GitHub Pages. O botão de download aponta para a Release mais recente
do trem correspondente (`releases/latest` para a suíte; a tag `vrack-*` mais nova para o
Rack, e assim por diante).

## Estado das séries

| Linha | Série | Última versão publicada |
| --- | --- | --- |
| suíte | `vX.Y.Z` | `v0.5.1` |
| ForjaRack | `vrack-X.Y.Z` | `vrack-1.0.5` (04/11/2026, 4 assets) |
| Minis e cada mini | `vminis-`, `vcanivete-`, `vminidrum-`, `vminisat-` | — (`vminis-1.0.3` morreu no macOS em fila; retaggar do main) |
| Drumforge | `vdrumforge-` | — (`vdrumforge-1.0.3` idem) |
| Pianos | `vpiano-` | — (`vpiano-1.0.3` idem) |
| Airwindows | `vairwindows-` | — |
