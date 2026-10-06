#!/bin/sh
# Imbedda CsoundLib64.framework nel bundle del plugin/app, lo rende
# davvero rilocabile (niente piu' dipendenza da /Library/Frameworks sulla
# macchina di chi riceve il plugin) e firma tutto - in un unico passaggio
# automatico lanciato come postbuildCommand ad OGNI build. Dopo questo
# script non serve piu' nessun comando a mano in Terminale.
#
# Perche' serve tutto questo (non solo copia+firma):
#
# 1) Il framework reale di Csound 7 (installato dal pkg "csound7Environment"
#    in /Applications/Csound/CsoundLib64.framework - NON /Library/Frameworks,
#    che resta la vecchia installazione Csound 6.18) ha l'install name
#    (LC_ID_DYLIB) di CsoundLib64 scritto come path ASSOLUTO verso quella
#    posizione (verificato con 'strings'/'otool -D' sul binario). Quando
#    Xcode linka il nostro plugin contro la copia che teniamo nel progetto,
#    copia quello stesso path assoluto dentro il nostro eseguibile: a
#    runtime, su una macchina dove Csound non e' installato in
#    /Applications/Csound (l'intero senso di "monolitico"), il plugin non
#    troverebbe piu' la libreria e fallirebbe a caricarsi. Risolviamo
#    riscrivendo l'ID a un path rilocabile (@rpath/...) sulla COPIA
#    imbeddata, e correggendo di conseguenza il riferimento gia' compilato
#    nel nostro eseguibile con install_name_tool -change (l'ID vero e il
#    path assoluto, insieme al numero di versione dentro Versions/, vengono
#    letti dinamicamente - non scritti a mano - cosi' funziona anche se una
#    futura versione di Csound cambia il path o il numero esatto).
#
# 2) LD_RUNPATH_SEARCH_PATHS=@executable_path/../Frameworks (impostato in
#    Csound.jucer via customXcodeFlags) dice al loader dove cercare
#    @rpath/CsoundLib64.framework: funziona sia per lo Standalone (.app)
#    sia per il VST3 (.vst3), perche' in entrambi MacOS/ e Frameworks/ sono
#    cartelle sorelle dentro Contents/.
#
# 3) Csound cerca i suoi opcode "plugin" aggiuntivi (osc, pvsops, midi...)
#    in una cartella il cui default e' anch'esso compilato come path
#    assoluto sotto /Applications/Csound (vedi Resources/Opcodes64): quella
#    parte e' risolta lato codice C++, non qui - vedi getEmbeddedOpcodeDir()
#    in PluginProcessor.cpp, che passa il path della copia imbeddata
#    direttamente come secondo argomento di csoundCreate().
#
# 4) Alcune build di Csound (6.x) imbeddavano anche un libcsnd6*.dylib NON
#    firmato, che faceva fallire la fase "Embed Frameworks"/"Sign
#    ...framework" generata da Xcode/Projucer (firma Versions/* senza
#    --deep). La vera 7.0.0 non lo contiene piu', ma firmiamo comunque tutto
#    il bundle con --deep in un solo passaggio finale (copre anche questo
#    caso se mai tornasse, e sovrascrive/ignora qualunque firma parziale
#    messa in automatico da Xcode prima di questo script, eliminando ogni
#    dipendenza dall'ordine delle fasi di build).
#
# 5) Csound metteva anche una cartella "libs" (librerie di terze parti:
#    liblo, libportaudio, libportmidi) DIRETTAMENTE nella radice del
#    framework, non dentro Versions/<ver>/: Apple non lo permette (un
#    framework imbeddato puo' avere nella radice solo i soliti symlink
#    verso Versions/Current - CsoundLib64, Headers, Resources), e
#    "codesign --deep" sul bundle falliva con "unsealed contents present
#    in the root directory of an embedded framework". Risolto spostando
#    "libs" dentro Versions/<ver>/libs e rimettendo al suo posto un
#    normale symlink, esattamente come fa il framework stesso per
#    CsoundLib64/Headers/Resources - i path relativi tipo
#    "@loader_path/../libs/..." gia' presenti nei vari binari (CsoundLib64
#    stesso, i plugin opcode in Opcodes64) continuano a funzionare
#    identici, perche' per dyld un symlink e' del tutto trasparente.
#
# Puo' anche essere lanciato a mano per debug, impostando a mano le
# variabili che Xcode fornisce da solo durante il build, es.:
#   CODESIGNING_FOLDER_PATH=/path/al/Csound.app \
#   TARGET_BUILD_DIR=/path/build EXECUTABLE_PATH=Csound.app/Contents/MacOS/Csound \
#   WRAPPER_NAME=Csound.app ./embed_csound_framework.sh

