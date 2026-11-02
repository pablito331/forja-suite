; ============================================================
;  Forja - instalador Windows (VST3)
;
;  Serve os dois trens de release a partir do MESMO arquivo:
;    suíte         (tag vX.Y.Z)      -> payload com todas as familias
;    produto       (tag vrack-1.0.0) -> payload filtrado por
;                                       collect_payload.ps1 -OnlyPath
;
;  O que entra em [Components] e [Files] e decidido pelo manifest que o coletor
;  escreve (payload\forja-payload.txt): so as familias com bundle coletado
;  viram componente. Nada de lista duplicada entre aqui e o coletor, e nada de
;  componente vazio.
;
;  Build local (com Inno Setup 6 instalado):
;    powershell -File installer/collect_payload.ps1 -Clean
;    iscc installer/forja.iss
;  Produto:
;    powershell -File installer/collect_payload.ps1 -Clean -OnlyPath Forjarack -LeiamE Forjarack/Forjarack/README.md
;    iscc /DProductName=ForjaRack /DProductVersion=1.0.0 installer/forja.iss
;  A CI faz isso no job Windows do release.yml.
; ============================================================

; Versao vem da CI (/DProductVersion=..., ou /DAppVersion= para a suite);
; localmente cai no padrao.
#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif

#ifdef ProductVersion
  #define EfetivoVersion ProductVersion
#else
  #define EfetivoVersion AppVersion
#endif

#ifdef ProductName
  #define EfetivoName ProductName
#else
  #define EfetivoName "Forja"
#endif

; ============================================================
;  ISPP: catalogo de grupos de payload e selecao pelo manifest do coletor.
;  Cada linha e (pasta do payload|componente ascii|descricao). O nome do
;  componente vive SO aqui; ao nascer uma familia/base nova, soma-se uma linha
;  aqui e uma em Get-ForjaFamily no coletor.
;
;  As tres ultimas linhas sao os tremens de plugin unico da base Minis: quando
;  -OnlyPath desce ate a pasta do plugin (Minis/Canivete/Canivete) o coletor
;  agrupa pelo nome do plugin, nao pela base. O validate_site.ps1 confere os
;  dois lados.
; ============================================================
#define FamilyCatalog \
  "Amps|amps|Amps - amplificadores e cabinets;" + \
  "Dynamics|dynamics|Dynamics - compressores e gates;" + \
  "Eq|eq|EQ - equalizadores;" + \
  "Lofi|lofi|Lo-Fi - degradacao criativa;" + \
  "OneKnob|oneknob|OneKnob - um unico controle;" + \
  "Saturation|saturation|Saturation - saturacao harmonica;" + \
  "Space|space|Space - reverberacao e delay;" + \
  "Strips|strips|Strips - tiras de canal (mixer);" + \
  "Tape|tape|Tape - simulacao de fita;" + \
  "Drumforge|drumforge|Drumforge - sampler de bateria;" + \
  "Rack|rack|ForjaRack - rack de gavetas em serie;" + \
  "Minis|minis|Minis - Canivete, Minidrum, Minisat;" + \
  "Canivete|canivete|Canivete - leitor de soundfonts;" + \
  "Minidrum|minidrum|Minidrum - toca-kit de bateria;" + \
  "Minisat|minisat|Minisat - saturador de um knob;"

#define PayloadManifest AddBackslash(SourcePath) + "payload\forja-payload.txt"

; O manifest traz uma familia por linha, cercada por quebras de linha; procurar
; o token cerca-do-mesmo-lado evita que "Minis" case com um nome futuro tipo
; "Minisat".
#define FamiliaColetada(str Token) \
  FileExists(PayloadManifest) && \
  (Pos("|" + Token + "|", "|" + StringChange(FileRead(PayloadManifest), Chr(13) + Chr(10), "|") + "|") > 0)

; #emit so aceita string literal: as duas linhas de secao viram macros que
; recebem os campos do catalogo e devolvem a linha ja formatada. Barras normais
; no Source: (o locator do Inno aceita) para evitar a fuga de contrabarra dentro
; da literal.
#define ComponentLine(str Comp, str Desc) \
  "Name: \"" + Comp + "\"; Description: \"" + Desc + "\"; Types: full custom"

