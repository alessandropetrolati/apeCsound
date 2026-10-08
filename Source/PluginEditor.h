#pragma once

#include <JuceHeader.h>
#include <vector>
#include "PluginProcessor.h"
#include "CsoundTokeniser.h"
#include "CsoundCodeEditor.h"
#include "CsoundLookAndFeel.h"
#include "CsoundParameterEditor.h"

/**
    Editor del plugin/standalone (juce::AudioProcessorEditor): editor con
    syntax highlighting + console log + un bottone "Apply", agganciata a
    CsoundAudioProcessor. JUCE la istanzia da
    CsoundAudioProcessor::createEditor() sia nella Standalone app sia
    quando/se il plugin verra' aperto in una DAW.

    Il motore Csound e' sempre in esecuzione (lo avvia da solo
    CsoundAudioProcessor::prepareToPlay, col testo salvato, appena il plugin
    viene caricato): non c'e' un vero Run/Stop. Il bottone "Apply" si
    limita a rimpiazzare il .csd attualmente in esecuzione con quello appena
    modificato nell'editor - non "avvia" nulla che non sia gia' partito.

    Il testo del .csd e' persistito in getStateInformation/setStateInformation
    (CsoundAudioProcessor), quindi una DAW che salva e ricarica il progetto
    ritrova lo stesso codice; CsoundAudioProcessor::prepareToPlay lo compila
    e avvia automaticamente, cosi' l'audio funziona di nuovo senza dover
    premere "Apply" a mano dopo un reload.

    La logica di binding dei widget (GUI designer, eventualmente da
    rivalutare in futuro) andra' agganciata a
    CsoundAudioProcessor::setControlChannel / getControlChannel, gia'
    pronti qui tramite `audioProcessor`.

    Il bottone "Parametri..." non apre una finestra separata (una
    juce::DocumentWindow a parte finiva in background dietro l'host in
    diverse DAW/wrapper, essendo un top-level separato dalla finestra del
    plugin): mostra invece CsoundParameterMappingPanel come una "finestra"
    DENTRO i confini di questo editor (un normale juce::Component, non un
    vero top-level) che pero' si comporta come tale - SPOSTABILE
    trascinandone la barra del titolo (vedi CsoundParameterMappingPanel::
    mouseDown/mouseDrag), non un overlay modale fisso: niente velo che
    scurisce o blocca il resto dell'editor, codice/consolle restano sempre
    interagibili sotto/intorno ad essa, cosi' si puo' trascinare la maniglia
    "#N" di una riga direttamente sull'editor di codice anche col pannello
    aperto. Non sostituisce piu' l'area della consolle come in una versione
    precedente: consolle ed editor restano sempre al loro posto. Qui si
    definisce il "rename" (nome canale Csound) e i metadata (range/default/
    skew/increment per i parametri float, default per i bool, etichette/
    indice per i choice) dei 64 slot apvts, isolati in tab separate dentro il pannello
    (vedi CsoundParameterMappingPanel). I VALORI restano affidati
    all'automazione host o a una UI dedicata futura, qui si editano solo i
    metadata per slot.

    Trascinando la maniglia "#N" di una riga del pannello sull'editor di
    codice si inserisce automaticamente un chnget per quel canale (vedi
    CsoundCodeEditor e CsoundParameterMappingPanel::ParamRow) - per questo
    l'intero editor eredita anche juce::DragAndDropContainer, il mixin che
    coordina drag and drop tra componenti della stessa finestra.
*/
class CsoundAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          public juce::DragAndDropContainer,
                                          public juce::FileDragAndDropTarget,
                                          private CsoundAudioProcessor::Listener,
                                          private juce::CodeDocument::Listener
{
public:
    explicit CsoundAudioProcessorEditor (CsoundAudioProcessor& p);
    ~CsoundAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Cmd+Z (Ctrl+Z su Windows)/Cmd+Shift+Z: annullano/ripetono l'ULTIMA
    // operazione in ordine CRONOLOGICO su sharedUndoManager sotto, SIA che
    // sia stata una modifica di testo nell'editor, SIA una modifica di
    // mappatura/metadata nel pannello Parametri - richiesta esplicita: "una
    // linearita' avanti e indietro tra l'editor e le configurazioni dei
    // Parametri", un'UNICA cronologia, non due scollegate. Vedi il
    // commento su performUndo()/performRedo() e su sharedUndoManager piu'
    // sotto per il quadro completo.
    bool keyPressed (const juce::KeyPress& key) override;

    // Richiamate sia da keyPressed() sopra sia dalla voce di menu "Undo"/
    // "Redo" del pannello Parametri (showPanelMenu()) sia da
    // CsoundCodeEditor::onUndoRequested/onRedoRequested (impostate nel
    // costruttore sotto - CsoundCodeEditor intercetta Cmd+Z/Cmd+Shift+Z
    // PRIMA che la classe base JUCE possa consumarli da sola per il
    // proprio undo testuale interno): in TUTTI i casi, semplicemente
    // sharedUndoManager.undo()/redo(). Nessuna euristica su chi ha il
    // focus - vedi sharedUndoManager piu' sotto sul perche' non serve piu'.
    void performUndo();
    void performRedo();

private:
    // CsoundAudioProcessor::Listener
    void csoundMessageReceived (const juce::String& message) override;
    void csoundEngineStarted() override;
    void csoundEngineStopped() override;

    // juce::CodeDocument::Listener: CsoundCodeEditor (vedi "editor" sotto)
    // ha GIA' un proprio listener privato sullo stesso document (per l'help
    // inline) - juce::CodeDocument supporta piu' listener indipendenti sullo
    // stesso documento, quindi questo secondo qui non interferisce. Oltre
    // ad aggiornare il bordo rosso di Apply (updateApplyButtonDirtyState),
    // fa anche da punto di ingresso per bridgeCodeEditIntoSharedUndo()
    // sotto - vedi li' per il perche'.
    void codeDocumentTextInserted (const juce::String&, int) override { updateApplyButtonDirtyState(); bridgeCodeEditIntoSharedUndo(); }
    void codeDocumentTextDeleted (int, int) override                  { updateApplyButtonDirtyState(); bridgeCodeEditIntoSharedUndo(); }

    // Rispecchia (quando serve, vedi l'implementazione nel .cpp) la
    // transazione CORRENTE dell'UndoManager interno di "document" come
    // UN'UNICA voce in sharedUndoManager sotto - cosi' una modifica di
    // testo e una modifica di un parametro restano intrecciate in un
    // ordine cronologico coerente in UNA SOLA cronologia (richiesta
    // esplicita). Vedi il commento esteso nel .cpp.
    void bridgeCodeEditIntoSharedUndo();

    // Guardia di rientranza per bridgeCodeEditIntoSharedUndo() sopra - vedi
    // il commento nel .cpp su CodeEditTransactionProxy.
    bool isApplyingCodeUndoRedo = false;

    // Confronta il testo ATTUALE dell'editor con l'ultimo testo applicato
    // nel processor (audioProcessor.getCsdText()): se sono diversi, il
    // bottone Apply viene circondato da un bordo rosso (vedi
    // CsoundLookAndFeel::drawButtonBackground, che legge la proprieta'
    // dinamica "pendingChanges" sul bottone) per segnalare che il codice e'
    // stato modificato e non coincide piu' con quello in esecuzione - vedi
    // il listener del document sopra. Dopo un Load CSD questo confronto
    // risulterebbe "non modificato" (il document e' stato appena
    // sincronizzato col testo caricato), ma l'utente non ha ancora premuto
    // Apply su QUESTO codice - vedi markApplyPendingAfterLoad() sotto, che
    // forza il bordo rosso in quel caso specifico invece di chiamare questa.
    void updateApplyButtonDirtyState();

    // Promemoria visivo dopo un Load CSD: anche se il document e' appena
    // stato sincronizzato col testo del file caricato (quindi
    // updateApplyButtonDirtyState() sopra lo giudicherebbe "non
    // modificato"), caricare un .csd e' comunque un cambio di codice che
    // l'utente non ha confermato premendo Apply - il bordo rosso resta
    // quindi acceso finche' non si preme Apply esplicitamente, anche se il
    // motore e' gia' stato ricompilato in automatico (vedi
    // CsoundAudioProcessor::restoreStateFromTree).
    void markApplyPendingAfterLoad();

    void appendToLog (const juce::String& text);

    // Barra fissa (non un popup) tra l'editor e la consolle: mostra
    // sintassi + descrizione dell'opcode su cui si trova il caret
    // (digitato, cliccato, o raggiunto con le freccie), alimentata da
    // CsoundCodeEditor::onOpcodeHelpChanged.
    struct OpcodeHelpBar final : public juce::Component
    {
        void paint (juce::Graphics& g) override;
        void setHelpText (const juce::String& syntax, const juce::String& description);

    private:
        juce::String syntaxText, descriptionText;
    };

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    CsoundAudioProcessor& audioProcessor;

    juce::CodeDocument document;
    CsoundTokeniser tokeniser;
    CsoundCodeEditor editor { document, &tokeniser };

    OpcodeHelpBar opcodeHelpBar;
    static constexpr int opcodeHelpBarHeight = 26;

    juce::TextEditor logConsole;

    // Cronologia di Undo/Redo UNICA e CONDIVISA fra l'editor di codice e il
    // pannello Parametri (richiesta esplicita, vedi il commento su
    // performUndo()/performRedo() sopra) - rimpiazza la vecchia euristica
    // basata sul focus. Dichiarata QUI, PRIMA di parameterPanel sotto (che
    // ne riceve un riferimento al proprio costruttore): l'ordine di
    // dichiarazione dei membri e' anche l'ordine di inizializzazione in
    // C++, quindi deve esistere gia' quando parameterPanel viene costruita.
    // NON e' in alcun modo collegata a audioProcessor.apvts: i valori dei
    // parametri (slider/toggle/combo) sono gestiti dalla DAW/host
    // (automazione, il proprio undo se ce l'ha) e NON passano piu' da
    // qui - vedi il commento sul costruttore di GenericParamRow in
    // CsoundParameterEditor.h sul perche'. Questa cronologia copre SOLO:
    // (1) le transazioni di testo dell'editor di codice (vedi
    // bridgeCodeEditIntoSharedUndo() sopra), (2) le modifiche di
    // mappatura/metadata del pannello Parametri (nome canale, min/max,
    // opzioni, aggiunta/rimozione di un parametro).
    juce::UndoManager sharedUndoManager;

    // Pannello del mapping parametri (rename canale + metadata per slot):
    // nascosto di default, mostrato come SIDEBAR ANCORATA a destra (non
    // piu' una finestra flottante spostabile - cambiato su richiesta
    // esplicita: l'overlay centrato "non convinceva") quando si attiva la
    // voce "Show Parameters" del menu hamburger (non piu' un bottone
    // dedicato nella toolbar, richiesta esplicita) - vedi
    // showingParameterPanel e toggleParameterPanel() in PluginEditor.cpp.
    // editor/opcodeHelpBar/logConsole restano SEMPRE a
    // sinistra della sidebar, mai coperti: resized() si limita a
    // restringere la loro larghezza di sidebarWidth + sidebarDividerWidth
    // quando la sidebar e' visibile, cosi' sia l'editing del codice sia il
    // drag della maniglia "#N" sull'editor restano sempre possibili.
    CsoundParameterMappingPanel parameterPanel { audioProcessor, sharedUndoManager };
    bool showingParameterPanel = true;
    void toggleParameterPanel();

    // Larghezza corrente della sidebar (ridimensionabile trascinando
    // sidebarDivider sotto) - inizializzata a una larghezza stretta ma
    // comoda (le card si adattano comunque, vedi wrap dinamico dei campi),
    // poi limitata in resized() tra sidebarMinWidth e uno spazio che lascia
    // comunque sidebarEditorMinWidth all'editor/consolle.
    int sidebarWidth = 360;
    static constexpr int sidebarMinWidth = 280;
    static constexpr int sidebarEditorMinWidth = 300;
    static constexpr int sidebarDividerWidth = 6;

    // Striscia verticale draggabile tra editor/consolle e la sidebar:
    // cattura il proprio mouseDown/mouseDrag per ridimensionare sidebarWidth
    // (vedi .cpp) invece di spostare l'intero pannello come succedeva prima
    // con la barra del titolo della vecchia finestra flottante.
    struct SidebarDivider final : public juce::Component
    {
        SidebarDivider();
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;

        // getCurrentWidth fornisce la larghezza attuale al mouseDown (per
        // calcolare il delta rispetto al punto di partenza del trascinamento);
        // onDrag riceve poi la nuova larghezza desiderata ad ogni mouseDrag -
        // il clamp (sidebarMinWidth/sidebarEditorMinWidth) resta a resized()
        // del genitore, non a questo componente.
        std::function<int()> getCurrentWidth;
        std::function<void (int)> onDrag;

    private:
        int widthAtDragStart = 0;
    };
    SidebarDivider sidebarDivider;

    // Altezza corrente della consolle (ridimensionabile trascinando
    // consoleDivider sotto, richiesto esplicitamente) - stesso meccanismo di
    // sidebarWidth/sidebarDivider ma sull'asse verticale, tra editor/
    // opcodeHelpBar sopra e logConsole sotto (ancorata al bordo inferiore
    // della finestra, vedi resized()).
    int consoleHeight = 180;
    static constexpr int consoleMinHeight = 80;
    static constexpr int consoleEditorMinHeight = 200;
    static constexpr int consoleDividerHeight = 6;

    // Striscia orizzontale draggabile tra editor/opcodeHelpBar e la
    // consolle: stesso schema di SidebarDivider sopra, ma sull'asse Y (resize
    // dell'ALTEZZA della consolle invece della larghezza della sidebar).
    struct ConsoleDivider final : public juce::Component
    {
        ConsoleDivider();
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;

        // Vedi il commento identico su SidebarDivider::getCurrentWidth/onDrag
        // sopra - stessa idea, sull'altezza della consolle invece della
        // larghezza della sidebar.
        std::function<int()> getCurrentHeight;
        std::function<void (int)> onDrag;

    private:
        int heightAtDragStart = 0;
    };
    ConsoleDivider consoleDivider;

    // La toolbar e' una barra dedicata (sfondo + separatore disegnati in
    // paint(), bounds calcolati in resized()) che contiene questi bottoni,
    // con icone disegnate da CsoundLookAndFeel in base al nome
    // (Component::setName) assegnato nel costruttore.
    juce::Rectangle<int> toolbarBounds;
    juce::TextButton applyButton        { "Apply" };

    // Menu hamburger del pannello Parametri: ancorato a destra in QUESTA
    // toolbar (richiesta esplicita) - l'oggetto VERO (stile/nome/onClick)
    // resta di proprieta' di CsoundParameterMappingPanel, qui si tiene solo
    // un diametro fisso condiviso per posizionarlo (stesso principio di
    // clearConsoleButtonDiameter sotto: circolare, senza testo, non
    // proporzionato al contenuto) - vedi resized() e il costruttore, dove
    // viene riparentato con addAndMakeVisible() sul riferimento restituito
    // da parameterPanel.getMenuButton(). Il "+" NON e' piu' qui (richiesta
    // esplicita): e' tornato dentro la title bar del pannello Parametri
    // stesso (vedi panelTitleBarButtonDiameter in CsoundParameterEditor.h),
    // quindi questo diametro/gap ora riguarda SOLO il burger - vedi anche
    // applyButton sopra, che usa lo stesso diametro per restare alla sua
    // altezza.
    static constexpr int panelToolbarButtonDiameter = 38; // ingrandito ANCORA (richiesta esplicita: "ingrandiscili per favorire uso su iOS")

    // Non piu' nella toolbar (richiesta esplicita): un piccolo bottone
    // CIRCOLARE (proprieta' dinamica "circular", vedi CsoundLookAndFeel::
    // drawButtonBackground/drawButtonText), posizionato in resized() in
    // overlap sopra l'angolo in alto a destra di logConsole invece che in
    // fila con gli altri bottoni - testo vuoto apposta (icona "clear",
    // cioe' lo stesso cestino, gia' centrata senza testo dal ramo
    // "circular" di drawButtonText). static constexpr sotto: diametro fisso,
    // non proporzionato al testo (non c'e' testo) ne' all'altezza toolbar.
    juce::TextButton clearConsoleButton {};
    static constexpr int clearConsoleButtonDiameter = 28;
    static constexpr int clearConsoleButtonMargin = 6;

    // Mostra/nasconde logConsole (+ consoleDivider), analogo a
    // parameterPanel sopra - vedi toggleConsole() sotto. Visibile di
    // default (showingConsole parte a true). Niente piu' un bottone
    // dedicato nella toolbar (richiesta esplicita, come per
    // showingParameterPanel/toggleParameterPanel() sopra): l'azione e' ora
    // SOLO la voce spuntabile "Show Console" nel menu hamburger del
    // pannello Parametri (vedi parameterPanel.onToggleConsoleRequested/
    // isConsoleVisible, impostate nel costruttore).
    bool showingConsole = true;
    void toggleConsole();

    // Save/Load CSD: scrivono/leggono su disco (FileChooser, filtro *.csd -
    // un .csd VERO, il mapping dei parametri va in appendice dentro un tag
    // dedicato, vedi CsoundAudioProcessor::saveSessionToFile) l'intero
    // stato - codice Csound + mapping dei 64 parametri - INDIPENDENTEMENTE
    // dal progetto della DAW (che resta comunque salvato/ripristinato come
    // sempre da getStateInformation/setStateInformation). Senza questo,
    // rimuovere il plugin dalla traccia o perdere il progetto avrebbe
    // fatto perdere anche il codice: vedi CsoundAudioProcessor::
    // saveSessionToFile/loadSessionFromFile.
    //
    // I vecchi bottoni "Save as CSD..."/"Load CSD" nella toolbar sono stati
    // RIMOSSI (richiesta esplicita): promptSaveSession()/promptLoadSession()
    // sotto restano le uniche funzioni che sanno fare il lavoro vero, ma
    // sono ora richiamate dalle voci "Save as CSD..."/"Load CSD" del menu
    // hamburger del pannello Parametri (vedi parameterPanel.
    // onSaveSessionRequested/onLoadSessionRequested, impostate nel
    // costruttore).

    // juce::FileChooser e' asincrono (launchAsync): deve restare in vita
    // finche' il suo callback non e' scattato, quindi va tenuto come
    // membro (non una variabile locale che morirebbe subito) - un solo
    // chooser alla volta basta, Save e Load non possono essere aperti
    // contemporaneamente dalla stessa UI.
    std::unique_ptr<juce::FileChooser> activeFileChooser;

    // onSaved, se presente, scatta SOLO se il salvataggio va davvero a buon
    // fine (file scritto) - usato da confirmDiscardCurrentStateThenLoad()
    // sotto per incatenare "salva, poi procedi col Load" quando l'utente
    // sceglie "Save" nel dialogo di conferma. Il bottone "Save as CSD..." nella
    // toolbar chiama semplicemente promptSaveSession() senza argomenti (il
    // default nullptr), nessun comportamento diverso per lui.
    void promptSaveSession (std::function<void()> onSaved = nullptr);
    void promptLoadSession();

    // juce::FileDragAndDropTarget: drag and drop di un .csd dal Finder/
    // Explorer DIRETTAMENTE sull'editor, stessa destinazione finale di
    // "Load CSD..." - vedi loadSessionFile() sotto, che fattorizza la
    // logica di successo comune a entrambi i percorsi (FileChooser e
    // drag and drop) invece di duplicarla. Interfaccia DIVERSA da
    // juce::DragAndDropTarget (quella che CsoundCodeEditor implementa per
    // il drag INTERNO delle righe del pannello Parametri, vedi il
    // commento in testa a questa classe) - questa qui e' per file che
    // arrivano da FUORI l'applicazione (dal sistema operativo), nessun
    // conflitto tra le due essendo interfacce distinte.
    //
    // L'EREDITA' da FileDragAndDropTarget sopra (vedi l'elenco classi in
    // testa al file) deve restare PUBLIC, non private: ComponentPeer
    // individua chi supporta il drop di file con un dynamic_cast fatto da
    // codice DI JUCE, fuori da questa classe - con ereditarieta' privata
    // quel cast fallisce silenziosamente (ritorna nullptr) e
    // isInterestedInFileDrag/filesDropped non vengono MAI chiamati, pur
    // compilando senza errori. Stesso identico motivo per cui
    // juce::DragAndDropContainer qui sopra e' gia' public (lo richiede
    // CsoundCodeEditor::itemDropped tramite
    // DragAndDropContainer::findParentDragContainerFor, un cast
    // altrettanto "esterno").
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // Feedback visivo mentre il file e' trascinato SOPRA l'editor (prima
    // del rilascio): fileDragEnter/fileDragExit sono virtuali OPZIONALI di
    // FileDragAndDropTarget (non serve fileDragMove, non ci serve la
    // posizione) - mostrano/nascondono dropHighlightOverlay sotto.
    // filesDropped() sopra nasconde comunque l'overlay per sicurezza (la
    // documentazione JUCE non garantisce che fileDragExit scatti sempre
    // dopo un drop andato a buon fine).
    //
    // NOTA storica: la prima implementazione disegnava il bordo/overlay
    // direttamente in CsoundAudioProcessorEditor::paint() (il GENITORE) alle
    // bounds di "editor" - invisibile, perche' JUCE dipinge i figli DOPO il
    // paint() del genitore: "editor" (CsoundCodeEditor, opaco) ridisegnava
    // sempre il proprio sfondo sopra, coprendo l'overlay. Serve quindi un
    // componente FIGLIO dedicato, portato in primo piano (toFront) sopra
    // "editor", che disegna lui stesso bordo+overlay nel proprio paint() -
    // cosi' il suo ordine di disegno e' DOPO "editor", non prima.
    struct DropHighlightOverlay final : public juce::Component
    {
        DropHighlightOverlay() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds();
            g.setColour (juce::Colour (0xff3d8bfd).withAlpha (0.12f));
            g.fillRect (area);
            g.setColour (juce::Colour (0xff3d8bfd));
            g.drawRect (area, 3);
        }
    };
    DropHighlightOverlay dropHighlightOverlay;

    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;

    // Punto d'ingresso comune a promptLoadSession() (FileChooser) e
    // filesDropped() sopra (drag and drop): il codice nell'editor (e la
    // mappatura parametri nella sidebar) e' uno STATO che esiste solo qui
    // finche' non viene scritto su un .csd - il progetto della DAW lo
    // salva gia' (getStateInformation), ma caricare un nuovo file lo
    // sostituirebbe comunque, perdendolo se non e' mai stato esportato.
    // Mostra quindi SEMPRE un dialogo NATIVO del sistema operativo a 3 vie
    // (showNativeThreeButtonAlert, vedi NativeAlertMac.h/.mm - un NSAlert
    // vero con pulsanti "Save"/"Overwrite"/"Cancel" dal testo personalizzato,
    // non juce::AlertWindow che e' disegnato da JUCE ne' juce::
    // NativeMessageBox che e' nativo ma coi pulsanti fissi Yes/No/Cancel)
    // prima di procedere: "Cancel" non fa nulla, "Overwrite" chiama subito
    // performLoadSessionFile(file) sotto, "Save" apre PRIMA il browser di
    // esportazione (promptSaveSession) e chiama performLoadSessionFile(file)
    // solo se il salvataggio va a buon fine (altrimenti annulla il Load).
    void loadSessionFile (const juce::File& file);

    // Logica di successo vera e propria (ex intero corpo di
    // loadSessionFile() prima di questa modifica): carica il file nel
    // processor, rilegge editor/pannello parametri dal nuovo stato,
    // segnala il bordo rosso di Apply (vedi markApplyPendingAfterLoad()) e
    // ricorda la cartella per la prossima volta - chiamata SOLO da
    // loadSessionFile() sopra, dopo la conferma dell'utente. Richiama ANCHE
    // performApply() alla fine (richiesta esplicita: sia dopo "Overwrite"
    // sia dopo "Save" il .csd appena caricato deve essere gia' in esecuzione,
    // l'utente non deve premere Apply a mano una seconda volta).
    void performLoadSessionFile (const juce::File& file);

    // "Initialize Session" del menu hamburger (richiesta esplicita:
    // "pulisce tutto e carica il CSD hard coded") - stesso schema conferma/
    // esegui di loadSessionFile()/performLoadSessionFile() sopra, ma senza
    // un file: un dialogo nativo a due vie (showNativeTwoButtonAlert,
    // stesso usato da CsoundParameterMappingPanel::removeAllParameters())
    // chiede conferma PRIMA, visto che l'azione sostituisce TUTTO (codice +
    // mapping parametri) senza che l'utente abbia scelto un file nuovo da
    // cui aspettarselo.
    void promptInitializeSession();

    // Esegue davvero l'inizializzazione, chiamata SOLO da
    // promptInitializeSession() sopra dopo la conferma: CsoundAudioProcessor::
    // initializeSession() azzera la mappatura e sostituisce
    // audioProcessor.getCsdText() con defaultCsdText(), poi qui si rilegge
    // quello stato in document/parameterPanel e si richiama performApply(),
    // esattamente come fa performLoadSessionFile() dopo un Load CSD -
    // stessa UX, nessuna sorpresa: dopo la conferma il codice di default e'
    // gia' in esecuzione, nessun bordo rosso residuo.
    void performInitializeSession();

    // Corpo del vecchio applyButton.onClick, estratto in un metodo a se'
    // (richiesta esplicita) cosi' da poter essere richiamato ANCHE da
    // performLoadSessionFile() sopra dopo un Load CSD riuscito, non solo da
    // un click diretto sul bottone Apply.
    void performApply();

    static constexpr int toolbarHeight = 44;

    // Dimensione MINIMA della finestra (vedi setResizeLimits() nel
    // costruttore) - BUG corretto: senza limiti, rimpicciolendo la finestra
    // al massimo, l'area assegnata a "editor" in resized() collassa a
    // larghezza/altezza 0 (i clamp sidebarEditorMinWidth/
    // consoleEditorMinHeight sopra impediscono alla SIDEBAR/CONSOLLE di
    // rubare troppo spazio, ma non impediscono alla FINESTRA stessa di
    // diventare piu' piccola del minimo che TUTTI gli elementi insieme
    // richiedono) - con area.getWidth()/getHeight() a 0, dentro
    // juce::CodeEditorComponent::paint() il calcolo "right - gutterSize"
    // diventa negativo e fa scattare una jassert interna di JUCE
    // (coordsToRectangle, w/h devono essere >= 0), con conseguente crash
    // (EXC_BREAKPOINT). minWindowWidth/Height sono scelti un po' sopra la
    // somma esatta dei minimi di sidebar+editor (586) e di
    // toolbar+consolle+editor (364), per un margine di sicurezza.
    static constexpr int minWindowWidth  = 640;
    static constexpr int minWindowHeight = 420;

    CsoundLookAndFeel lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundAudioProcessorEditor)
};
