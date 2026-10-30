# apeCsound – App Store Connect metadata

Character limits are checked (App Store Connect: Name 30, Subtitle 30, Promotional Text 170, Keywords 100, Description 4000).
Keywords do not repeat words already in Name/Subtitle (Apple indexes those automatically).

---

## English (U.S.) – primary

**Name** (9/30)
apeCsound

**Subtitle** (27/30)
Csound 7 synth & FX in AUv3

**Promotional Text** (158/170)
Write Csound code, press Apply and play it as an AUv3 instrument or effect in your favourite host. Parameters from your .csd become host-automatable controls.

**Keywords** (≤100)
synthesizer,audio unit,dsp,sound design,live coding,effects,midi,modular,granular,algorithmic,fm

**Description**

apeCsound brings the full power of Csound 7, one of the most complete sound synthesis and signal processing languages ever created, inside your music apps.

Write or paste any Csound orchestra, press Apply, and apeCsound turns it into an Audio Unit (AUv3) instrument or effect you can play, record and automate in GarageBand, Logic Pro for iPad, AUM, Cubasis, Loopy Pro and any other AUv3 host. It also runs as a standalone app.

THE CSOUND ENGINE, BUILT IN
• Csound 7 is embedded in the app: nothing else to install
• Thousands of opcodes: oscillators, filters, physical models, granular, spectral (pvs), FM, waveguides, reverbs, delays and much more
• Process live audio from the host as an effect, or play it as a MIDI instrument
• Audio channels follow the host layout automatically

PARAMETERS YOUR HOST CAN AUTOMATE
• Describe sliders, switches and menus directly in your .csd file
• apeCsound builds the control panel for you and exposes every parameter to the host for automation and MIDI learn
• Values are read in Csound with a simple chnget
• Buttons can be switches or momentary, and can start instruments with a score line on press and release
• A Monitor shows what your code writes: meters in dB with clip, dials, lamps, step lights, text and sparklines
• Undo and redo for every change, with one shared history

A CODE EDITOR MADE FOR CSOUND
• Syntax highlighting with dark and light themes, line numbers and adjustable text size
• Opcode completion that inserts the complete syntax, with outputs and optional arguments, in classic or modern (functional) style
• Jump between arguments with Tab
• Inline help bar showing the syntax of the opcode under the cursor
• Jump to the definition of a variable, instrument, opcode or channel, and back
• Find and replace, comment/uncomment, smart indentation
• Designed for touch: native-style selection handles, magnifier, edit menu and long-press caret navigation
• Built-in link to the Csound reference manual

SESSIONS AND FILES
• Open and save standard .csd files from Files, iCloud Drive or any document provider
• Your code and parameters are saved with the host project
• Start from a ready-made template or from a blank session
• Import CsoundQt widgets as parameters

CONSOLE
• All Csound messages and errors in a dedicated console, so you can see exactly what is happening

Whether you are a sound designer, a composer, a teacher or a student, apeCsound turns your iPad and iPhone into a complete Csound workstation that lives inside your favourite music apps.

Csound is free software distributed under the GNU LGPL. apeCsound is developed by apeSoft and is not affiliated with the Csound developers.

**What's New in This Version (1.0.7)**
apeCsound 1.0.7 makes your patches easier to play, to watch and to read.

BUTTONS THAT PLAY YOUR INSTRUMENTS
• Every switch parameter is now a Button with two modes. Toggle works like a switch: tap it on, tap it off. Momentary is on only while you hold it, like a key or a sustain pedal - and your host records the press and the release, so automation plays them back exactly.
• EVENT ON and EVENT OFF: give a Button a score line and apeCsound sends it for you when the button is pressed and when it is released. "i 10 0 2" plays a two-second note at each tap. With a held note ("i 10.1 0 -1" on press, "i -10.1 0 0" on release) the button sounds as long as you hold it. No code to write, and it works with host automation and MIDI learn.
• Prefer to do it in Csound? Copy Trigger Code, in the menu of any Button, gives you the few lines that start an instrument once per press, ready to paste.
• A cleaner configuration with one-tap selectors for Toggle / Momentary and the initial state.

