# apeCsound — ROADMAP

## TODO: su iOS, l'AUv3 anche come strumento (22 novembre 2026)

**Che cosa.** Oggi apeCsound su iOS è un solo Audio Unit: effetto con MIDI (`aumf`, codice
`Csnd`, produttore `Apes`). Si vede tra gli effetti, non tra gli **strumenti**. L'idea: la
stessa estensione AUv3 dichiarata **due volte** — l'effetto con MIDI che c'è già, più lo
strumento (`aumu`) — così un host la offre anche in uno slot strumento.

**Già fatto e provato in Density 27** (stesso giorno): lì l'estensione nasce strumento e uno
script le aggiunge l'effetto con MIDI. L'utente ha verificato su un dispositivo che l'host
vede tutte e due le voci. Qui è lo stesso lavoro, al contrario.

**Come.** Nessuna modifica al codice:

- il wrapper AUv3 di JUCE non distingue strumento ed effetto: crea lo stesso plug-in
  qualunque tipo l'host chieda;
- `CsoundAudioProcessor::isBusesLayoutSupported` accetta già l'ingresso disattivato (uno
  slot strumento non dà audio in ingresso).

Passi:

- [ ] Copiare `DENSITY27/tools/ios/add_fx_component.sh` in `scripts/ios/` e adattarlo: agire
      solo se la prima voce è `aumf`; la copia diventa `type aumu`, un **codice nuovo** (per
      esempio `Csni`: mai lo stesso codice con due tipi), nome "apeSoft: apeCsound (Instr)",
      `tags` = `Synth`.
- [ ] In `apeCsound.jucer`, exporter iOS, campo *Post-Build Shell Script*:
      `sh "$PROJECT_DIR/../../scripts/ios/<script>.sh"` (controllare quanti `..` servono da
      `Builds/iOS`). Lo script gira per ogni target e tocca solo l'`.appex`.
- [ ] *Save Project* in Projucer, build per iOS. Nel log deve comparire la riga dello script.
- [ ] Controllare il plist dell'estensione **costruita** (non quello del progetto, che ha
      sempre una voce sola): due voci in `NSExtension > NSExtensionAttributes > AudioComponents`.
- [ ] Provare su un dispositivo: lo strumento compare tra gli strumenti (AUM, GarageBand,
      Cubasis), suona con una `.csd` che non usa l'ingresso audio, riceve il MIDI.
- [ ] Decidere i nomi che si leggono nell'host e aggiornare README e note di rilascio.

**Da sapere.**

- Una `.csd` che legge l'ingresso audio (`inch`, `ins`) in uno slot strumento riceve silenzio:
  dirlo nel manuale.
- Lo stato salvato in un progetto dell'host è legato al codice dell'Audio Unit: un progetto
  salvato con l'effetto non si riapre come strumento, e viceversa.
- Se serve anche l'effetto semplice senza MIDI (`aufx`), per gli host che non mostrano
  l'`aumf`, è una voce in più nello stesso script.
- Su macOS l'AUv3 ha lo stesso plist: valutare se farlo anche lì (stesso script, exporter
  macOS).

**Icona dell'AUv3 su iOS.** In Density 27 l'icona non compariva nell'host. Lì si è passati a
un catalogo proprio (*Custom Xcassets Folder* nell'exporter iOS), con icone senza canale alfa:
Projucer, quando rigenera le icone dal PNG, aggiunge l'alfa, e Apple rifiuta l'icona da 1024
con l'alfa. Controllare se apeCsound ha lo stesso difetto.
