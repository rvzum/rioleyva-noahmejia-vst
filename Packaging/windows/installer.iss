; RIO LEYVA x NOAH MEJIA VST -- Windows installer (Inno Setup script).
;
; Installs the VST3 plugin bundle into the standard shared VST3 folder
; (Common Files\VST3, 64-bit) so every DAW on the machine can find it --
; mirrors the macOS installer's system-wide /Library/Audio/Plug-Ins/VST3/
; install (see Scripts/build_installer.sh / Packaging/README.md).
;
; This project is built and signed nowhere on Windows yet (no Windows
; machine available locally), so this script is driven entirely by
; .github/workflows/build-installers.yml on a windows-latest GitHub
; Actions runner. It expects two values passed on the ISCC command line:
;
;   ISCC.exe Packaging\windows\installer.iss ^
;       /DAppVersion="0.1.0" ^
;       /DSourceVst3Dir="C:\path\to\RIO LEYVA x NOAH MEJIA VST.vst3" ^
;       /O"Packaging\build" /F"RIO LEYVA x NOAH MEJIA VST Installer"
;
; AppVersion defaults to 0.0.0 and SourceVst3Dir must exist for a local
; test run -- see Packaging/windows/README.md for how to run this by hand
; if you ever get access to a Windows machine.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceVst3Dir
  #define SourceVst3Dir "..\..\build\27wavVST_artefacts\Release\VST3\RIO LEYVA x NOAH MEJIA VST.vst3"
#endif

#define AppName "RIO LEYVA x NOAH MEJIA VST"
#define AppPublisher "27wav"
#define VstBundleName "RIO LEYVA x NOAH MEJIA VST.vst3"

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
OutputBaseFilename={#AppName} Installer
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayIcon={app}\{#VstBundleName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceVst3Dir}\*"; DestDir: "{app}\{#VstBundleName}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
WelcomeLabel2=This installs {#AppName} as a VST3 plugin for every DAW on this computer (into the shared Common Files\VST3 folder).%n%nThis installer is not digitally signed (no code-signing certificate yet). Windows SmartScreen may show a "Windows protected your PC" warning the first time it runs -- click "More info", then "Run anyway", to continue. This is standard for unsigned installers and does not affect the plugin itself.
