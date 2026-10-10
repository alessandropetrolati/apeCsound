#!/bin/bash
# apeCsound: build del pacchetto (Packages), firma (productsign),
# notarizzazione (notarytool), staple e verifica del pacchetto macOS.
#   ingresso: Installers/build/apeCsound.pkg  (prodotto da "apeCsoun d macOS.pkgproj")
#   uscita:   Installers/build/apeCsound_signed.pkg
# Il log completo va in Installers/Scripts/notarize_log.txt.
# Si lancia con un doppio clic dal Finder, dopo build_release_mac.command.

# Identita' Developer ID Installer di Alessandro Petrolati (vedi la nota su
# IDENTITY in build_release_mac.command). Elenco:
#   security find-identity -v | grep "Developer ID Installer"
INSTALLER_ID="Developer ID Installer: Alessandro Petrolati (4863C638XM)"

# Profilo notarytool nel Portachiavi, da creare una volta con:
#   xcrun notarytool store-credentials "apeSoft" --apple-id <tuo Apple ID> \
#         --team-id <tuo Team ID> --password <password specifica per app>
PROFILE="apeSoft"

PKGPROJ="apeCsoun d macOS.pkgproj"
PKG="apeCsound.pkg"
SIGNED="apeCsound_signed.pkg"

cd "$(dirname "$0")/.." || exit 1          # cartella Installers
ROOT="$(pwd)"
LOG="Scripts/notarize_log.txt"
exec > >(tee "$LOG") 2>&1
echo "=== $(date)  $(sw_vers -productName) $(sw_vers -productVersion) $(uname -m)"

# --- 0. Build del pacchetto con Packages (packagesbuild), se installato
PACKAGESBUILD="$(command -v packagesbuild || echo /usr/local/bin/packagesbuild)"
if [ -x "$PACKAGESBUILD" ]; then
    echo "=== packagesbuild: $PKGPROJ"
    "$PACKAGESBUILD" -v "$PKGPROJ" || { echo "ERRORE: packagesbuild"; exit 1; }
else
    echo "packagesbuild non trovato: uso il pacchetto gia' presente in Installers/build"
fi

cd build || { echo "ERRORE: cartella Installers/build assente"; exit 1; }
[ -f "$PKG" ] || { echo "ERRORE: $PKG non trovato in Installers/build"; exit 1; }
ls -la "$PKG"

# --- 1. Firma Developer ID Installer
# Se Packages ha gia' firmato il .pkg con l'identita' giusta, productsign non
# serve (e su un pacchetto gia' firmato puo' fallire).
rm -f "$SIGNED"
if pkgutil --check-signature "$PKG" 2>/dev/null | grep -q "$INSTALLER_ID"; then
    echo "=== $PKG e' gia' firmato da Packages con $INSTALLER_ID: copio in $SIGNED"
    cp "$PKG" "$SIGNED"
else
    if pkgutil --check-signature "$PKG" 2>/dev/null | grep -q "Developer ID Installer"; then
        echo "ATTENZIONE: $PKG e' firmato con un'altra identita' (controlla il certificato in $PKGPROJ):"
        pkgutil --check-signature "$PKG" | grep "Developer ID Installer"
        echo "ERRORE: rimuovi il certificato dal progetto Packages o imposta quello di Alessandro Petrolati"
        exit 1
    fi
    echo "=== productsign"
    productsign --sign "$INSTALLER_ID" "$PKG" "$SIGNED" || { echo "ERRORE: productsign"; exit 1; }
fi

# --- 2. Notarizzazione
echo "=== notarytool submit (attesa del risultato)"
OUT="$(xcrun notarytool submit "$SIGNED" --keychain-profile "$PROFILE" --wait 2>&1)"
RC=$?
echo "$OUT"
SUBMISSION_ID="$(echo "$OUT" | awk '/^ *id:/{print $2; exit}')"
if [ $RC -ne 0 ] || ! echo "$OUT" | grep -q "status: Accepted"; then
    echo "ERRORE: notarizzazione non accettata"
    if [ -n "$SUBMISSION_ID" ]; then
        echo "=== log di Apple per $SUBMISSION_ID"
        xcrun notarytool log "$SUBMISSION_ID" --keychain-profile "$PROFILE"
    fi
    exit 1
fi

# --- 3. Staple
echo "=== stapler staple"
xcrun stapler staple "$SIGNED" || { echo "ERRORE: stapler"; exit 1; }
xcrun stapler validate "$SIGNED"

# --- 4. Verifiche
echo "=== pkgutil --check-signature"
pkgutil --check-signature "$SIGNED"
echo "=== spctl --assess"
spctl --assess -vvv --type install "$SIGNED"
SPCTL=$?

echo
ls -la "$ROOT/build/$SIGNED"
[ $SPCTL -eq 0 ] && echo "=== RISULTATO: OK (firmato, notarizzato, staple)" || echo "=== RISULTATO: spctl ha rifiutato il pacchetto"
echo "=== FINE"
