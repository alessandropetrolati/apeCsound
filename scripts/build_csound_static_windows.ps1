# Static build of Csound 7 for Windows (x64, Visual Studio 2022 / MSVC):
# core + libsndfile + libsamplerate + OSC opcodes (liblo, static), no realtime
# audio/MIDI backends, no csbeats/DSSI, no separate opcode plugins - the same "monolithic" setup
# as the macOS/iOS/Linux scripts: apeCsound drives Csound through the C API
# with its own audio/MIDI I/O, and Csound is linked statically into the
# VST3 and the Standalone.
#
# Runtime: STATIC MSVC runtime (/MT, /MTd in Debug), the same one set in the
# .jucer (VS2022 exporter, useRuntimeLibDLL="0"): no Visual C++ Redistributable
# needed on the user's machine, and no LNK2038 "RuntimeLibrary mismatch".
# Every library is built in Debug AND Release, each linked by the matching
# configuration of the Visual Studio project.
#
# Requirements (once):
#   - Visual Studio 2022 (or Build Tools) with "Desktop development with C++"
#   - CMake 3.21+ and Git in the PATH
#   - win_flex / win_bison: downloaded automatically (winflexbison) if missing
#
# Usage (PowerShell, from the project root or the scripts folder):
#   powershell -ExecutionPolicy Bypass -File scripts\build_csound_static_windows.ps1
#
# Output (the Projucer VS2022 exporter points here):
#   build\csound-install\windows\lib\Debug\   csound64.lib  sndfile.lib  samplerate.lib  lo.lib  csoundosc.lib
#   build\csound-install\windows\lib\Release\ csound64.lib  sndfile.lib  samplerate.lib  lo.lib  csoundosc.lib
#   build\csound-install\windows\include\     csound.h and friends
#
# Then: open apeCsound.jucer in Projucer, save (Builds\VisualStudio2022),
# open the .sln and build "apeCsound - VST3" / "apeCsound - Standalone Plugin" (x64).

$ErrorActionPreference = "Stop"

$CsoundRepo     = "https://github.com/csound/csound.git"
$CsoundBranch   = "7.0.0"     # official Csound 7.0.0 release tag, same as the other scripts
$SndfileRepo    = "https://github.com/libsndfile/libsndfile.git"
$SamplerateRepo = "https://github.com/libsndfile/libsamplerate.git"
$LibloRepo      = "https://github.com/radarsat1/liblo.git"
$DirentRepo     = "https://github.com/tronkko/dirent.git"   # dirent.h for MSVC (required by Csound's CMake)
$WinFlexBisonUrl = "https://github.com/lexxmark/winflexbison/releases/download/v2.5.25/win_flex_bison-2.5.25.zip"

$ScriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Resolve-Path (Join-Path $ScriptDir "..")
$BuildRoot   = Join-Path $ProjectRoot "build"
$Work        = Join-Path $BuildRoot "windows-x64"
$Stage       = Join-Path $BuildRoot "csound-install\windows-x64"
$Out         = Join-Path $BuildRoot "csound-install\windows"
$Configs     = @("Debug", "Release")
# Generatore CMake: Visual Studio 2022 (lo stesso dell'exporter VS2022 del
# .jucer). Si puo' cambiare con la variabile d'ambiente CSOUND_CMAKE_GENERATOR,
# es. "Visual Studio 18 2026" su una macchina con solo VS 2026.
$GeneratorName = if ($env:CSOUND_CMAKE_GENERATOR) { $env:CSOUND_CMAKE_GENERATOR } else { "Visual Studio 17 2022" }
$Generator   = @("-G", $GeneratorName, "-A", "x64")

function Need($tool) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "'$tool' not found in PATH. Install it first (see the header of this script)."
    }
}
Need cmake
Need git

function CloneOnce($repo, $branch, $dest) {
    if (-not (Test-Path (Join-Path $dest ".git"))) {
        Write-Host "==> cloning $repo ($branch)"
        git clone --branch $branch --depth 1 $repo $dest
        if ($LASTEXITCODE -ne 0) { throw "git clone failed: $repo" }
    }
}

function Run($exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe failed (exit $LASTEXITCODE)" }
}

# Static MSVC runtime in every library: /MT (Release), /MTd (Debug).
$Common = @(
    "-DCMAKE_POLICY_DEFAULT_CMP0091=NEW",
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`$<`$<CONFIG:Debug>:Debug>",
    "-DBUILD_SHARED_LIBS=OFF"
)

