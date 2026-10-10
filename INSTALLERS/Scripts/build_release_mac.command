#!/bin/bash
# apeCsound: build Release di Standalone (.app), AU (.component) e VST3,
# copia in Installers/NOTORIZED (i percorsi usati da "apeCsoun d macOS.pkgproj"),
# rimozione e nuova firma Developer ID, verifica.
# Il log completo va in Installers/Scripts/build_release_log.txt.
# Si lancia con un doppio clic dal Finder.

# Identita' Developer ID Application di Alessandro Petrolati. Basta una parte
# univoca del nome; se nel Portachiavi ce n'e' piu' d'una, aggiungere il
# Team ID tra parentesi, es. "Developer ID Application: Alessandro Petrolati (ABCDE12345)".
# Elenco delle identita' installate: security find-identity -v -p codesigning
IDENTITY="Developer ID Application: Alessandro Petrolati (4863C638XM)"

# Opzioni di firma. --timestamp e --options runtime (hardened runtime) servono
# per la notarizzazione del pacchetto .pkg: senza, notarytool rifiuta i binari.
SIGN_OPTS="--force --timestamp --options runtime"

# Entitlements dell'app standalone: con l'hardened runtime, senza
# com.apple.security.device.audio-input il microfono/ingresso audio e' muto.
# (AU e VST3 girano nel processo dell'host: valgono gli entitlements dell'host.)
APP_ENTITLEMENTS="Installers/Scripts/apeCsound_app.entitlements"

cd "$(dirname "$0")/../.." || exit 1          # radice del progetto (contiene apeCsound.jucer)
ROOT="$(pwd)"
LOG="Installers/Scripts/build_release_log.txt"
exec > >(tee "$LOG") 2>&1

echo "=== $(date)  $(sw_vers -productName) $(sw_vers -productVersion) $(uname -m)"
FAIL=0

if ! security find-identity -v -p codesigning | grep -q "$IDENTITY"; then
    echo "ERRORE: identita' di firma \"$IDENTITY\" non trovata nel Portachiavi."
    echo "        Identita' disponibili:"
    security find-identity -v -p codesigning
    exit 1
fi

# --- 1. Projucer: rigenera il progetto Xcode
PROJUCER=""
for p in "/Applications/Projucer.app" \
         "$HOME/Desktop/Advanced 2018/JUCE/Projucer.app" \
         "$HOME/Desktop/Advanced 2018/JUCE/extras/Projucer/Builds/MacOSX/build/Release/Projucer.app"; do
    [ -d "$p" ] && PROJUCER="$p" && break
done
[ -z "$PROJUCER" ] && PROJUCER="$(mdfind 'kMDItemCFBundleIdentifier == "com.juce.theprojucer"' | head -1)"
if [ -n "$PROJUCER" ]; then
    echo "=== Projucer: $PROJUCER"
    "$PROJUCER/Contents/MacOS/Projucer" --resave "apeCsound.jucer" || { echo "ERRORE: resave"; exit 1; }
else
    echo "ATTENZIONE: Projucer non trovato, uso il progetto Xcode esistente"
fi

