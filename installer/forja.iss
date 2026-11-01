; ============================================================
;  Forja - instalador Windows (VST3)
;  Gerado para a suuite: escolhe quais familias instalar.
;
;  Compoe um componente por familia a partir das pastas que
;  installer/collect_payload.ps1 montou em installer/payload/<Familia>/.
;  Cada familia e append-only: ao nascer uma famlia/base nova, some
;  uma linha em [Components] e uma em [Files] aqui (mesma lista que o
;  coletor imprime ao terminar).
;
;  Build local (com Inno Setup 6 instalado):
;    powershell -File installer/collect_payload.ps1 -Clean
;    iscc installer/forja.iss
;  A CI faz isso no job Windows do release.yml.
; ============================================================

; Versao vem da CI (/DAppVersion=vX.Y.Z); localmente cai no padrao.
#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif

[Setup]
AppName=Forja
AppVersion={#AppVersion}
AppVerName=Forja {#AppVersion}
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
OutputBaseFilename=Forja-{#AppVersion}-windows-x64
OutputDir=dist
Compression=lzma2/max
SolidCompression=yes
UninstallDisplayName=Forja {#AppVersion} (suite de plugins VST3)
LicenseFile=..\LICENSE

[Tasks]
Name: "docs"; Description: "Atalho no menu Iniciar para a documentacao da suite"; \
  Flags: unchecked

[Types]
Name: "full"; Description: "Tudo - instala todas as familias"
Name: "custom"; Description: "Personalizada - escolha as familias"; Flags: inspect

[Components]
; Nome tecnico (ascii) e a descricao vista na arvore de selecao.
Name: "amps";        Description: "Amps - amplificadores e cabinets";       Types: full custom
Name: "dynamics";    Description: "Dynamics - compressores e gates";        Types: full custom
Name: "eq";          Description: "EQ - equalizadores";                     Types: full custom
Name: "lofi";        Description: "Lo-Fi - degradacao criativa";            Types: full custom
Name: "oneknob";     Description: "OneKnob - um unico controle";            Types: full custom
Name: "saturation";  Description: "Saturation - saturacao harmonica";       Types: full custom
Name: "space";       Description: "Space - reverberacao e delay";           Types: full custom
Name: "strips";      Description: "Strips - tiras de canal (mixer)";        Types: full custom
Name: "tape";        Description: "Tape - simulacao de fita";               Types: full custom
Name: "drumforge";   Description: "Drumforge - sampler de bateria";         Types: full custom
Name: "rack";        Description: "ForjaRack - rack de efeitos em serie";   Types: full custom
Name: "minis";       Description: "Minis - mini plugins (Minidrum)";        Types: full custom

[Files]
; Cada familia copia seus .vst3 direto para a raiz VST3 ({app}), achatando o
; agrupamento por familia feito no payload. recursesubdirs preserva a estrutura
; interna de cada bundle (Contents\...).
Source: "payload\Amps\*";        DestDir: "{app}"; Components: amps;       Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\Dynamics\*";    DestDir: "{app}"; Components: dynamics;   Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\Eq\*";          DestDir: "{app}"; Components: eq;         Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\Lofi\*";        DestDir: "{app}"; Components: lofi;       Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\OneKnob\*";     DestDir: "{app}"; Components: oneknob;    Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\Saturation\*";  DestDir: "{app}"; Components: saturation; Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\Space\*";       DestDir: "{app}"; Components: space;      Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\Strips\*";      DestDir: "{app}"; Components: strips;     Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\Tape\*";        DestDir: "{app}"; Components: tape;       Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\Drumforge\*";   DestDir: "{app}"; Components: drumforge;  Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\Rack\*";        DestDir: "{app}"; Components: rack;       Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist
Source: "payload\Minis\*";       DestDir: "{app}"; Components: minis;      Flags: recursesubdirs createallsubdirs ignoreversion skipifsourcedoesntexist

; Documentacao e creditos MIT num lugar limpo (fora da raiz VST3), sempre junto.
Source: "payload\LICENSE.txt";                          DestDir: "{commonappdata}\Forja"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\docs\instalar\INSTALAR-WINDOWS.txt";        DestDir: "{commonappdata}\Forja"; DestName: "LEIA-ME.txt"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\Documentacao da suite Forja"; Filename: "{commonappdata}\Forja\LEIA-ME.txt"; Tasks: docs
Name: "{group}\Desinstalar Forja"; Filename: "{uninstallexe}"

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