New-Item -ItemType Directory -Force -Path $Work, $Stage | Out-Null

# ---------------------------------------------------------------------
# 0. win_flex / win_bison (Csound's grammar needs a modern bison)
# ---------------------------------------------------------------------
$FlexDir = Join-Path $Work "winflexbison"
$WinFlex  = Join-Path $FlexDir "win_flex.exe"
$WinBison = Join-Path $FlexDir "win_bison.exe"

if (-not (Test-Path $WinBison)) {
    Write-Host "==> downloading winflexbison"
    New-Item -ItemType Directory -Force -Path $FlexDir | Out-Null
    $zip = Join-Path $Work "winflexbison.zip"
    Invoke-WebRequest -Uri $WinFlexBisonUrl -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $FlexDir -Force
}

# ---------------------------------------------------------------------
# 1. dirent.h (header only)
# ---------------------------------------------------------------------
$DirentSrc = Join-Path $Work "dirent"
CloneOnce $DirentRepo "master" $DirentSrc
$DirentInclude = Join-Path $DirentSrc "include"

# ---------------------------------------------------------------------
# 2. libsndfile (core formats only) and libsamplerate, Debug + Release
# ---------------------------------------------------------------------
$SndfileSrc    = Join-Path $Work "sndfile\src"
$SamplerateSrc = Join-Path $Work "samplerate\src"
CloneOnce $SndfileRepo "master" $SndfileSrc
CloneOnce $SamplerateRepo "master" $SamplerateSrc

$SndfileBuild    = Join-Path $Work "sndfile\build"
$SamplerateBuild = Join-Path $Work "samplerate\build"

Write-Host "==> configuring libsndfile"
Run cmake (@("-S", $SndfileSrc, "-B", $SndfileBuild) + $Generator + $Common + @(
    "-DBUILD_PROGRAMS=OFF", "-DBUILD_EXAMPLES=OFF", "-DBUILD_TESTING=OFF",
    "-DENABLE_EXTERNAL_LIBS=OFF", "-DENABLE_MPEG=OFF", "-DENABLE_CPACK=OFF"))

Write-Host "==> configuring libsamplerate"
Run cmake (@("-S", $SamplerateSrc, "-B", $SamplerateBuild) + $Generator + $Common + @(
    "-DBUILD_TESTING=OFF", "-DLIBSAMPLERATE_EXAMPLES=OFF", "-DLIBSAMPLERATE_INSTALL=ON"))

foreach ($cfg in $Configs) {
    Write-Host "==> building libsndfile + libsamplerate ($cfg)"
    Run cmake @("--build", $SndfileBuild, "--config", $cfg, "--parallel")
    Run cmake @("--install", $SndfileBuild, "--config", $cfg, "--prefix", (Join-Path $Stage "sndfile-$cfg"))
    Run cmake @("--build", $SamplerateBuild, "--config", $cfg, "--parallel")
    Run cmake @("--install", $SamplerateBuild, "--config", $cfg, "--prefix", (Join-Path $Stage "samplerate-$cfg"))
}

# ---------------------------------------------------------------------
# 2b. liblo (OSC), static, Debug + Release: serve agli opcode OSC
#     (OSCinit/oscinit, OSClisten/osclisten...), vedi la sezione 3b di
#     build_csound_static_macos.sh e patch_csound_static_osc.py.
# ---------------------------------------------------------------------
$LibloSrc   = Join-Path $Work "liblo\src"
$LibloBuild = Join-Path $Work "liblo\build"
CloneOnce $LibloRepo "master" $LibloSrc

Write-Host "==> configuring liblo"
Run cmake (@("-S", (Join-Path $LibloSrc "cmake"), "-B", $LibloBuild) + $Generator + $Common + @(
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
    "-DWITH_STATIC=ON", "-DWITH_TOOLS=OFF", "-DWITH_TESTS=OFF", "-DWITH_EXAMPLES=OFF", "-DWITH_CPP_TESTS=OFF",
    "-DTHREADING=ON"))

