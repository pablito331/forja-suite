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
roda os 37 testes inteiros em todo tag, nos três sistemas. Só o *staging* e o empacotamento
são filtrados. Um trem de produto é uma seleção de payload, nunca um subset de build.

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
- `testRegex` — quais testes pertencem à linha (documentação; a CI roda todos).
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
cruzado: o workflow **Smoke** (`.github/workflows/smoke.yml`) compila e **liga** um plugin com
o GCC do mesmo runner da CI de release em todo push em `Forja/Source/**`,
`Forja/CMakeLists.txt`, `Airwindows/Eq/Cinzel/**`, `cmake/**` e `CMakeLists.txt`.

## O que sai em cada artefato

Nomes (`artifactBase` + versão resolvida do tag):

| Trem | Windows | macOS / Linux |
| --- | --- | --- |
| suíte | `Forja-<tag>-windows-x64.exe` + `.zip` | `Forja-<tag>-macos-universal.zip`, `Forja-<tag>-linux-x64.zip` |
| produto | `Forja<Produto>-<x.y.z>-windows-x64.exe` + `.zip` | `Forja<Produto>-<x.y.z>-<so>.zip` |

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
| ForjaRack | `vrack-X.Y.Z` | — (sem tag) |
| Minis e cada mini | `vminis-`, `vcanivete-`, `vminidrum-`, `vminisat-` | — |
| Drumforge | `vdrumforge-` | — |
| Airwindows | `vairwindows-` | — |