#define FileLine(str Folder, str Comp) \
  "Source: \"payload/" + Folder + "/*\"; DestDir: \"{app}\"; Components: " + Comp + \
  "; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist"

[Setup]
AppName={#EfetivoName}
AppVersion={#EfetivoVersion}
AppVerName={#EfetivoName} {#EfetivoVersion}
AppPublisher=Forja
DefaultDirName={commonpf}\Common Files\VST3
DefaultGroupName=Forja
; O destino e a pasta de escaneamento VST3; nao deixar o usuario redirecionar
; para um lugar que a DAW nao acha.
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
; Escrever em Program Files pede elevacao; o UAC aparece automaticamente.
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#ifdef ProductName
OutputBaseFilename={#ProductName}-{#EfetivoVersion}-windows-x64
#else
OutputBaseFilename=Forja-{#EfetivoVersion}-windows-x64
#endif
OutputDir=dist
Compression=lzma2/max
SolidCompression=yes
UninstallDisplayName={#EfetivoName} {#EfetivoVersion} (VST3)
LicenseFile=..\LICENSE

[Tasks]
Name: "docs"; Description: "Atalho no menu Iniciar para a documentacao"; \
  Flags: unchecked

[Types]
Name: "full"; Description: "Tudo - instala todas as familias disponiveis"
Name: "custom"; Description: "Personalizada - escolha as familias"; Flags: inspect

[Components]
; Nome tecnico (ascii) e a descricao vista na arvore de selecao. So as familias
; presentes no manifest do coletor sao emitidas.
#sub EachFamily
  #if FamiliaColetada(\1)
    #emit ComponentLine(\2, \3)
  #endif
#endsub
#for {EachFamily FamilyCatalog}

[Files]
; Cada familia copia seus .vst3 direto para a raiz VST3 ({app}), achatando o
; agrupamento por familia feito no payload. recursesubdirs preserva a estrutura
; interna de cada bundle (Contents\...).
#sub EachFamilyFiles
  #if FamiliaColetada(\1)
    #emit FileLine(\1, \2)
  #endif
#endsub
#for {EachFamilyFiles FamilyCatalog}

; Licenca e LEIA-ME num lugar limpo (fora da raiz VST3), sempre junto. O
; LEIA-ME.txt vem do coletor: guia de install para a suite, README do produto
; quando o trem e de um produto so.
Source: "payload\LICENSE.txt";   DestDir: "{commonappdata}\Forja"; Flags: ignoreversion skipifsourcedoesntexist
Source: "payload\LEIA-ME.txt";   DestDir: "{commonappdata}\Forja"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\docs\instalar\INSTALAR-MACOS.txt";    DestDir: "{commonappdata}\Forja"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\docs\instalar\INSTALAR-LINUX.txt";    DestDir: "{commonappdata}\Forja"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\Documentacao {#EfetivoName}"; Filename: "{commonappdata}\Forja\LEIA-ME.txt"; Tasks: docs
Name: "{group}\Desinstalar {#EfetivoName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{commonappdata}\Forja\LEIA-ME.txt"; Description: "Abrir as instrucoes de uso na DAW"; \
  Flags: postinstall nowait skipifsilent unchecked

[Code]
// Guarda contra o instalador rodar sem nenhuma familia selecionada: sem .vst3
// instalado o usuario so ganharia um desinstalador vazio.
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectComponents then
  begin
    if WizardSelectedComponents(False) = '' then
    begin
      MsgBox('Escolha pelo menos uma familia para instalar.', mbInformation, MB_OK);
      Result := False;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if WizardSelectedComponents(False) <> '' then
      Log('Forja: ao abrir a DAW, refaca o escaneamento de plugins para os novos VST3 aparecerem.')
    else
      Log('Forja: nenhuma familia marcada; nada instalado alem da documentacao.');
  end;
end;
