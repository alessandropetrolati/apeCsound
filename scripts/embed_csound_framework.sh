#!/bin/sh
# Copia CsoundLib64 dentro il bundle del plugin/app e firma tutto - lanciato
# come postbuildCommand ad OGNI build. Dopo questo script non serve nessun
# comando a mano in Terminale.
#
# Niente piu' install_name_tool/@rpath: il plugin non LINKA piu' Csound a
# tempo di compilazione (niente extraCustomFrameworks nel .jucer), la
# carica lui stesso a runtime con dlopen() - vedi Source/CsoundDynamicLib.
# h/.cpp per il perche' (linkare il framework costringeva Xcode a gestire
# da solo install name/@rpath/fase "Embed Frameworks", una catena fragile
# che e' stata fonte di problemi multipli: install_name_tool, codesign su
# libcsnd6 non firmato, "unsealed contents" per la cartella "libs" che
# viola le regole Apple sui framework imbeddati, LD_RUNPATH_SEARCH_PATHS).
# Senza un LC_LOAD_DYLIB da correggere, questo script si riduce a tre
# passi: copia, pulizia, firma.
#
# Puo' anche essere lanciato a mano per debug, impostando a mano le
# variabili che Xcode fornisce da solo durante il build, es.:
#   CODESIGNING_FOLDER_PATH=/path/al/Csound.app \
#   TARGET_BUILD_DIR=/path/build WRAPPER_NAME=Csound.app \
#   ./embed_csound_framework.sh

set -e

FRAMEWORK_SRC="/Users/alessandropetrolati/Desktop/Advanced 2018/Csound/Csound/CsoundLib64.framework"
DEST_CONTENTS="$CODESIGNING_FOLDER_PATH/Contents"

# Il target "Shared Code" (libreria statica interna di JUCE, non un bundle)
# non ha una cartella Contents: niente da copiare, usciamo senza errore.
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

# Csound mette anche una cartella "libs" (librerie di terze parti: liblo,
# libportaudio, libportmidi) DIRETTAMENTE nella radice del framework, non
# dentro Versions/<ver>/: Apple non lo permette (un framework imbeddato
# puo' avere nella radice solo i soliti symlink verso Versions/Current), e
# "codesign --deep" falliva con "unsealed contents present in the root
# directory of an embedded framework". Risolto spostando "libs" dentro
# Versions/<ver>/libs e rimettendo al suo posto un normale symlink,
# esattamente come fa il framework stesso per CsoundLib64/Headers/
# Resources - i path relativi tipo "@loader_path/../libs/..." gia'
# presenti nei vari binari (CsoundLib64 stesso, i plugin opcode in
# Opcodes64) continuano a funzionare identici, perche' per dyld un symlink
# e' del tutto trasparente.
CURRENT_VERSION=$(basename "$(readlink "$DEST/CsoundLib64.framework/Versions/Current")")
LIBS_DIR="$DEST/CsoundLib64.framework/libs"

if [ -d "$LIBS_DIR" ] && [ ! -L "$LIBS_DIR" ]; then
    mv "$LIBS_DIR" "$DEST/CsoundLib64.framework/Versions/$CURRENT_VERSION/libs"
    ln -s "Versions/Current/libs" "$LIBS_DIR"
fi

# Firma ricorsiva (--deep) ad-hoc di tutto il bundle: copre il framework
# appena copiato (incluso qualunque dylib di terze parti al suo interno),
# in un solo comando finale, indipendente dall'ordine delle fasi di build
# di Xcode.
if [ -n "$WRAPPER_NAME" ] && [ -d "$TARGET_BUILD_DIR/$WRAPPER_NAME" ]; then
    codesign --force --deep --sign - "$TARGET_BUILD_DIR/$WRAPPER_NAME"
else
    codesign --force --deep --sign - "$DEST/CsoundLib64.framework"
fi

echo "CsoundLib64.framework copiato e firmato in: $DEST (caricato a runtime via dlopen, non linkato)"
