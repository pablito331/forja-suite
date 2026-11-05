; ============================================================
;  Forja - instalador Windows (VST3)
;
;  Serve os dois trens de release a partir do MESMO arquivo:
;    suite         (tag vX.Y.Z)      -> payload com todas as familias
;    produto       (tag vrack-1.0.0) -> payload filtrado por
;                                       collect_payload.ps1 -OnlyPath
;
;  O que entra em [Components] e [Files] e decidido pelo manifest que o coletor
;  escreve (payload\forja-payload.isl): so as familias com bundle coletado viram
;  linha. Nada de lista duplicada entre aqui e o coletor, e nada de componente
;  vazio.
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
;  O coletor escreve payload\forja-payload.isl com um
;  #define ForjaColhido_<Familia> para cada familia com bundle coletado, e o
;  instalador so oferece o que de fato existe no payload -- componente vazio e
;  impossivel por construcao, inclusive no trem de um produto.
;
;  Porque o catalogo de familias vive em LINHAS LITERAIS aqui embaixo, e nao
;  num #for sobre uma string concatenada (as quatro armadilhas reais do ISPP
;  6.7, todas provadas no ISCC local):
;   - o separador de argumentos de macro e ",", entao o catalogo antigo com ";"
;     entregava a linha INTEIRA em \1 e \2/\3 vazios;
;   - dentro de #sub, uma referencia "\N" dentro de chamada de macro derruba o
;     parser com "Illegal character in input file: '\'";
;   - #if aceita apenas inteiro/booleano literal ou defined(NOME_LITERAL);
;     IsDefined("..." + F) e C(F) definido sobre expressao dao erro;
;   - FileRead so le a primeira linha do arquivo, entao parsear o .txt do
;     manifest seria pior que deixar o coletor falar ISPP.
;  Familia nova: soma-se um bloco em [Components] e um em [Files] aqui, e uma
;  linha em Get-ForjaFamily no coletor. O validate_site.ps1 confere os dois lados.
; ============================================================
#define PayloadInclude AddBackslash(SourcePath) + "payload\forja-payload.isl"

#if FileExists(PayloadInclude)
  #include PayloadInclude
#else
  #error O include do manifest nao existe. Rode installer/collect_payload.ps1 antes do ISCC.
#endif

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

; ISCC nao aceita continuacao de linha em entradas de secao: o backslash e o
; resto viram texto literal do Description. Vai tudo numa linha so.
[Tasks]
Name: "docs"; Description: "Atalho no menu Iniciar para a documentacao"; Flags: unchecked

[Types]
Name: "full"; Description: "Tudo - instala todas as familias disponiveis"
; Nao ha flag de tipo chamada "inspect": em [Types] as flags validas sao "full"
; e "custom" (implicitas no Name). O Flags: aca derrubava o ISCC da CI com
; "Parameter Flags includes an unknown flag".
Name: "custom"; Description: "Personalizada - escolha as familias"

