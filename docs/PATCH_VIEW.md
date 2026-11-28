# Vista "Patch" di Algorithm (Max / Pure Data) - messa da parte

Stato: **sospesa** (25 novembre 2026). `main` è tornato a `4157d43`
("Menu: Show Algorithm is a toggle..."). Il lavoro è conservato nel branch
locale **`patch-view`** (ultimo commit `8fdc0a9`), non pushato.

Per riprenderla: `git checkout patch-view` (oppure `git merge patch-view`
su main). Mockup: `docs/mockup_patcher.html`.

## Cosa fa

- Sostituisce Map / Blocks / Flow con un patch come in Max / Pd. Due livelli:
  **Orchestra** (patch principale) e **uno strumento** (subpatch).
- Orchestra: righe dello score = message box `[i 1 0 10]`, strumenti e UDO
  = subpatch `[p instr 1]` (doppio tocco: si apre), bus globali / canali /
  eventi = cavi, `[dac~ N ch]`, badge verde con le note attive.
- Strumento:
  - opcode = oggetto su una riga con gli argomenti, ingressi sopra, uscite
    sotto, il segnale scende; cavi audio blu spessi, pvs tratteggiati,
    controllo grigi sottili;
  - `chnget` = number box con valore dal vivo; globali = `[r x]` / `[s x]`;
    commenti del codice accanto agli oggetti;
  - variabile verso molti oggetti (o lontana) = senza filo: etichetta
    sotto l'uscita e sopra ogni ingresso;
  - array = un oggetto `[kArr[]]`; indici (`kIndex`) non disegnati;
  - **vista Audio** (predefinita): solo il percorso del suono; i controlli
    che entrano sono etichette sopra gli ingressi; "Control objects too"
    nel menu disegna tutto;
  - ciò che non arriva a un'uscita: a parte, in grigio, con un titolo.
- Disposizione: strati dall'alto (Kahn + ALAP), baricentri per ridurre
  gli incroci, gruppi scollegati affiancati, a capo oltre ~1500 px.
- Restano: valore sul cavo (hover), sonda con oscilloscopio e Listen,
  Explain, codice accanto, aiuto + Trace con doppio tocco, zoom.

File: `Source/CsoundPatcher.h` (nuovo), modifiche in `CsoundGraphView.h`,
guida in `PluginEditor.cpp`, CHANGELOG / README / annuncio / App Store.
Provata: banco di prova con ASan/UBSan su 2315 csd, nessun crash. Mai
compilata in Xcode.

## Valutazione (perché è sospesa)

- Meglio del vecchio Flow per strumenti la cui idea è la catena audio
  (synth, effetti): instr 10 da >100 a 39 oggetti, leggibile dall'alto.
- Non basta per strumenti la cui idea sta nel controllo (instr 10 di
  `es1_prisma_piano2.csd`: nascita dei fantasmi, memoria d'attacco,
  rotazione sui canali): la vista Audio lo nasconde, "Control objects
  too" torna un groviglio.
- Mancano rispetto al Flow: il nome degli ingressi (`kM1 -> amp`), il
  colore del ruolo, la frase riassuntiva ("Audio path: ...").
- Le espressioni lunghe restano codice dentro una scatola.

## Idee per riprenderla

1. Nome dell'ingresso sulle etichette (`kM1 -> amp`).
2. Frase riassuntiva in cima.
3. Per il controllo: un modo diverso dal grafo (Explain, o un subpatch
   `[p controls]` come nel mockup).
4. Provarla su più csd reali prima di decidere.