set -e

FRAMEWORK_SRC="/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/Csound/CsoundLib64.framework"
DEST_CONTENTS="$CODESIGNING_FOLDER_PATH/Contents"

# Il target "Shared Code" (libreria statica interna di JUCE, non un bundle)
# non ha una cartella Contents: niente da imbeddare, usciamo senza errore.
if [ ! -d "$DEST_CONTENTS" ]; then
    exit 0
fi

DEST="$DEST_CONTENTS/Frameworks"
mkdir -p "$DEST"
rm -rf "$DEST/CsoundLib64.framework"
cp -R "$FRAMEWORK_SRC" "$DEST/"

# .DS_Store (residuo di Finder) dentro il framework fa fallire codesign con
# "unsealed contents present in the root directory" - rimosso sempre.
find "$DEST/CsoundLib64.framework" -name ".DS_Store" -delete

# Versions/Current e' un symlink verso la cartella versionata vera (es.
# "7.0"): lo leggiamo invece di scrivere il numero a mano, cosi' lo script
# non si rompe alla prossima versione di Csound.
CURRENT_VERSION=$(basename "$(readlink "$DEST/CsoundLib64.framework/Versions/Current")")

# "libs" nella radice del framework -> dentro Versions/<ver>/libs + symlink
# al suo posto (vedi punto 5 piu' sopra). -L la rileva gia' come symlink
# (es. se questa versione di Csound dovesse gia' metterla correttamente al
# suo posto in futuro): in tal caso non c'e' nulla da fare.
LIBS_DIR="$DEST/CsoundLib64.framework/libs"

if [ -d "$LIBS_DIR" ] && [ ! -L "$LIBS_DIR" ]; then
    mv "$LIBS_DIR" "$DEST/CsoundLib64.framework/Versions/$CURRENT_VERSION/libs"
    ln -s "Versions/Current/libs" "$LIBS_DIR"
fi

EMBEDDED_BIN="$DEST/CsoundLib64.framework/CsoundLib64"
NEW_ID="@rpath/CsoundLib64.framework/Versions/$CURRENT_VERSION/CsoundLib64"
OLD_ID=$(otool -D "$EMBEDDED_BIN" | tail -1)

if [ -n "$OLD_ID" ] && [ "$OLD_ID" != "$NEW_ID" ]; then
    # Rende la copia imbeddata rilocabile...
    install_name_tool -id "$NEW_ID" "$EMBEDDED_BIN"

    # ...e corregge eventuali altri binari DENTRO il framework stesso che la
    # referenzino col vecchio path assoluto (es. libcsnd6 nelle build 6.x -
    # non presente nella vera 7.0.0, ma teniamo il controllo per solidita'
    # nel caso tornasse in una release futura).
    for OTHER_BIN in "$DEST/CsoundLib64.framework/Versions/$CURRENT_VERSION"/lib*.dylib; do
        if [ -f "$OTHER_BIN" ]; then
            install_name_tool -change "$OLD_ID" "$NEW_ID" "$OTHER_BIN" || true
        fi
    done

    # ...e soprattutto corregge il NOSTRO eseguibile appena linkato, che a
    # link-time ha ricevuto il vecchio path assoluto come dipendenza.
    MAIN_BIN="$TARGET_BUILD_DIR/$EXECUTABLE_PATH"
    if [ -f "$MAIN_BIN" ]; then
        install_name_tool -change "$OLD_ID" "$NEW_ID" "$MAIN_BIN"
    fi
fi

# Firma ricorsiva (--deep) ad-hoc di tutto il bundle: copre il framework
# imbeddato (incluso libcsnd6) E il nostro eseguibile appena modificato da
# install_name_tool (che invalida qualunque firma precedente), in un solo
# comando finale, indipendente dall'ordine delle fasi di build di Xcode.
if [ -n "$WRAPPER_NAME" ] && [ -d "$TARGET_BUILD_DIR/$WRAPPER_NAME" ]; then
    codesign --force --deep --sign - "$TARGET_BUILD_DIR/$WRAPPER_NAME"
else
    codesign --force --deep --sign - "$DEST/CsoundLib64.framework"
fi

echo "CsoundLib64.framework imbeddato, rilinkato e firmato in: $DEST"
