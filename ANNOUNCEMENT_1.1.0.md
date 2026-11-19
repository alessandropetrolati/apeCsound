# apeCsound 1.1.0 — see how a Csound patch works

**apeCsound 1.1.0** is out for macOS, iPadOS and iOS (AU, AUv3, VST3 and standalone).

This release adds **Algorithm**, a new view that helps you *read* a `.csd`: not another editor, but a guide that shows how the code turns into sound. Open it with **Show Algorithm** (Cmd+6). It sits beside the code, follows what you write and never changes it.

## From the orchestra to a single line

- **Orchestra map** — the whole file at a glance: which instrument starts which, what comes from the score, MIDI and your sliders, what goes to the speakers, and — while Csound plays — how many notes each instrument is playing.
- **Blocks** — every instrument opens as a few functional blocks with musical names: controls, sound sources, shaping in time, spectral processing, transformation, space and effects, output. The comments of your code become their captions.
- **Boxes** — tap a block and you get its opcodes drawn like the block diagram of a synth, from the sources to the outputs, with ports to the neighbouring blocks. Pinch to zoom.
- **Explain** — a plain-English sentence beside every line (*"envelope: rises to 1 in 5 ms, then falls over the rest of the note"*), with the values resolved — which note from the score, which table, drawn next to the line — and a badge that says when each line runs: once at note start (i), at every control period (k), on every sample (a). Step through the code in the order Csound runs it.

Navigation is as simple as the Settings of an iPhone: start from the orchestra, tap to go in, one button to go back.

## Watch and listen to the signals

While your patch plays, touch any cable to read its value, open an oscilloscope, or press **Listen** to hear that signal alone — the oscillator before the filter, then after it — without stopping or recompiling anything. **Trace** shows where a signal comes from and where it ends, in one tap.

## Csound 7 ready

The new view understands the modern Csound 7 syntax: `for` loops, `switch`, typed variables, structs and new-style opcodes. It has been tested on more than 2,300 `.csd` files, including the whole Csound 7 test suite.

## Also in this release

Everything in the 1.0.8 update — the virtual MIDI keyboard, CSD_DIR / CSD_PATH channels, undo for parameter moves, a silent Pause, and many fixes — is of course included. The full list is in the release notes (CHANGELOG.txt) and the built-in **apeCsound Guide** has a new chapter on Algorithm.

apeCsound is developed by apeSoft (www.apesoft.it). Csound is free software distributed under the GNU LGPL.

---

# apeCsound 1.1.0 — capire come funziona una patch Csound

È disponibile **apeCsound 1.1.0** per macOS, iPadOS e iOS (AU, AUv3, VST3 e standalone).

La novità è **Algorithm**, una vista che aiuta a *leggere* un `.csd`: non un altro editor, ma una guida che mostra come il codice diventa suono. Si apre con **Show Algorithm** (Cmd+6), sta accanto al codice, segue quello che scrivi e non lo modifica mai.

- **Mappa dell'orchestra**: tutto il file in un colpo d'occhio — chi avvia chi, cosa arriva da score, MIDI e slider, cosa va alle uscite e quante note suona ogni strumento mentre Csound è in esecuzione.
- **Blocchi**: ogni strumento si apre in pochi blocchi con nomi musicali (controlli, sorgenti, forma nel tempo, elaborazione spettrale, trasformazione, spazio ed effetti, uscita); i commenti del tuo codice diventano le loro didascalie.
- **Scatole**: tocchi un blocco e vedi i suoi opcode disegnati come lo schema a blocchi di un synth, dalle sorgenti alle uscite. Pinch per lo zoom.
- **Explain**: una frase accanto a ogni riga, i valori risolti (quale nota dallo score, quale tabella, disegnata) e quando gira ogni riga — una volta all'inizio della nota (i), a ogni ciclo di controllo (k), a ogni campione (a). Si può seguire il codice passo passo nell'ordine in cui Csound lo esegue.

La navigazione è semplice come le Impostazioni dell'iPhone: si parte dall'orchestra, si tocca per entrare, un pulsante per tornare indietro.

Mentre la patch suona, tocchi un cavo per leggerne il valore, vedere l'oscilloscopio o premere **Listen** per ascoltare solo quel segnale, senza fermare né ricompilare nulla. **Trace** mostra da dove arriva un segnale e dove finisce.

La vista comprende la sintassi di Csound 7 (`for`, `switch`, variabili con tipo, struct, opcode nel nuovo stile) ed è stata provata su oltre 2.300 file `.csd`, compresa l'intera suite di test di Csound 7.

Tutte le novità della 1.0.8 (tastiera MIDI virtuale, canali CSD_DIR / CSD_PATH, undo dei parametri, Pause silenziosa e molte correzioni) sono incluse. L'elenco completo è nelle note di versione (CHANGELOG.txt) e la **apeCsound Guide** integrata ha un nuovo capitolo su Algorithm.