foreach ($cfg in $Configs) {
    Write-Host "==> building liblo ($cfg)"
    Run cmake @("--build", $LibloBuild, "--config", $cfg, "--parallel")
    Run cmake @("--install", $LibloBuild, "--config", $cfg, "--prefix", (Join-Path $Stage "liblo-$cfg"))

    # lo\lo.h + lo_endian.h generato, se l'install non li ha copiati
    $loInc = Join-Path $Stage "liblo-$cfg\include\lo"
    if (-not (Test-Path (Join-Path $loInc "lo.h"))) {
        New-Item -ItemType Directory -Force -Path $loInc | Out-Null
        Copy-Item (Join-Path $LibloSrc "lo\*.h") $loInc
        Get-ChildItem -Path $LibloBuild -Recurse -Filter "*.h" | Where-Object { $_.FullName -match "\\lo\\" } |
            ForEach-Object { Copy-Item $_.FullName $loInc -Force }
    }
}

function FindLib($dir, $patterns) {
    foreach ($p in $patterns) {
        $f = Get-ChildItem -Path $dir -Recurse -Filter $p -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($f) { return $f.FullName }
    }
    throw "No library matching '$($patterns -join ', ')' in $dir"
}

# ---------------------------------------------------------------------
# 3. Csound 7, static, against our own sndfile/samplerate, all backends off.
#    One build tree per configuration (it must link the matching sndfile).
# ---------------------------------------------------------------------
$CsoundSrc = Join-Path $Work "csound\src"
# Csound: a checkout of a DIFFERENT ref (e.g. an old "develop" clone) is
# replaced, so that every build uses exactly $CsoundBranch.
if (Test-Path (Join-Path $CsoundSrc ".git")) {
    $currentRef = (git -C $CsoundSrc describe --tags --exact-match 2>$null)
    if (-not $currentRef) { $currentRef = (git -C $CsoundSrc rev-parse --abbrev-ref HEAD) }
    if ($currentRef -ne $CsoundBranch) {
        Write-Host "==> Csound sources are at '$currentRef', need '$CsoundBranch': re-cloning"
        Remove-Item -Recurse -Force $CsoundSrc
    }
}
CloneOnce $CsoundRepo $CsoundBranch $CsoundSrc

# OSC statico: patch condivisa dei sorgenti Csound (stessa logica di
# patch_csound_static_osc.py, qui in PowerShell per non richiedere Python):
# make_plugin() STATICO con APECSOUND_STATIC_PLUGINS=ON, plugin osc con la
# funzione apecsound_register_osc_opcodes() e gli entry point rinominati.
function Patch-CsoundStaticOsc($src) {
    $util = Join-Path $src "cmake\cmake-utilities.cmake"
    $t = [IO.File]::ReadAllText($util)
    if (-not $t.Contains("APECSOUND_STATIC_PLUGINS")) {
        $old = "    if(APPLE)`n        add_library(`${libname} SHARED `${srcs})"
        $t = $t.Replace("`r`n", "`n")
        if (-not $t.Contains($old)) { throw "patch: make_plugin() in cmake-utilities.cmake not recognised" }
        $t = $t.Replace($old, "    if(APECSOUND_STATIC_PLUGINS)`n        add_library(`${libname} STATIC `${srcs})`n    elseif(APPLE)`n        add_library(`${libname} SHARED `${srcs})")
        [IO.File]::WriteAllText($util, $t)
    }

    $ops = Join-Path $src "Opcodes\CMakeLists.txt"
    $t = [IO.File]::ReadAllText($ops)
    if (-not $t.Contains("apecsound_osc_register.c")) {
        $old = 'make_plugin(osc "OSC.c;osc_blob.c")'
        if (-not $t.Contains($old)) { throw "patch: make_plugin(osc ...) in Opcodes\CMakeLists.txt not recognised" }
        $t = $t.Replace($old, 'make_plugin(osc "OSC.c;osc_blob.c;apecsound_osc_register.c")' + "`n" +
            '    target_compile_definitions(osc PRIVATE csound_opcode_init=apecsound_osc_opcode_init csoundModuleInfo=apecsound_osc_module_info)')
        [IO.File]::WriteAllText($ops, $t)
    }

    $reg = @"
/* csdl.h, come ogni plugin: csoundCore.h da solo e' vietato fuori dal core. */
#include "csdl.h"

int64_t apecsound_osc_opcode_init(CSOUND *csound, OENTRY **ep);

/* apeCsound: registra gli opcode OSC (OSCinit, OSClisten, OSCsend...) in
   un'istanza Csound appena creata, al posto del caricamento del plugin
   dinamico. Chiamata da apeCsound dopo csoundCreate(). */
int32_t apecsound_register_osc_opcodes(CSOUND *csound)
{
    OENTRY *ep = NULL;
    int64_t bytes = apecsound_osc_opcode_init(csound, &ep);

    if (ep == NULL || bytes <= 0)
        return -1;

    /* Dai plugin le funzioni del core si chiamano tramite la struct CSOUND. */
    return csound->AppendOpcodes(csound, ep, (int32_t) (bytes / (int64_t) sizeof(OENTRY)));
}
"@
    [IO.File]::WriteAllText((Join-Path $src "Opcodes\apecsound_osc_register.c"), $reg)
}

