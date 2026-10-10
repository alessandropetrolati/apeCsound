#!/bin/bash
# apeCsound: compila il VST3 (Release), lo installa in ~/Library/Audio/Plug-Ins/VST3
# e lo verifica con pluginval (strictness 5).
# Il log completo va in Installers/Scripts/pluginval_log.txt.
# Si lancia con un doppio clic dal Finder.

cd "$(dirname "$0")/../.." || exit 1          # radice del progetto
LOG="Installers/Scripts/pluginval_log.txt"
exec > >(tee "$LOG") 2>&1

echo "=== $(date)  $(sw_vers -productName) $(sw_vers -productVersion) $(uname -m)"

PLUGINVAL="/Applications/pluginval.app/Contents/MacOS/pluginval"
[ -x "$PLUGINVAL" ] || { echo "ERRORE: pluginval non trovato in /Applications"; exit 1; }

# 1. Build VST3 Release (il progetto Xcode e' gia' stato rigenerato da run_auval.command)
# link simbolici rotti lasciati da un Archive di Xcode bloccano la build
for l in "Builds/MacOSX/build/Release/apeCsound.vst3" "Builds/MacOSX/build/Release/juce_vst3_helper"; do
    [ -L "$l" ] && [ ! -e "$l" ] && rm "$l" && echo "rimosso link rotto: $l"
done

echo "=== xcodebuild VST3 (Release)"
xcodebuild -project Builds/MacOSX/apeCsound.xcodeproj -target "apeCsound - VST3" \
           -configuration Release -quiet build 2>&1 | grep -v "pragma message\|#pragma\|^ *|\|^ *[0-9]* |\|In file included\|warning generated" \
           ; echo "xcodebuild exit: ${PIPESTATUS[0]}"
SRC="Builds/MacOSX/build/Release/apeCsound.vst3"
[ -d "$SRC" ] || { echo "ERRORE: $SRC non trovato"; exit 1; }

# 2. Installa
DST="$HOME/Library/Audio/Plug-Ins/VST3/apeCsound.vst3"
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
rm -rf "$DST" && cp -R "$SRC" "$DST"
echo "=== installato: $DST"
codesign -dv "$DST" 2>&1 | grep -E "Identifier|TeamIdentifier" | head -2
[ -f "$DST/Contents/Resources/moduleinfo.json" ] && echo "moduleinfo.json presente" || echo "ATTENZIONE: moduleinfo.json assente"
python3 Installers/Scripts/verify_vst3_manifest.py "$DST"

# 3. pluginval
echo "=== pluginval --strictness-level 5"
"$PLUGINVAL" --strictness-level 5 --validate "$DST"
RES=$?
echo "=== pluginval exit code: $RES"
echo "=== FINE"
