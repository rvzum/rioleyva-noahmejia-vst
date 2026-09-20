; RIO LEYVA x NOAH MEJIA VST -- Windows installer (Inno Setup script).
;
; Installs the VST3 plugin bundle into the standard shared VST3 folder
; (Common Files\VST3, 64-bit) so every DAW on the machine can find it --
; mirrors the macOS installer's system-wide /Library/Audio/Plug-Ins/VST3/
; install (see Scripts/build_installer.sh / Packaging/README.md).
;
; Optionally also bundles the sample banks (on by default, uncheckable
; on the Select Components page) into a machine-wide, admin-only location
; -- C:\ProgramData\27wav\Sample Banks -- which
; SampleLibraryManager::getBundledLibraryRoot() reads alongside each
; user's own %USERPROFILE%\Music\27wav rioleyva & noahmejia banks\.
; ProgramData was chosen over a per-user path because Inno Setup's
; {user*} constants are documented as unreliable when the installer runs
; elevated (as this one does, via PrivilegesRequired=admin) -- see
; Packaging/README.md for the full rationale, which mirrors the macOS
; side's /Library/Application Support/27wav/Sample Banks choice.
;
; This project is built and signed nowhere on Windows yet (no Windows
; machine available locally), so this script is driven entirely by
; .github/workflows/build-installers.yml on a windows-latest GitHub
; Actions runner. It expects these values passed on the ISCC command line:
;
;   ISCC.exe Packaging\windows\installer.iss ^
;       /DAppVersion="0.1.0" ^
;       /DSourceVst3Dir="C:\path\to\RIO LEYVA x NOAH MEJIA VST.vst3" ^
;       /DSourceSamplesDir="C:\path\to\27wav rioleyva & noahmejia banks" ^
;       /O"Packaging\build" /F"RIO LEYVA x NOAH MEJIA VST Installer"
;
; AppVersion defaults to 0.0.0 and SourceVst3Dir must exist for a local
; test run. SourceSamplesDir is OPTIONAL: omit /DSourceSamplesDir entirely
; (don't pass it, even empty) to build a plugin-only installer -- useful
; for a quick local test build without the ~2GB of sample content. See
; Packaging/windows/README.md for how to run this by hand if you ever get
; access to a Windows machine.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceVst3Dir
  #define SourceVst3Dir "..\..\build\27wavVST_artefacts\Release\VST3\RIO LEYVA x NOAH MEJIA VST.vst3"
#endif

#define AppName "RIO LEYVA x NOAH MEJIA VST"
#define AppPublisher "27wav"
#define VstBundleName "RIO LEYVA x NOAH MEJIA VST.vst3"
#define SamplesInstallDir "{commonappdata}\27wav\Sample Banks"

#ifdef SourceSamplesDir
  #define IncludeSamples
  #define OutputBaseName AppName + " Installer"
#else
  #define OutputBaseName AppName + " Installer (no samples)"
#endif

[Setup]
AppId={{5F6C322D-7A0D-4C39-857A-0A060089324A}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableWelcomePage=no
OutputDir=build
OutputBaseFilename={#OutputBaseName}
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayIcon={app}\{#VstBundleName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

#ifdef IncludeSamples
[Types]
Name: "full"; Description: "Full installation (plugin + sample banks)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "plugin"; Description: "VST3 Plugin"; Types: full custom; Flags: fixed
Name: "samples"; Description: "Sample Banks"; Types: full custom
#endif

[Files]
#ifdef IncludeSamples
Source: "{#SourceVst3Dir}\*"; DestDir: "{app}\{#VstBundleName}"; Flags: recursesubdirs createallsubdirs ignoreversion; Components: plugin
Source: "{#SourceSamplesDir}\*"; DestDir: "{#SamplesInstallDir}"; Flags: recursesubdirs createallsubdirs ignoreversion; Components: samples
#else
Source: "{#SourceVst3Dir}\*"; DestDir: "{app}\{#VstBundleName}"; Flags: recursesubdirs createallsubdirs ignoreversion
#endif

[Messages]
#ifdef IncludeSamples
WelcomeLabel2=This installs {#AppName} as a VST3 plugin for every DAW on this computer (into the shared Common Files\VST3 folder), along with its sample banks (installed once, shared by every user on this computer).%n%nThis installer is not digitally signed (no code-signing certificate yet). Windows SmartScreen may show a "Windows protected your PC" warning the first time it runs -- click "More info", then "Run anyway", to continue. This is standard for unsigned installers and does not affect the plugin itself.
#else
WelcomeLabel2=This installs {#AppName} as a VST3 plugin for every DAW on this computer (into the shared Common Files\VST3 folder).%n%nThis installer is not digitally signed (no code-signing certificate yet). Windows SmartScreen may show a "Windows protected your PC" warning the first time it runs -- click "More info", then "Run anyway", to continue. This is standard for unsigned installers and does not affect the plugin itself.
#endif