Patch-CsoundStaticOsc $CsoundSrc

# MSVC has no dirent.h. Csound's CMake only find_file()s it (configure-time
# checks), it does not add its folder to the compiler include path, so
# new_orc_parser.c fails with C1083. Csound's H/ folder is always on the
# include path of csound64 / csound64-static: put the header there.
Copy-Item -Force (Join-Path $DirentInclude "dirent.h") (Join-Path $CsoundSrc "H\dirent.h")

foreach ($cfg in $Configs) {
    $sndfileLib    = FindLib (Join-Path $Stage "sndfile-$cfg\lib")    @("sndfile.lib", "libsndfile*.lib")
    $samplerateLib = FindLib (Join-Path $Stage "samplerate-$cfg\lib") @("samplerate.lib", "libsamplerate*.lib")
    $csoundBuild   = Join-Path $Work "csound\build-$cfg"
    $loLib         = FindLib (Join-Path $Stage "liblo-$cfg\lib")      @("liblo_static.lib", "lo_static.lib", "liblo.lib", "lo.lib", "*lo*.lib")

    Write-Host "==> configuring Csound ($cfg)"
    Write-Host "    SndFile:    $sndfileLib"
    Write-Host "    SampleRate: $samplerateLib"

    Run cmake (@("-S", $CsoundSrc, "-B", $csoundBuild) + $Generator + $Common + @(
        "-DCMAKE_INCLUDE_PATH=$DirentInclude",
        "-DFLEX_EXECUTABLE=$WinFlex",
        "-DBISON_EXECUTABLE=$WinBison",
        "-DSndFile_LIBRARY=$sndfileLib",
        "-DSndFile_INCLUDE_DIR=$(Join-Path $Stage "sndfile-$cfg\include")",
        "-DSampleRate_LIBRARY=$samplerateLib",
        "-DSampleRate_INCLUDE_DIR=$(Join-Path $Stage "samplerate-$cfg\include")",
        "-DUSE_VCPKG=OFF",
        "-DBUILD_STATIC_LIBRARY=ON",
        "-DBUILD_PLUGINS=OFF",
        "-DUSE_LIBSNDFILE=ON", "-DUSE_LIBSAMPLERATE=ON",
        "-DUSE_CURL=OFF", "-DUSE_GETTEXT=OFF",
        "-DBUILD_UTILITIES=OFF", "-DBUILD_TESTS=OFF", "-DBUILD_INSTALLER=OFF", "-DBUILD_DOCS=OFF",
        "-DUSE_DOUBLE=ON", "-DFAIL_MISSING=OFF",
        "-DUSE_PORTAUDIO=OFF", "-DUSE_PORTMIDI=OFF", "-DUSE_JACK=OFF", "-DUSE_ALSA=OFF",
        "-DUSE_PULSEAUDIO=OFF", "-DUSE_PIPEWIRE=OFF", "-DUSE_IPMIDI=OFF",
        "-DUSE_COREMIDI=OFF", "-DUSE_AUDIOUNIT=OFF",
        "-DBUILD_OSC_OPCODES=ON", "-DAPECSOUND_STATIC_PLUGINS=ON",
        "-DLibLO_LIBRARY=$loLib",
        "-DLibLO_INCLUDE_DIR=$(Join-Path $Stage "liblo-$cfg\include")",
        "-DBUILD_CSBEATS=OFF", "-DBUILD_DSSI_OPCODES=OFF",
        "-DUSE_ASA=OFF", "-DUSE_GIT_COMMIT=OFF"))
    # NB: keep BUILD_CSOUND_COMMAND ON: tests/commandline references the
    # csound-bin target unconditionally (generate step fails without it).
    # It is not built anyway: only the csound64-static target is built below.

    Write-Host "==> building Csound ($cfg)"
    # Only the static library: the DLL, csound.exe, csdebugger, utilities... are
    # not needed by apeCsound (and "cmake --install" would need all of them).
    Run cmake @("--build", $csoundBuild, "--config", $cfg, "--parallel", "--target", "csound64-static", "osc")

    # Manual "install": libcsound64.lib + public headers (src\include + generated version headers)
    $csoundStage = Join-Path $Stage "csound-$cfg"
    if (Test-Path $csoundStage) { Remove-Item -Recurse -Force $csoundStage }
    New-Item -ItemType Directory -Force -Path (Join-Path $csoundStage "lib"), (Join-Path $csoundStage "include\csound") | Out-Null
    $staticLib = Get-ChildItem -Path $csoundBuild -Recurse -Filter "libcsound64.lib" |
                 Where-Object { $_.FullName -match "\\$cfg\\" } | Select-Object -First 1
    if (-not $staticLib) { throw "libcsound64.lib ($cfg) not found under $csoundBuild" }
    Copy-Item $staticLib.FullName (Join-Path $csoundStage "lib\libcsound64.lib")
    Copy-Item (Join-Path $CsoundSrc "include\*.h")   (Join-Path $csoundStage "include\csound")
    Copy-Item (Join-Path $CsoundSrc "include\*.hpp") (Join-Path $csoundStage "include\csound") -ErrorAction SilentlyContinue
    Copy-Item (Join-Path $csoundBuild "include\*.h") (Join-Path $csoundStage "include\csound")

    # Opcode OSC statici (osc.lib, vedi Patch-CsoundStaticOsc) -> csoundosc.lib
    $oscLib = Get-ChildItem -Path $csoundBuild -Recurse -Filter "osc.lib" |
              Where-Object { $_.FullName -match "\\$cfg\\" } | Select-Object -First 1
    if (-not $oscLib) { throw "osc.lib ($cfg) not found under $csoundBuild (OSC opcodes not built: is liblo found?)" }
    Copy-Item $oscLib.FullName (Join-Path $csoundStage "lib\csoundosc.lib")
    Copy-Item $loLib (Join-Path $csoundStage "lib\lo.lib")
}