[Components]
; Nome tecnico (ascii) e a descricao vista na arvore de selecao.
; Airwindows: nove familias.
#if defined(ForjaColhido_Amps)
Name: "amps"; Description: "Amps - amplificadores e cabinets"; Types: full custom
#endif
#if defined(ForjaColhido_Dynamics)
Name: "dynamics"; Description: "Dynamics - compressores e gates"; Types: full custom
#endif
#if defined(ForjaColhido_Eq)
Name: "eq"; Description: "EQ - equalizadores"; Types: full custom
#endif
#if defined(ForjaColhido_Lofi)
Name: "lofi"; Description: "Lo-Fi - degradacao criativa"; Types: full custom
#endif
#if defined(ForjaColhido_OneKnob)
Name: "oneknob"; Description: "OneKnob - um unico controle"; Types: full custom
#endif
#if defined(ForjaColhido_Saturation)
Name: "saturation"; Description: "Saturation - saturacao harmonica"; Types: full custom
#endif
#if defined(ForjaColhido_Space)
Name: "space"; Description: "Space - reverberacao e delay"; Types: full custom
#endif
#if defined(ForjaColhido_Strips)
Name: "strips"; Description: "Strips - tiras de canal (mixer)"; Types: full custom
#endif
#if defined(ForjaColhido_Tape)
Name: "tape"; Description: "Tape - simulacao de fita"; Types: full custom
#endif
; Bases Forja.
#if defined(ForjaColhido_Drumforge)
Name: "drumforge"; Description: "Drumforge - sampler de bateria"; Types: full custom
#endif
#if defined(ForjaColhido_Rack)
Name: "rack"; Description: "ForjaRack - rack de gavetas em serie"; Types: full custom
#endif
#if defined(ForjaColhido_Minis)
Name: "minis"; Description: "Minis - Canivete, Minidrum, Minisat"; Types: full custom
#endif
; Trens de plugin unico da base Minis: com -OnlyPath descendo ate a pasta do
; plugin (Minis/Canivete), o coletor agrupa pelo nome do plugin, nao pela base.
#if defined(ForjaColhido_Canivete)
Name: "canivete"; Description: "Canivete - leitor de soundfonts"; Types: full custom
#endif
#if defined(ForjaColhido_Minidrum)
Name: "minidrum"; Description: "Minidrum - toca-kit de bateria"; Types: full custom
#endif
#if defined(ForjaColhido_Minisat)
Name: "minisat"; Description: "Minisat - saturador de um knob"; Types: full custom
#endif
; Base Pianos.
#if defined(ForjaColhido_Pianos)
Name: "pianos"; Description: "Pianos - Pianoforge, piano de soundfont com corpo fisico"; Types: full custom
#endif
; Trem de plugin unico da base Pianos: com -OnlyPath descendo ate a pasta do
; plugin (Pianos/Pianoforge/Pianoforge), o coletor agrupa pelo nome do plugin.
#if defined(ForjaColhido_Pianoforge)
Name: "pianoforge"; Description: "Pianoforge - piano de soundfont com corpo fisico"; Types: full custom
#endif
; Base Traco (EQ grafico original Forja).
#if defined(ForjaColhido_Traco)
Name: "traco"; Description: "Traco - EQ grafico original Forja"; Types: full custom
#endif

[Files]
; Cada familia copia seus .vst3 direto para a raiz VST3 ({app}), achatando o
; agrupamento por familia feito no payload. recursesubdirs preserva a estrutura
; interna de cada bundle (Contents\...).
#if defined(ForjaColhido_Amps)
Source: "payload/Amps/*"; DestDir: "{app}"; Components: amps; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Dynamics)
Source: "payload/Dynamics/*"; DestDir: "{app}"; Components: dynamics; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Eq)
Source: "payload/Eq/*"; DestDir: "{app}"; Components: eq; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Lofi)
Source: "payload/Lofi/*"; DestDir: "{app}"; Components: lofi; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_OneKnob)
Source: "payload/OneKnob/*"; DestDir: "{app}"; Components: oneknob; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Saturation)
Source: "payload/Saturation/*"; DestDir: "{app}"; Components: saturation; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Space)
Source: "payload/Space/*"; DestDir: "{app}"; Components: space; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Strips)
Source: "payload/Strips/*"; DestDir: "{app}"; Components: strips; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Tape)
Source: "payload/Tape/*"; DestDir: "{app}"; Components: tape; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Drumforge)
Source: "payload/Drumforge/*"; DestDir: "{app}"; Components: drumforge; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Rack)
Source: "payload/Rack/*"; DestDir: "{app}"; Components: rack; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Minis)
Source: "payload/Minis/*"; DestDir: "{app}"; Components: minis; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Canivete)
Source: "payload/Canivete/*"; DestDir: "{app}"; Components: canivete; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Minidrum)
Source: "payload/Minidrum/*"; DestDir: "{app}"; Components: minidrum; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Minisat)
Source: "payload/Minisat/*"; DestDir: "{app}"; Components: minisat; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Pianos)
Source: "payload/Pianos/*"; DestDir: "{app}"; Components: pianos; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Pianoforge)
Source: "payload/Pianoforge/*"; DestDir: "{app}"; Components: pianoforge; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif
#if defined(ForjaColhido_Traco)
Source: "payload/Traco/*"; DestDir: "{app}"; Components: traco; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
#endif

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
Filename: "{commonappdata}\Forja\LEIA-ME.txt"; Description: "Abrir as instrucoes de uso na DAW"; Flags: postinstall nowait skipifsilent unchecked

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
      Log('Forja: nenhuma familia marcada; nada instalado alem da documentacao.')
  end;
end;
