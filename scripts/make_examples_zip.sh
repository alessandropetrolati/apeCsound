#!/usr/bin/env bash
#
# Crea Resources/Examples.zip, gli esempi di fabbrica compilati dentro
# apeCsound (BinaryData::Examples_zip, vedi apeCsound.jucer) ed estratti al
# primo avvio in apeCsound/CSD/Examples (CsoundAudioProcessor::installExamples).
#
# Sorgente predefinita: ~/Documents/apeCsound/CSD/Examples (oppure il primo
# argomento). Tutto il contenuto della sorgente finisce nella radice dello
# zip (es. ape/, Cabbage/, CsoundQT/, README_Examples.md): per aggiungere una
# raccolta basta creare una sottocartella.
#
# SSDIR/SFDIR/SADIR/INCDIR NON vanno nello zip: sono dati dell'utente e
# apeCsound le crea da solo in ~/Documents/apeCsound.
#
# Dopo averlo rigenerato: aumenta kExamplesVersion in PluginProcessor.cpp,
# cosi' chi ha gia' installato apeCsound riceve gli esempi nuovi (senza
# sovrascrivere quelli che ha modificato).
#
# Usage:  ./make_examples_zip.sh [cartella-sorgente]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SRC="${1:-$HOME/Documents/apeCsound/CSD/Examples}"
OUT="$PROJECT_ROOT/Resources/Examples.zip"

# Raccolte obbligatorie (controllo contro cancellazioni accidentali)
for item in ape Cabbage CsoundQT README_Examples.md; do
  [ -e "$SRC/$item" ] || { echo "ERROR: $SRC/$item not found"; exit 1; }
done

# Tutto il contenuto della sorgente, esclusi file nascosti e cartelle di lavoro
ITEMS=()
while IFS= read -r -d '' p; do
  name="$(basename "$p")"
  case "$name" in
    .*|__MACOSX|SSDIR|SFDIR|SADIR|INCDIR|User) continue ;;
  esac
  ITEMS+=("$name")
done < <(find "$SRC" -mindepth 1 -maxdepth 1 -print0 | sort -z)

rm -f "$OUT"
# -X: niente attributi extra; esclusi i metadati macOS (niente __MACOSX)
(cd "$SRC" && zip -qrX "$OUT" "${ITEMS[@]}" -x "*/.DS_Store" ".DS_Store" "*/__MACOSX/*")

echo "Contents: ${ITEMS[*]}"
echo "Created $OUT ($(du -h "$OUT" | cut -f1))"
unzip -l "$OUT" | tail -1