# ---------------------------------------------------------------------
# 4. flatten into build\csound-install\windows\{lib\<cfg>, include}
#    with fixed names: csound64.lib, sndfile.lib, samplerate.lib
# ---------------------------------------------------------------------
Write-Host "==> collecting libraries and headers in $Out"
if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
New-Item -ItemType Directory -Force -Path (Join-Path $Out "include") | Out-Null

foreach ($cfg in $Configs) {
    $libOut = Join-Path $Out "lib\$cfg"
    New-Item -ItemType Directory -Force -Path $libOut | Out-Null

    Copy-Item (FindLib (Join-Path $Stage "csound-$cfg")     @("libcsound64.lib", "csound64.lib", "*csound*.lib")) (Join-Path $libOut "csound64.lib")
    Copy-Item (FindLib (Join-Path $Stage "sndfile-$cfg")    @("sndfile.lib", "libsndfile*.lib"))                  (Join-Path $libOut "sndfile.lib")
    Copy-Item (FindLib (Join-Path $Stage "samplerate-$cfg") @("samplerate.lib", "libsamplerate*.lib"))            (Join-Path $libOut "samplerate.lib")
    Copy-Item (Join-Path $Stage "csound-$cfg\lib\csoundosc.lib") (Join-Path $libOut "csoundosc.lib")
    Copy-Item (Join-Path $Stage "csound-$cfg\lib\lo.lib")        (Join-Path $libOut "lo.lib")
}

# Csound installs its headers in include\csound\; the plugin includes them as "csound.h".
$csoundInc = Join-Path $Stage "csound-Release\include\csound"
if (-not (Test-Path $csoundInc)) { $csoundInc = Join-Path $Stage "csound-Release\include" }
Copy-Item -Recurse -Force (Join-Path $csoundInc "*")                                   (Join-Path $Out "include")
Copy-Item -Recurse -Force (Join-Path $Stage "sndfile-Release\include\*")    (Join-Path $Out "include")
Copy-Item -Recurse -Force (Join-Path $Stage "samplerate-Release\include\*") (Join-Path $Out "include")
Copy-Item -Recurse -Force (Join-Path $Stage "liblo-Release\include\*")      (Join-Path $Out "include")

if (-not (Test-Path (Join-Path $Out "include\csound.h"))) { throw "csound.h not found in $Out\include" }

Write-Host ""
Write-Host "Done."
Get-ChildItem -Recurse (Join-Path $Out "lib") | Format-Table FullName, Length
Write-Host "Next: open apeCsound.jucer in Projucer, save, then build Builds\VisualStudio2022\apeCsound.sln (x64)."