A NEW MONITOR FOR WHAT YOUR CODE WRITES
• Every channel your patch writes with chnset now gets the right display by itself - nothing to declare: a level bar for a declared range, a sliding marker when there is no range, a dial with a needle for values around zero (LFOs, pan), a row of step lights for a sequencer position, a glowing lamp for on/off values, a display for text, a sparkline of the last seconds.
• Audio channels become real level meters in dB, with peak hold and a CLIP light that stays on until you tap it, so you never miss an overload.
• Numbered channels (Band1, Band2...) and stereo pairs (Out_L / Out_R) are joined in one card; compact mode fits dozens of channels on the screen.
• Touch and hold a card to see what it is, reset what it has measured, or copy a ready-made chn_k line with the range it has seen.
• A card that never changes is dimmed and tells you why: usually a channel your code creates but never writes.
• Note for existing patches: the Monitor attributes of version 1.0.5 (db, int, hidden, group:) are no longer needed or read. Your patches still run unchanged; only the look of those cards changes. The Guide explains the new way.

FIND YOUR WAY IN THE CODE
• Jump to Definition: select a variable, an instrument number, a user-defined opcode, a macro or a channel name and choose Definition to go where it is defined - or Cmd+click with a keyboard and trackpad. A Back button takes you right back to where you were.

APECSOUND REMEMBERS
• Your layout is saved with your project: which panels are open, the collapsed groups, the Monitor mode and where you were in the code. New sessions start from the layout you used last, and Config > Reset Layout brings back the default one.

BETTER IMPORT, NEW EXAMPLES
• Cabbage and CsoundQt files: momentary buttons are now recognised, CsoundQt event buttons become Buttons with their score line - including held notes that stop when you release them - and are named after their label.
• New ButtonTest example with every way to use a Button, a rewritten MonitorTest that explains each kind of display, and a CsoundQt test file. The built-in Guide has new chapters on Buttons, the Monitor and code navigation.

Plus many smaller fixes and refinements throughout the app.

---

## Italiano

**Nome** (9/30)
apeCsound

**Sottotitolo** (30/30)
Synth ed effetti Csound 7 AUv3

**Testo promozionale** (155/170)
Scrivi codice Csound, premi Apply e suonalo come strumento o effetto AUv3 nel tuo host preferito. I parametri del .csd diventano controlli automatizzabili.

**Parole chiave** (≤100)
sintetizzatore,audio unit,dsp,sound design,live coding,effetti,midi,modulare,granulare,algoritmica

**Descrizione**

apeCsound porta tutta la potenza di Csound 7, uno dei linguaggi più completi mai creati per la sintesi e l'elaborazione del suono, dentro le tue app musicali.

Scrivi o incolla un'orchestra Csound, premi Apply e apeCsound la trasforma in uno strumento o effetto Audio Unit (AUv3) da suonare, registrare e automatizzare in GarageBand, Logic Pro per iPad, AUM, Cubasis, Loopy Pro e in qualsiasi altro host AUv3. Funziona anche come app autonoma.

IL MOTORE CSOUND, INTEGRATO
• Csound 7 è incluso nell'app: non serve installare nient'altro
• Migliaia di opcode: oscillatori, filtri, modelli fisici, granulare, spettrale (pvs), FM, guide d'onda, riverberi, delay e molto altro
• Elabora l'audio dell'host come effetto, o suonalo come strumento MIDI
• I canali audio seguono automaticamente la configurazione dell'host

