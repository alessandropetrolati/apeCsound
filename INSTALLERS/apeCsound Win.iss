; apeCsound - installer Windows (Inno Setup).
; NOTA: le release Windows si fanno con GitHub Actions (.github/workflows/
; build-windows.yml, che produce uno zip). Questo script resta come
; alternativa locale: compila l'installer dai file messi in WINBIN\.
;
;   WINBIN\apeCsound.vst3\   (cartella bundle VST3, come esce dal build)
;   WINBIN\apeCsound.exe     (standalone)

#define MyAppName "apeCsound"
#define MyAppVersion "1.1.1"
#define MyAppPublisher "apeSoft"
#define MyAppURL "https://www.apesoft.it/"

[Setup]
AppId={{FFFE4825-1A70-497E-946B-960CEA17EB9E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

DefaultDirName={commonpf64}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppPublisher}\{#MyAppName}

; 64 bit (necessario per {commoncf64})
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

UninstallDisplayIcon={app}\{#MyAppName}.exe
LicenseFile=License.txt
OutputDir=build
OutputBaseFilename={#MyAppName}_{#MyAppVersion}_Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
DisableProgramGroupPage=yes

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in"; Types: full custom
Name: "standalone"; Description: "Standalone application"; Types: full custom

[Files]
; VST3: nella cartella comune dei plug-in VST3
Source: "WINBIN\{#MyAppName}.vst3\*"; DestDir: "{commoncf64}\VST3\{#MyAppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
; Standalone
Source: "WINBIN\{#MyAppName}.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppName}.exe"; Components: standalone
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{group}\Website"; Filename: "{#MyAppURL}"

[Run]
Filename: "{app}\{#MyAppName}.exe"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent unchecked; Components: standalone
