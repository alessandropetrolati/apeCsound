#!/bin/bash
# apeCsound: rigenera il progetto Xcode, compila l'AU (Release), lo installa
# in ~/Library/Audio/Plug-Ins/Components e lo verifica con auval.
# Il log completo va in Installers/Scripts/auval_log.txt.
# Si lancia con un doppio clic dal Finder.

# Codici AU dal .jucer: produttore 'Apes', plugin 'Csnd', tipo 'aufx' (effetto).
AU_TYPE="aufx"
AU_SUBTYPE="Csnd"
AU_MANUFACTURER="Apes"

cd "$(dirname "$0")/../.." || exit 1          # radice del progetto
LOG="Installers/Scripts/auval_log.txt"
exec > >(tee "$LOG") 2>&1

echo "=== $(date)  $(sw_vers -productName) $(sw_vers -productVersion) $(uname -m)"

# 1. Projucer: rigenera Builds/MacOSX dal .jucer
PROJUCER=""
for p in "/Applications/Projucer.app" \
         "$HOME/Desktop/Advanced 2018/JUCE/Projucer.app" \
         "$HOME/Desktop/Advanced 2018/JUCE/extras/Projucer/Builds/MacOSX/build/Release/Projucer.app" \
         "$HOME/Desktop/Advanced 2018/JUCE/extras/Projucer/Builds/MacOSX/build/Debug/Projucer.app"; do
    [ -d "$p" ] && PROJUCER="$p" && break
done
[ -z "$PROJUCER" ] && PROJUCER="$(mdfind 'kMDItemCFBundleIdentifier == "com.juce.theprojucer"' | head -1)"
if [ -n "$PROJUCER" ]; then
    echo "=== Projucer: $PROJUCER"
    "$PROJUCER/Contents/MacOS/Projucer" --resave "apeCsound.jucer" || { echo "ERRORE: resave"; exit 1; }
else
    echo "ATTENZIONE: Projucer non trovato, uso il progetto Xcode esistente"
fi

# 2. Build AU Release
echo "=== xcodebuild AU (Release)"
xcodebuild -project Builds/MacOSX/apeCsound.xcodeproj -target "apeCsound - AU" \
           -configuration Release -quiet build || { echo "ERRORE: build AU"; exit 1; }

# 3. Installa il componente
SRC="Builds/MacOSX/build/Release/apeCsound.component"
DST="$HOME/Library/Audio/Plug-Ins/Components/apeCsound.component"
[ -d "$SRC" ] || { echo "ERRORE: $SRC non trovato"; exit 1; }
mkdir -p "$HOME/Library/Audio/Plug-Ins/Components"
rm -rf "$DST" && cp -R "$SRC" "$DST"
echo "=== installato: $DST"
codesign -dv "$DST" 2>&1 | grep -E "Authority|TeamIdentifier|Identifier" | head -4
killall -9 AudioComponentRegistrar 2>/dev/null
sleep 2

# 4. auval
echo "=== auval -a ($AU_MANUFACTURER)"
auval -a 2>/dev/null | grep -i "$AU_MANUFACTURER"
echo "=== auval -strict -v $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER"
auval -strict -v "$AU_TYPE" "$AU_SUBTYPE" "$AU_MANUFACTURER"
RES=$?
echo "=== auval exit code: $RES"
echo "=== FINE"