PARAMETRI AUTOMATIZZABILI DALL'HOST
• Descrivi slider, interruttori e menu direttamente nel file .csd
• apeCsound costruisce il pannello di controllo e rende ogni parametro disponibile all'host per l'automazione e il MIDI learn
• In Csound i valori si leggono con un semplice chnget
• I pulsanti possono essere interruttori o momentanei, e possono avviare strumenti con una riga di score alla pressione e al rilascio
• Un Monitor mostra ciò che il codice scrive: meter in dB con clip, quadranti, spie, luci a passi, testo e sparkline
• Annulla e ripeti per ogni modifica, con un'unica cronologia condivisa

UN EDITOR DI CODICE PENSATO PER CSOUND
• Evidenziazione della sintassi con tema scuro e chiaro, numeri di riga e dimensione del testo regolabile
• Completamento degli opcode che inserisce la sintassi completa, con uscite e argomenti opzionali, in stile classico o moderno (funzionale)
• Passa da un argomento all'altro con Tab
• Barra di aiuto con la sintassi dell'opcode sotto il cursore
• Vai alla definizione di una variabile, di uno strumento, di un opcode o di un canale, e torna indietro
• Cerca e sostituisci, commenta/decommenta, indentazione intelligente
• Progettato per il touch: maniglie di selezione in stile nativo, lente d'ingrandimento, menu di modifica e navigazione del cursore con pressione prolungata
• Collegamento diretto al manuale di riferimento di Csound

SESSIONI E FILE
• Apri e salva file .csd standard da File, iCloud Drive o qualsiasi altro fornitore di documenti
• Codice e parametri vengono salvati insieme al progetto dell'host
• Parti da un modello pronto o da una sessione vuota
• Importa i widget di CsoundQt come parametri

CONSOLE
• Tutti i messaggi e gli errori di Csound in una console dedicata, per vedere esattamente cosa succede

Che tu sia sound designer, compositore, docente o studente, apeCsound trasforma iPad e iPhone in una postazione Csound completa che vive dentro le tue app musicali preferite.

Csound è software libero distribuito con licenza GNU LGPL. apeCsound è sviluppato da apeSoft e non è affiliato agli sviluppatori di Csound.

**Novità di questa versione (1.0.7)**
PULSANTI CHE SUONANO
• Toggle o Momentary: un pulsante Momentary è acceso solo finché lo tieni premuto, e l'host registra pressione e rilascio
• EVENT ON / EVENT OFF: una riga di score inviata alla pressione e al rilascio - con una nota tenuta il pulsante suona come un tasto, senza scrivere codice
• Oppure lascia che sia il codice a reagire: Copy Trigger Code ti dà le righe da incollare

UN NUOVO MONITOR
• Ogni canale scritto dal codice ha da solo la visualizzazione giusta: barra di livello, quadrante, luci a passi, spia, testo, sparkline e meter audio in dB con spia di clip
• Coppie stereo e canali numerati diventano una sola card; la modalità compatta ne mostra decine
• Tieni premuta una card per copiare una riga chn_k pronta con il suo range
• Nota: gli attributi del Monitor della 1.0.5 (db, int, hidden, group:) non sono più usati - vedi la Guida

EDITOR DI CODICE
• Vai alla definizione: seleziona un nome e scegli Definition (o Cmd+clic con il trackpad) per raggiungere dove è definita una variabile, uno strumento, un opcode, una macro o un canale, e Back per tornare

E ANCORA
• apeCsound ricorda il layout con il progetto: pannelli aperti, gruppi e la posizione nel codice
• Import migliore dei pulsanti di Cabbage e CsoundQt, compresi i pulsanti event di CsoundQt
• Nuovi esempi: ButtonTest e un MonitorTest aggiornato, con spiegazioni
• Molte correzioni

---

## Other fields

- **Primary category:** Music · **Secondary:** Developer Tools (or Education)
- **Support URL:** https://www.apesoft.it
- **Marketing URL:** https://www.apesoft.it
- **Copyright:** 2026 apeSoft
- **Age rating:** 4+ (no objectionable content)
