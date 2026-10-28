#define MyAppName "apeCsound"
#define MyAppVersion "1.0.5"
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

; Se installi plugin in Program Files, è meglio che l'uninstaller stia vicino ad essi
DefaultDirName={commonpf64}\Studiologic\Super Sledge
DefaultGroupName=Studiologic\Super Sledge

; Forza l'installazione a 64-bit (necessario per {commoncf64} su sistemi 64-bit)
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

UninstallDisplayIcon={app}\{#MyAppName}.ico
LicenseFile=License.txt
InfoBeforeFile=Introduction.txt
InfoAfterFile=Readme.txt
OutputDir="build"
OutputBaseFilename=Syntex_Setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern

; Suggerisco di lasciare la DirPage attiva se l'utente volesse cambiare il percorso dei presets
DisableDirPage=no
DisableProgramGroupPage=yes

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin"; Types: full custom; Flags: fixed
Name: "aax"; Description: "AAX Plugin"; Types: full
; Name: "presets"; Description: "Presets and documentation"; Types: full custom; Flags: fixed

[Files]
; VST3 Plugin
Source: "WINBIN\Super Sledge.vst3"; DestDir: "{commoncf64}\VST3"; Flags: ignoreversion; Components: vst3

; AAX Plugin
Source: "WINBIN\Super Sledge.aaxplugin\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins\Super Sledge.aaxplugin"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: aax

; Presets - Adattato al nuovo percorso JUCE con la sottocartella "PRESET"
;Source: "Studiologic\Super Sledge\*"; DestDir: "{userappdata}\{#MyAppPublisher}\PRESET\{#MyAppName}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: presets

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
; Shortcut cartella presets aggiornata
; Name: "{group}\Super Sledge Presets Folder"; Filename: "{userappdata}\{#MyAppPublisher}\PRESET\{#MyAppName}"
Name: "{group}\Website"; Filename: "{#MyAppURL}"