# --- 2. Build Release (per target: non dipende dagli scheme condivisi)
REL="Builds/MacOSX/build/Release"
# Un Archive di Xcode lascia in build/Release dei LINK SIMBOLICI verso
# DerivedData/.../ArchiveIntermediates: la build normale non riesce a scriverci
# ("unable to create directory ...apeCsound.vst3") e lo script finirebbe per
# copiare i prodotti VECCHI dell'archivio. Si rimuovono tutti, validi o rotti.
for l in "$REL"/*; do
    [ -L "$l" ] && rm "$l" && echo "rimosso link simbolico (Archive): $l"
done

build() # $1 = target Xcode
{
    echo "=== xcodebuild $1 (Release)"
    xcodebuild -project Builds/MacOSX/apeCsound.xcodeproj -target "$1" -configuration Release -quiet build 2>&1 \
        | grep -v "pragma message\|#pragma\|^ *|\|^ *[0-9]* |\|In file included\|warning generated\|^note: Run script\|Using the first of multiple\|platform:macOS"
    local rc=${PIPESTATUS[0]}
    echo "xcodebuild $1 exit: $rc"
    [ $rc -eq 0 ] || FAIL=1
}

build "apeCsound - Standalone Plugin"
build "apeCsound - AU"
build "apeCsound - VST3"

# --- 3. Controllo del manifest VST3 (moduleinfo.json): se i CID non
# corrispondono al binario, lo corregge (vedi fix_moduleinfo_json.py)
if [ -d "$REL/apeCsound.vst3" ]; then
    echo "=== verifica moduleinfo.json"
    if ! python3 Installers/Scripts/verify_vst3_manifest.py "$REL/apeCsound.vst3"; then
        echo "=== moduleinfo.json non coerente: correzione"
        python3 Installers/Scripts/fix_moduleinfo_json.py "$REL/apeCsound.vst3" --no-backup || FAIL=1
    fi
fi

# --- 4. Copia in Installers/NOTORIZED (nomi e percorsi del .pkgproj)
OUT="Installers/NOTORIZED"
mkdir -p "$OUT"
for b in "apeCsound.app" "apeCsound.component" "apeCsound.vst3"; do
    if [ -d "$REL/$b" ]; then
        rm -rf "$OUT/$b"
        ditto "$REL/$b" "$OUT/$b"
        echo "copiato: $OUT/$b"
    else
        echo "ERRORE: $REL/$b non trovato"
        FAIL=1
    fi
done

# --- 4b. Standalone SENZA AUv3: con buildAUv3 attivo nel .jucer, l'app
# standalone macOS incorpora l'estensione (Contents/PlugIns/apeCsound.appex).
# Su macOS si distribuiscono solo AU e VST3: l'estensione si toglie qui,
# prima della firma (che altrimenti la includerebbe nel sigillo dell'app).
if [ -d "$OUT/apeCsound.app/Contents/PlugIns" ]; then
    for x in "$OUT/apeCsound.app/Contents/PlugIns/"*.appex; do
        [ -e "$x" ] || continue
        rm -rf "$x" && echo "rimossa estensione AUv3 dall'app: $x"
    done
    rmdir "$OUT/apeCsound.app/Contents/PlugIns" 2>/dev/null
fi

# --- 5. Rimuovi firma e rifirma
for b in "apeCsound.vst3" "apeCsound.component"; do
    [ -d "$OUT/$b" ] || continue
    echo "=== firma: $b"
    codesign --remove-signature "$OUT/$b"
    codesign -s "$IDENTITY" $SIGN_OPTS "$OUT/$b" || FAIL=1
done

if [ -d "$OUT/apeCsound.app" ]; then
    echo "=== firma: apeCsound.app"
    codesign --remove-signature "$OUT/apeCsound.app"
    if [ -f "$APP_ENTITLEMENTS" ]; then
        codesign -s "$IDENTITY" $SIGN_OPTS --entitlements "$APP_ENTITLEMENTS" "$OUT/apeCsound.app" || FAIL=1
    else
        echo "ATTENZIONE: $APP_ENTITLEMENTS assente: app firmata senza entitlements (ingresso audio muto)"
        codesign -s "$IDENTITY" $SIGN_OPTS "$OUT/apeCsound.app" || FAIL=1
    fi
fi

# --- 6. Verifica firme
for b in "apeCsound.app" "apeCsound.component" "apeCsound.vst3"; do
    [ -d "$OUT/$b" ] || continue
    echo "=== verifica: $b"
    codesign --verify --deep --strict --verbose=2 "$OUT/$b" 2>&1 || FAIL=1
    codesign -dv "$OUT/$b" 2>&1 | grep -E "^Authority=Developer ID|^TeamIdentifier|^Timestamp|flags=" | head -4
done

echo
ls -la "$ROOT/$OUT"
echo
[ $FAIL -eq 0 ] && echo "=== RISULTATO: OK" || echo "=== RISULTATO: ERRORI (vedi sopra)"
echo "=== FINE"
