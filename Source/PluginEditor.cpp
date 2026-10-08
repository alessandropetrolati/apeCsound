#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "NativeAlertMac.h"
#include <cmath>

namespace
{
    // Fa da ponte fra l'UndoManager INTERNO di juce::CodeDocument (che
    // raggruppa le battute in transazioni per conto suo, vedi
    // CodeEditorComponent::newTransaction()/il timer di pausa in
    // digitazione) e lo sharedUndoManager di PluginEditor (richiesta
    // esplicita: un'UNICA cronologia lineare fra editor di codice e
    // pannello Parametri, vedi bridgeCodeEditIntoSharedUndo() in
    // PluginEditor.cpp). Un'istanza rappresenta UNA transazione di
    // CodeDocument (un "gruppo" di battute, non un singolo carattere).
    //
    // perform() viene richiamata la PRIMA volta nello stesso istante in cui
    // viene creata (juce::UndoManager::perform() lo fa sempre, per
    // "eseguire" l'azione appena spinta) - ma il testo e' GIA' stato
    // scritto dalla digitazione normale un istante prima, quindi quella
    // prima chiamata deve essere un no-op (firstPerform): solo un
    // SUCCESSIVO Redo (dopo un Undo) deve davvero rifare redo() sul
    // document.
    //
    // guardFlag: impostato a true per la durata della chiamata a undo()/
    // redo() sul document - vedi il commento sulla guardia di rientranza
    // in bridgeCodeEditIntoSharedUndo().
    struct CodeEditTransactionProxy final : public juce::UndoableAction
    {
        CodeEditTransactionProxy (juce::CodeDocument& documentIn, bool& guardFlagIn)
            : document (documentIn), guardFlag (guardFlagIn) {}

        bool perform() override
        {
            if (! firstPerform)
            {
                const juce::ScopedValueSetter<bool> guard (guardFlag, true);
                document.getUndoManager().redo();
            }

            firstPerform = false;
            return true;
        }

        bool undo() override
        {
            const juce::ScopedValueSetter<bool> guard (guardFlag, true);
            document.getUndoManager().undo();
            return true;
        }

        juce::CodeDocument& document;
        bool& guardFlag;
        bool firstPerform = true;

        JUCE_DECLARE_NON_COPYABLE (CodeEditTransactionProxy)
    };

    // Cartella di partenza di TUTTI i file chooser (Save As/Load/Relocate):
    // SEMPRE la cartella base della sessione, ~/Documents/apeCsound (vedi
    // CsoundAudioProcessor::getBaseFolder) - richiesta esplicita: "devono
    // puntare alla cartella Documents/apeCsound, come alla prima
    // apertura". Niente piu' "ricorda l'ultima cartella usata" (il vecchio
    // juce::PropertiesFile e' stato rimosso): salvando li' dentro il path
    // nello stato del progetto resta relativo, quindi portabile.
    juce::File getCsdChooserStartDirectory()
    {
        return CsoundAudioProcessor::getBaseFolder();
    }
}

//==============================================================================
CsoundAudioProcessorEditor::CsoundAudioProcessorEditor (CsoundAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    document.replaceAllContent (audioProcessor.getCsdText());
    document.clearUndoHistory();

    // Secondo listener indipendente sullo stesso document (vedi il
    // commento in PluginEditor.h): aggiunto DOPO il replaceAllContent qui
    // sopra apposta, altrimenti il caricamento iniziale del testo
    // (identico a audioProcessor.getCsdText() per definizione) scatenerebbe
    // una chiamata a updateApplyButtonDirtyState() inutile - a questo punto
    // comunque risulterebbe "non modificato", quindi non cambia nulla nella
    // pratica, ma e' piu' chiaro cosi'.
    document.addListener (this);

    editor.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain));
    addAndMakeVisible (editor);

    logConsole.setMultiLine (true);
    logConsole.setReadOnly (true);
    logConsole.setScrollbarsShown (true);
    logConsole.setCaretVisible (false);
    // Console scura "da terminale" di proposito, a contrasto col resto
    // dell'interfaccia chiara: accento teal sul testo per restare in tema.
    logConsole.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff10181f));
    logConsole.setColour (juce::TextEditor::textColourId, juce::Colour (0xff8fd9e0));
    logConsole.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    addAndMakeVisible (logConsole);

    // Il motore puo' girare da tempo con la UI chiusa: recuperiamo subito la
    // storia recente dei messaggi (buffer circolare in CsoundAudioProcessor)
    // cosi' la console non appare vuota alla (ri)apertura.
    for (const auto& msg : audioProcessor.getMessageHistory())
        appendToLog (msg);

    // Se il motore e' gia' in esecuzione (prepareToPlay puo' essere stato
    // chiamato prima che questo editor esistesse), alimentiamo subito
    // l'autocompletamento/help inline con l'elenco reale degli opcode;
    // altrimenti arrivera' tramite csoundEngineStarted() non appena parte.
    editor.setOpcodeSignatures (audioProcessor.getOpcodeSignatures());

    // La barra di help sotto l'editor viene riempita (o svuotata)
    // direttamente da CsoundCodeEditor, in base a dove si trova il caret.
    editor.onOpcodeHelpChanged = [this] (const juce::String& syntax, const juce::String& description)
    {
        opcodeHelpBar.setHelpText (syntax, description);
    };
    addAndMakeVisible (opcodeHelpBar);

    // Etichetta file + barra "file non trovato" (vedi SessionFileLabel/
    // SessionWarningBar in PluginEditor.h). La barra e' addChildComponent:
    // NON visibile finche' updateSessionStatus() non ha nulla da segnalare.
    // L'host puo' aver gia' ripristinato lo stato PRIMA che questo editor
    // esistesse (ordine tipico all'apertura di un progetto): per questo si
    // aggiorna anche qui alla costruzione, non solo in sessionStateRestored().
    addAndMakeVisible (sessionFileLabel);

    sessionWarningBar.relocateButton.onClick = [this] { promptRelocateSession(); };
    sessionWarningBar.saveAsButton.onClick   = [this] { promptSaveSession(); };
    addChildComponent (sessionWarningBar);
    updateSessionStatus();

    // Il nome del Component e' come CsoundLookAndFeel sceglie quale icona
    // disegnare (vedi getIconPathForButtonName) - non ha altro effetto.
    // Il motore Csound e' sempre in esecuzione (parte da solo in
    // prepareToPlay): questo bottone non "avvia" nulla, si limita a
    // rimpiazzare il .csd corrente con quello appena modificato nell'editor.
    applyButton.setName ("apply");
    applyButton.onClick = [this] { performApply(); };
    addAndMakeVisible (applyButton);

    clearConsoleButton.setName ("clear");
    clearConsoleButton.getProperties().set ("circular", true);
    clearConsoleButton.onClick = [this] { logConsole.clear(); };
    addAndMakeVisible (clearConsoleButton);

    // Sidebar + divisore: aperti di default (showingParameterPanel = true),
    // editor/consolle si restringono per farle spazio - vedi resized().
    // Niente piu' un bottone dedicato "Parameters" nella toolbar (richiesta
    // esplicita): l'azione di mostra/nasconde e' ora SOLO la voce spuntabile
    // "Show Parameters" nel menu hamburger, vedi onToggleParametersRequested/
    // isParametersPanelVisible impostate piu' sotto.
    addChildComponent (parameterPanel);
    addChildComponent (sidebarDivider);
    parameterPanel.setVisible (showingParameterPanel);
    sidebarDivider.setVisible (showingParameterPanel);

    // Menu hamburger: riparentato QUI (richiesta esplicita: vive nella
    // toolbar principale, non dentro il pannello) - vedi il commento su
    // panelToolbarButtonDiameter in PluginEditor.h. setLookAndFeel()
    // applica la LookAndFeel del pannello (la sua icona hamburger e'
    // disegnata SOLO da quella, vedi CsoundParameterMappingPanel::
    // getButtonLookAndFeel()), altrimenti erediterebbe CsoundLookAndFeel di
    // questo editor che non la conosce. Il "+" NON e' piu' qui (richiesta
    // esplicita): resta dentro la title bar del pannello Parametri stesso,
    // dove il pannello lo aggiunge/posiziona da solo - vedi il commento in
    // testa a parameterPanel.addButton in CsoundParameterEditor.h.
    parameterPanel.getMenuButton().setLookAndFeel (&parameterPanel.getButtonLookAndFeel());
    addAndMakeVisible (parameterPanel.getMenuButton());

    sidebarDivider.getCurrentWidth = [this] { return sidebarWidth; };
    sidebarDivider.onDrag = [this] (int newWidth)
    {
        sidebarWidth = newWidth;
        resized();
    };

    // Mostra/nasconde logConsole (+ consoleDivider), analogo a
    // parameterPanel sopra - vedi toggleConsole(). Visibile di default
    // (showingConsole = true). Niente piu' un bottone dedicato nella
    // toolbar (richiesta esplicita): stesso discorso di paramsButton
    // sopra, l'azione e' ora SOLO la voce spuntabile "Show Console" nel
    // menu hamburger.
    addAndMakeVisible (consoleDivider);
    consoleDivider.getCurrentHeight = [this] { return consoleHeight; };
    consoleDivider.onDrag = [this] (int newHeight)
    {
        consoleHeight = newHeight;
        resized();
    };

    // Riscontro in consolle per il tasto destro sulla maniglia (copia
    // chnget negli appunti) - vedi il commento su onParameterCopiedToClipboard
    // in CsoundParameterEditor.h: senza questo l'azione non lascia alcuna
    // traccia visibile.
    parameterPanel.onParameterCopiedToClipboard = [this] (const juce::String& message)
    {
        appendToLog (message);
    };

    // Save/Load Session su file (.csd), indipendenti dal progetto della
    // DAW - vedi il commento sul perche' in PluginEditor.h. Non piu' due
    // bottoni dedicati nella toolbar (richiesta esplicita): le stesse
    // funzioni sono ora richiamate dalle voci "Save as..."/"Load..." del
    // menu hamburger del pannello Parametri (vedi CsoundParameterMappingPanel::
    // showPanelMenu()).
    parameterPanel.onSaveSessionRequested = [this] { promptSaveSession(); };
    parameterPanel.onLoadSessionRequested = [this] { promptLoadSession(); };
    parameterPanel.onSaveLinkedRequested  = [this] { performSaveLinked(); };

    // Ogni cambio di STRUTTURA dei parametri (add/remove/metadata/undo/redo)
    // ricontrolla la barra "modifiche non salvate" - vedi onMappingChanged in
    // CsoundParameterEditor.h e SessionWarningBar in PluginEditor.h.
    parameterPanel.onMappingChanged = [this] { updateSessionStatus(); };

    // "Initialize Session" del menu hamburger - vedi promptInitializeSession()
    // in PluginEditor.h.
    parameterPanel.onInitializeSessionRequested = [this] { promptInitializeSession(); };

    // Sezione "View" del menu hamburger (Show Parameters/Show Console) -
    // vedi il commento su onToggleParametersRequested/onToggleConsoleRequested/
    // isParametersPanelVisible/isConsoleVisible in CsoundParameterEditor.h.
    parameterPanel.onToggleParametersRequested = [this] { toggleParameterPanel(); };
    parameterPanel.onToggleConsoleRequested    = [this] { toggleConsole(); };
    parameterPanel.isParametersPanelVisible    = [this] { return showingParameterPanel; };
    parameterPanel.isConsoleVisible            = [this] { return showingConsole; };

    // Apre la sidebar Parametri se e' chiusa quando si aggiunge un nuovo
    // parametro (vedi il commento su onEnsurePanelVisible in
    // CsoundParameterEditor.h) - richiesta esplicita.
    parameterPanel.onEnsurePanelVisible = [this]
    {
        if (! showingParameterPanel)
            toggleParameterPanel();
    };

    // Undo/Redo dal menu hamburger: SENZA questo, le voci "Undo"/"Redo" del
    // pannello richiamerebbero SOLO undoManager.undo()/redo() direttamente
    // (fallback in CsoundParameterMappingPanel::undo()/redo()) - che da
    // quando undoManager e' un RIFERIMENTO a sharedUndoManager (vedi
    // CsoundParameterEditor.h) sarebbe comunque corretto, ma passare da
    // performUndo()/performRedo() resta piu' chiaro ed e' lo stesso
    // instradamento usato da Cmd+Z/Cmd+Shift+Z in keyPressed().
    parameterPanel.onUndoRequested = [this] { performUndo(); };
    parameterPanel.onRedoRequested = [this] { performRedo(); };

    // Cmd+Z/Cmd+Shift+Z mentre l'editor di codice ha il focus: intercettati
    // DENTRO CsoundCodeEditor::keyPressed PRIMA che la classe base JUCE li
    // consumi da sola per il proprio undo testuale interno (vedi il
    // commento esteso su performUndo()/performRedo() in PluginEditor.h) -
    // instradati qui con le STESSE funzioni, cosi' editor di codice e
    // pannello Parametri condividono davvero un'unica cronologia.
    editor.onUndoRequested = [this] { performUndo(); };
    editor.onRedoRequested = [this] { performRedo(); };

    // clearConsoleButton galleggia in overlap sopra l'angolo in alto a
    // destra di logConsole (vedi resized()): deve restare sempre sopra di
    // essa nello z-order a prescindere dall'ordine di addAndMakeVisible
    // qui sopra, quindi portato in cima esplicitamente qui alla fine.
    clearConsoleButton.toFront (false);

    // dropHighlightOverlay: invisibile finche' non mostrato da
    // fileDragEnter(), ma va aggiunto (addChildComponent, non
    // addAndMakeVisible) e portato DAVANTI a "editor" nello z-order gia' da
    // subito - vedi il commento in testa a DropHighlightOverlay in
    // PluginEditor.h sul perche' serve un componente figlio invece di
    // disegnare nel paint() del genitore.
    addChildComponent (dropHighlightOverlay);
    dropHighlightOverlay.toFront (false);

    audioProcessor.addListener (this);

    // false = niente ResizableCornerComponent in basso a destra: il
    // ridimensionamento resta comunque possibile, ma tramite il bordo
    // nativo della finestra (host/wrapper Standalone), non una maniglia
    // disegnata da noi.
    setResizable (true, false);

    // Limite MINIMO di ridimensionamento (vedi il commento su
    // minWindowWidth/minWindowHeight in PluginEditor.h) - BUG corretto:
    // senza questo, rimpicciolendo la finestra al massimo l'area
    // dell'editor di codice collassava a 0 e juce::CodeEditorComponent::
    // paint() crashava (jassert interna su una larghezza/altezza negativa).
    // Nessun limite massimo sensato da imporre (la finestra puo' diventare
    // grande quanto si vuole), quindi un valore arbitrariamente alto.
    setResizeLimits (minWindowWidth, minWindowHeight, 8192, 8192);

    //setSize (960, 680);
    setSize (1024, 768);

    // Tentiamo di dare il focus da tastiera all'editor di codice non appena
    // la finestra del plugin e' pronta, cosi' digitare funziona subito
    // senza bisogno di un primo click. Farlo qui nel costruttore in modo
    // SINCRONO (grabKeyboardFocus() diretto) ha causato un crash in alcune
    // DAW: a questo punto del costruttore il componente non ha ancora
    // necessariamente un peer nativo valido. Rimandarlo con
    // MessageManager::callAsync lo esegue al giro successivo del message
    // loop, quando la finestra esiste di sicuro; il SafePointer evita un
    // crash se nel frattempo l'editor fosse gia' stato distrutto (es. la
    // DAW chiude la finestra del plugin prima che il callback scatti).
    juce::Component::SafePointer<CsoundCodeEditor> safeEditor (&editor);
    juce::MessageManager::callAsync ([safeEditor]
    {
        if (safeEditor != nullptr)
            safeEditor->grabKeyboardFocus();
    });
}

CsoundAudioProcessorEditor::~CsoundAudioProcessorEditor()
{
    document.removeListener (this);

    // Scollega la LookAndFeel del pannello Parametri impostata nel
    // costruttore sul burger (vedi il commento li') PRIMA che
    // parameterPanel (quindi la sua LookAndFeel) venga distrutta insieme
    // al resto dei membri di questo editor - per non lasciare, anche solo
    // per un istante durante lo smontaggio, un puntatore a LookAndFeel
    // pendente. Il "+" non serve piu' qui: e' un child DIRETTO del
    // pannello, che gli applica la sua LookAndFeel per semplice eredita'
    // nell'albero dei componenti (nessun setLookAndFeel esplicito da
    // scollegare).
    parameterPanel.getMenuButton().setLookAndFeel (nullptr);

    setLookAndFeel (nullptr);
    audioProcessor.removeListener (this);
}

void CsoundAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // Toolbar scurita (richiesta esplicita) - stesso petrolio/teal scuro
    // usato da logConsole (vedi 0xff10181f nel costruttore) invece del
    // bianco di prima, cosi' la fascia superiore e' coerente con il resto
    // dei toni scuri dell'app (consolle, pannello Parametri). I bottoni
    // restano leggibili: sono tutti sfondi pieni accentati (teal/colorato)
    // con testo bianco, non dipendono dal contrasto con lo sfondo della
    // toolbar dietro di loro.
    g.setColour (juce::Colour (0xff10181f));
    g.fillRect (toolbarBounds);

    g.setColour (juce::Colour (0xff2a3a44));
    g.drawLine ((float) toolbarBounds.getX(),     (float) toolbarBounds.getBottom() - 0.5f,
                (float) toolbarBounds.getRight(), (float) toolbarBounds.getBottom() - 0.5f, 1.0f);

    // L'evidenziazione del drag and drop CSD non si disegna piu' qui: vedi
    // dropHighlightOverlay (componente figlio dedicato, vedi il commento in
    // PluginEditor.h sul perche') mostrato/nascosto da fileDragEnter/
    // fileDragExit e posizionato in resized().
}

bool CsoundAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // Vedi il commento su questa dichiarazione in PluginEditor.h - risale
    // qui SOLO se nessun componente focalizzato piu' in basso nell'albero
    // (es. un campo nome o l'editor di codice) ha gia' consumato Cmd+Z per
    // il proprio undo testuale interno.
    //
    // 'Z' MAIUSCOLA, non 'z' (BUG corretto, stesso errore nell'identico
    // controllo in CsoundCodeEditor::keyPressed): per convenzione JUCE,
    // KeyPress::keyCode per i tasti-lettera e' SEMPRE il codice ASCII
    // maiuscolo, indipendentemente da Shift (che e' nei modifiers) - con la
    // minuscola questo confronto non corrispondeva MAI a un vero evento
    // Cmd+Z/Cmd+Shift+Z.
    if (key == juce::KeyPress ('Z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        performRedo();
        return true;
    }

    if (key == juce::KeyPress ('Z', juce::ModifierKeys::commandModifier, 0))
    {
        performUndo();
        return true;
    }

    // NIENTE Cmd+S qui (rimosso, richiesta esplicita): collide con il Save
    // del progetto della DAW. Il salvataggio del .csd e' SOLO la voce "Save"
    // del menu hamburger.

    // Esc toglie il focus da tastiera da QUALUNQUE componente lo abbia
    // (campo nome di un parametro, editor di codice...) - richiesto
    // esplicitamente. Stessa logica di bubbling di Cmd+Z sopra: se un campo
    // focalizzato ha gia' un suo significato per Esc (es. CsoundCodeEditor
    // chiude prima il popup di autocompletamento/la barra di help, vedi
    // CsoundCodeEditor::keyPressed) lo consuma PRIMA che arrivi qui, quindi
    // la prima pressione di Esc chiude quello, una seconda (nessun
    // popup/help piu' aperto) arriva fino a qui e sposta via il focus.
    if (key == juce::KeyPress::escapeKey)
    {
        juce::Component::unfocusAllComponents();
        return true;
    }

    return false;
}

// Rifatto completamente (richiesta esplicita: "rivedi completamente le
// funzioni Undo/Redo... una linearita' avanti e indietro tra l'editor e le
// configurazioni dei Parametri") - niente piu' euristica basata sul focus
// (lastMeaningfulFocusWasEditor/globalFocusChanged, rimossi): editor di
// codice e pannello Parametri condividono ora UN'UNICA cronologia
// (sharedUndoManager, vedi PluginEditor.h), quindi non c'e' piu' nulla da
// "indovinare" su quale dei due storicamente abbia il focus - un solo
// Cmd+Z/voce di menu annulla sempre il passo CRONOLOGICAMENTE piu' recente,
// a prescindere che sia stata una modifica di testo o di un parametro.
// L'editor di codice vi partecipa tramite CodeEditTransactionProxy (vedi
// bridgeCodeEditIntoSharedUndo() sotto): CsoundCodeEditor::keyPressed
// intercetta Cmd+Z/Cmd+Shift+Z PRIMA della classe base JUCE (che altrimenti
// li consumerebbe da sola per il proprio undo testuale interno,
// document.getUndoManager(), rendendoli per sempre invisibili a questa
// funzione) e li instrada qui tramite onUndoRequested/onRedoRequested
// (impostate nel costruttore sotto).
void CsoundAudioProcessorEditor::performUndo()
{
    sharedUndoManager.undo();
}

void CsoundAudioProcessorEditor::performRedo()
{
    sharedUndoManager.redo();
}

void CsoundAudioProcessorEditor::bridgeCodeEditIntoSharedUndo()
{
    // Guardia di rientranza: CodeEditTransactionProxy::perform()/undo()
    // (sotto) richiamano document.getUndoManager().redo()/undo(), che a
    // loro volta fanno scattare DI NUOVO questo stesso listener (vedi
    // CodeDocument::insert()/remove(): anche la riproduzione di un
    // Undo/Redo passa dagli stessi codeDocumentTextInserted/Deleted) - senza
    // questa guardia, un singolo Undo sullo sharedUndoManager finirebbe per
    // spingerci sopra UN'ALTRA copia della stessa transazione appena
    // annullata.
    if (isApplyingCodeUndoRedo)
        return;

    // getNumActionsInCurrentTransaction() == 1: questa e' la PRIMA
    // modifica di una transazione FRESCA di CodeDocument (juce::
    // CodeEditorComponent ne chiude una da sola dopo una pausa nella
    // digitazione, o esplicitamente per azioni come paste/undo/ecc. - vedi
    // CodeEditorComponent::newTransaction()) - ne rispecchiamo UNA SOLA
    // istanza proxy nello sharedUndoManager: un intero "gruppo" di
    // battute diventa cosi' UN solo passo di Undo condiviso, non uno per
    // carattere. Le modifiche successive nella STESSA transazione (count
    // > 1) non aggiungono altro: sono gia' comprese nel proxy appena
    // creato, perche' document.getUndoManager().undo() disfa l'INTERA
    // transazione in un colpo.
    //
    // beginNewTransaction() QUI e' essenziale (BUG corretto: mancava) -
    // senza, juce::UndoManager::perform() accoda il nuovo proxy alla
    // transazione CORRENTE invece di aprirne una nuova, quindi raffiche di
    // digitazione separate (e, se intercalate, anche azioni sui metadata
    // del pannello Parametri) finivano per essere fuse in un unico passo di
    // Undo - alcune diventavano cosi' annullabili solo "in blocco" insieme
    // ad altre, altre risultavano irraggiungibili singolarmente. Ogni
    // raffica di digitazione e' invece una transazione SUA, esattamente
    // come ogni modifica ai metadata (vedi undoManager.beginNewTransaction()
    // in CsoundParameterMappingPanel) - questo e' ciò che rende possibile
    // la "linearita' avanti e indietro" richiesta tra editor e pannello.
    if (document.getUndoManager().getNumActionsInCurrentTransaction() == 1)
    {
        sharedUndoManager.beginNewTransaction();
        sharedUndoManager.perform (new CodeEditTransactionProxy (document, isApplyingCodeUndoRedo));
    }
}

void CsoundAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    toolbarBounds = area.removeFromTop (toolbarHeight);

    auto toolbar = toolbarBounds.reduced (12, 7);

    // Dimensioni proporzionate al contenuto (testo + icona), non larghezze
    // fisse arbitrarie: TextButton::getBestWidthForHeight misura il testo
    // con il font reale della LookAndFeel, CsoundLookAndFeel::getIconAllowance
    // aggiunge lo spazio occupato dall'icona.
    const auto applyWidth = applyButton.getBestWidthForHeight (toolbar.getHeight())
                           + CsoundLookAndFeel::getIconAllowance (applyButton.getName());

    // Burger del pannello Parametri: UNICO bottone ancorato a destra nella
    // toolbar principale (richiesta esplicita: paramsButton/consoleButton
    // sono stati rimossi, le loro azioni sono voci del menu hamburger). Il
    // "+" NON vive piu' qui (richiesta esplicita, vedi il commento in testa
    // a parameterPanel.addButton in CsoundParameterEditor.h): e' tornato
    // dentro la title bar del pannello Parametri stesso, a sinistra, dove
    // ora apre lo stesso menu "Parameters" prima annidato nel burger.
    auto& panelMenuButton = parameterPanel.getMenuButton();

    panelMenuButton.setBounds (toolbar.removeFromRight (panelToolbarButtonDiameter)
                                       .withSizeKeepingCentre (panelToolbarButtonDiameter, panelToolbarButtonDiameter));

    // withSizeKeepingCentre (...): stessa identica altezza fissa di +/burger
    // sopra (panelToolbarButtonDiameter), invece dell'altezza "piatta" della
    // sola area toolbar ridotta - richiesta esplicita ("il tasto Apply deve
    // essere alto come + e burger").
    applyButton.setBounds (toolbar.removeFromLeft (applyWidth)
                                   .withSizeKeepingCentre (applyWidth, panelToolbarButtonDiameter));

    // Nome del file collegato (+ "•" se modificato) nello spazio che resta
    // tra Apply e il burger - vedi SessionFileLabel in PluginEditor.h.
    // Stessa altezza di Apply/burger (panelToolbarButtonDiameter), cosi' le
    // due righe (nome + dettagli) hanno spazio e le linee di separazione
    // sono alte quanto i bottoni accanto.
    sessionFileLabel.setBounds (toolbar.reduced (12, 0)
                                       .withSizeKeepingCentre (juce::jmax (0, toolbar.getWidth() - 24), panelToolbarButtonDiameter));

    // Barra "file non trovato" (vedi SessionWarningBar in PluginEditor.h):
    // a tutta larghezza subito sotto la toolbar, SOLO quando visibile -
    // altrimenti non sottrae spazio a editor/sidebar/consolle.
    if (sessionWarningBar.isVisible())
        sessionWarningBar.setBounds (area.removeFromTop (sessionWarningBarHeight));

    // Niente inset laterali: solo lo spazio verticale tra toolbar ed editor
    // resta. Sidebar ancorata A DESTRA, su tutta l'altezza rimanente (editor
    // + opcodeHelpBar + consolle si restringono insieme, non solo l'editor)
    // quando visibile - vedi il commento in testa a parameterPanel in
    // PluginEditor.h sul perche' (richiesta esplicita: editor sempre
    // accessibile/editabile sia per editing che per il drag della maniglia,
    // mai coperto da un overlay).
    area.removeFromTop (8);

    if (showingParameterPanel)
    {
        // Clamp della larghezza: mai sotto sidebarMinWidth, mai cosi' larga
        // da lasciare all'editor meno di sidebarEditorMinWidth - ricalcolato
        // ad OGNI resized() (sia per un ridimensionamento della finestra sia
        // per un trascinamento di sidebarDivider), cosi' i due casi usano la
        // stessa unica logica invece di duplicarla.
        const int maxSidebarWidth = juce::jmax (sidebarMinWidth,
                                                 area.getWidth() - sidebarEditorMinWidth - sidebarDividerWidth);
        sidebarWidth = juce::jlimit (sidebarMinWidth, maxSidebarWidth, sidebarWidth);

        auto sidebarArea = area.removeFromRight (sidebarWidth);
        auto dividerArea = area.removeFromRight (sidebarDividerWidth);

        parameterPanel.setBounds (sidebarArea);
        sidebarDivider.setBounds (dividerArea);
    }

    // Consolle nascosta (showingConsole = false, vedi toggleConsole()):
    // editor/opcodeHelpBar recuperano TUTTO lo spazio altrimenti occupato da
    // consolle + divisore, niente di quello spazio resta riservato/vuoto.
    if (showingConsole)
    {
        // Altezza della consolle ridimensionabile (richiesto esplicitamente),
        // stesso schema di clamp della sidebar sopra: mai sotto
        // consoleMinHeight, mai cosi' alta da lasciare a editor/opcodeHelpBar
        // meno di consoleEditorMinHeight.
        const int maxConsoleHeight = juce::jmax (consoleMinHeight,
                                                  area.getHeight() - consoleEditorMinHeight - consoleDividerHeight - opcodeHelpBarHeight);
        consoleHeight = juce::jlimit (consoleMinHeight, maxConsoleHeight, consoleHeight);

        auto bottomArea = area.removeFromBottom (consoleHeight);
        auto consoleDividerArea = area.removeFromBottom (consoleDividerHeight);

        logConsole.setBounds (bottomArea);
        consoleDivider.setBounds (consoleDividerArea);

        // clearConsoleButton galleggia in overlap sopra l'angolo in alto a
        // destra di logConsole (richiesto esplicitamente, al posto del
        // posto fisso che aveva nella toolbar): centrato esattamente sul
        // bordo superiore della consolle (meta' dentro, meta' fuori), come
        // un badge - toFront() nel costruttore lo tiene sempre sopra al
        // testo della consolle sotto.
        const auto consoleTopRight = bottomArea.getTopRight();
        clearConsoleButton.setBounds (consoleTopRight.x - clearConsoleButtonMargin*2 - clearConsoleButtonDiameter,
                                      consoleTopRight.y + clearConsoleButtonMargin*2,
                                      clearConsoleButtonDiameter, clearConsoleButtonDiameter);
        clearConsoleButton.setVisible (true);
    }
    else
    {
        clearConsoleButton.setVisible (false);
    }

    auto helpBarArea = area.removeFromBottom (opcodeHelpBarHeight);

    editor.setBounds (area);
    opcodeHelpBar.setBounds (helpBarArea);

    // Stesse bounds di "editor", sempre ricalcolate qui cosi' da restare
    // coerenti anche quando sidebar/consolle si ridimensionano - visibile
    // solo mentre fileDragEnter() l'ha attivato (vedi .h).
    dropHighlightOverlay.setBounds (editor.getBounds());
}

void CsoundAudioProcessorEditor::toggleParameterPanel()
{
    showingParameterPanel = ! showingParameterPanel;

    // Sidebar ancorata, non overlay: editor/consolle restano sempre
    // interagibili, si limitano a restringersi per farle spazio (vedi
    // resized() sopra) invece di essere coperti da essa.
    parameterPanel.setVisible (showingParameterPanel);
    sidebarDivider.setVisible (showingParameterPanel);

    resized();
    repaint();
}

void CsoundAudioProcessorEditor::toggleConsole()
{
    showingConsole = ! showingConsole;

    // Stesso schema di toggleParameterPanel() sopra: editor/opcodeHelpBar si
    // allargano per recuperare lo spazio (vedi resized()), nessun overlay.
    logConsole.setVisible (showingConsole);
    consoleDivider.setVisible (showingConsole);

    resized();
    repaint();
}

void CsoundAudioProcessorEditor::updateApplyButtonDirtyState()
{
    // "Modificato" = il testo ATTUALE dell'editor non coincide piu' con
    // l'ultimo testo applicato (Apply) o caricato (setStateInformation/
    // loadSessionFromFile) nel processor - cioe' quello che sta davvero
    // suonando in questo momento. Un semplice confronto di stringhe: il
    // document puo' essere anche lungo, ma questo scatta solo ad ogni
    // tasto premuto nell'editor, non nel ciclo audio - costo irrilevante.
    const bool dirty = document.getAllContent() != audioProcessor.getCsdText();

    // Component::getProperties() e' un juce::NamedValueSet dinamico,
    // qualunque Component lo ha gia' di serie: CsoundLookAndFeel::
    // drawButtonBackground lo legge per decidere se disegnare un bordo
    // rosso tutto intorno al bottone (vedi li') - nessuna nuova API,
    // nessun nuovo stato da tenere sincronizzato altrove.
    applyButton.getProperties().set ("pendingChanges", dirty);
    applyButton.repaint();
}

void CsoundAudioProcessorEditor::markApplyPendingAfterLoad()
{
    // A differenza di updateApplyButtonDirtyState() sopra, qui non si
    // confronta nulla: il bordo rosso viene forzato ACCESO
    // incondizionatamente, perche' un Load CSD e' sempre un cambio di
    // codice non ancora confermato con Apply, anche quando il testo
    // dell'editor coincide gia' (appena sincronizzato) con quello del
    // processor.
    applyButton.getProperties().set ("pendingChanges", true);
    applyButton.repaint();
}

void CsoundAudioProcessorEditor::promptSaveSession (std::function<void()> onSaved)
{
    // Un .csd VERO, non un formato proprietario: vedi il commento su
    // CsoundAudioProcessor::saveSessionToFile in PluginProcessor.h - il
    // codice resta testo Csound puro, il mapping dei parametri va in
    // appendice dentro <CsoundParams>. Cartella di partenza: SEMPRE
    // ~/Documents/apeCsound (vedi getCsdChooserStartDirectory); come nome
    // si propone quello del file collegato, se c'e', altrimenti Untitled.csd.
    const auto linked = audioProcessor.getLinkedCsdFile();
    const auto startingFile = getCsdChooserStartDirectory().getChildFile (
        linked != juce::File{} ? linked.getFileName() : juce::String ("Untitled.csd"));

    activeFileChooser = std::make_unique<juce::FileChooser> (
        "Save as...", startingFile, "*.csd");

    activeFileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, onSaved] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();

            if (file == juce::File{})
                return; // annullato dall'utente - onSaved NON scatta

            if (! file.hasFileExtension ("csd"))
                file = file.withFileExtension ("csd");

            const bool ok = audioProcessor.saveSessionToFile (file, document.getAllContent());

            if (ok)
            {

                // saveSessionToFile ha collegato la sessione al file e
                // azzerato l'eventuale avviso (file mancante/diverso): la
                // barra, se era visibile, sparisce.
                updateSessionStatus();
            }

            appendToLog (ok ? ("--- Session saved to " + file.getFullPathName()
                               + " (linked as " + audioProcessor.getLinkedCsdDisplayPath() + ") ---")
                             : "--- Failed to save session (file not writable?) ---");

            // Scatta SOLO se il file e' stato scritto davvero - usato da
            // loadSessionFile() per incatenare "salva, poi procedi col
            // Load" quando l'utente sceglie "Save" nel dialogo di conferma
            // (vedi il commento su promptSaveSession() in PluginEditor.h).
            if (ok && onSaved)
                onSaved();
        });
}

void CsoundAudioProcessorEditor::promptLoadSession()
{
    const auto startingDir = getCsdChooserStartDirectory();

    activeFileChooser = std::make_unique<juce::FileChooser> (
        "Load...", startingDir, "*.csd");

    activeFileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();

            if (file != juce::File{}) // non annullato dall'utente
                loadSessionFile (file);
        });
}

bool CsoundAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    // Un solo file, con estensione .csd - niente drag multiplo (quale dei
    // tanti andrebbe caricato?) o di altri tipi di file.
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("csd");
}

void CsoundAudioProcessorEditor::fileDragEnter (const juce::StringArray& files, int, int)
{
    if (isInterestedInFileDrag (files))
        dropHighlightOverlay.setVisible (true);
}

void CsoundAudioProcessorEditor::fileDragExit (const juce::StringArray&)
{
    dropHighlightOverlay.setVisible (false);
}

void CsoundAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    // fileDragExit non e' garantito dopo un drop riuscito (vedi il
    // commento in PluginEditor.h) - spegniamo qui comunque, altrimenti il
    // bordo resterebbe acceso indefinitamente dopo il caricamento.
    dropHighlightOverlay.setVisible (false);

    if (files.size() == 1)
        loadSessionFile (juce::File (files[0]));
}

void CsoundAudioProcessorEditor::loadSessionFile (const juce::File& file)
{
    // Sessione collegata a un file e SENZA modifiche dall'ultimo Save/Load
    // (stesso controllo dell'indicatore "•" nella toolbar, vedi
    // isSessionDirty): non c'e' nulla da perdere, si carica subito - BUG
    // corretto: prima si chiedeva SEMPRE, anche con il file appena salvato.
    if (! isSessionDirty())
    {
        performLoadSessionFile (file);
        return;
    }

    // Altrimenti il codice nell'editor (+ la mappatura parametri) e' uno
    // STATO che esiste solo qui: il progetto della DAW lo salva gia'
    // (getStateInformation), ma caricare un nuovo file lo sostituirebbe
    // comunque. Chiediamo cosa fare.
    //
    // showNativeThreeButtonAlert (vedi NativeAlertMac.h/.mm): dialogo NSAlert
    // DAVVERO nativo del sistema operativo, richiesto esplicitamente al
    // posto sia di juce::AlertWindow (testo libero ma disegnato da JUCE, non
    // nativo) sia di juce::NativeMessageBox (nativo ma pulsanti fissi
    // Yes/No/Cancel, non rietichettabili - era il compromesso adottato
    // prima di questo file). NSAlert invece accetta testo libero per
    // ciascuno dei tre pulsanti ("Save", "Overwrite", "Cancel"), restando
    // comunque il dialogo nativo del sistema. Sincrono (blocca il message
    // thread finche' l'utente non sceglie - va bene, siamo gia' su
    // quel thread, mai sul thread audio): 1 = Save, 2 = Overwrite,
    // 0 = Cancel o finestra chiusa.
    const auto linked = audioProcessor.getLinkedCsdFile();
    const juce::String message = linked != juce::File{}
        ? "The current session has unsaved changes to " + linked.getFileName()
        : "The current inline code is not saved to a .csd file "
          "and will be lost if you continue.";

    // Bottoni (riformulati, richiesta esplicita - "Save/Overwrite/Cancel"
    // era fuorviante: "Overwrite" sembrava riferito al FILE): "Save" ha la
    // doppia funzione del comando Save del menu (performSaveLinked: scrive
    // sul file collegato se c'e', altrimenti apre Save As...) e POI carica;
    // "Don't Save" scarta le modifiche e carica; "Cancel" non fa nulla.
    // 1 = Save, 2 = Don't Save, 0 = Cancel o finestra chiusa.
    const int result = showNativeThreeButtonAlert ("Unsaved changes", message,
                                                   "Save", "Don't Save", "Cancel");

    if (result == 1)
        performSaveLinked ([this, file] { performLoadSessionFile (file); });
    else if (result == 2)
        performLoadSessionFile (file);
}

void CsoundAudioProcessorEditor::performLoadSessionFile (const juce::File& file)
{
    // Nessuna copia/importazione nella cartella base (scelta esplicita): un
    // .csd fuori da ~/Documents/apeCsound resta dov'e', collegato con
    // path ASSOLUTO. Se poi il file non si trova piu' (altra macchina, file
    // spostato), il progetto suona comunque la copia incorporata e
    // l'editor mostra la barra "file non trovato" con Save As... - vedi
    // SessionWarningBar in PluginEditor.h.
    if (audioProcessor.loadSessionFromFile (file))
    {

        // L'editor di codice e il pannello parametri hanno il proprio
        // stato locale (document/righe), costruito a partire dal
        // processor - vanno rilette esplicitamente ora che
        // loadSessionFromFile ha sostituito quello stato, altrimenti
        // continuerebbero a mostrare la sessione precedente finche' non
        // si cambia tab/si riapre il pannello. Il reset incondizionato
        // dei 4 tipi di slot (vedi CsoundAudioProcessor::
        // resetAllParameterSlots, chiamato da loadSessionFromFile PRIMA
        // di leggere il nuovo file) fa si' che un .csd senza nessun
        // parametro svuoti davvero il pannello, invece di lasciare
        // appesa la mappatura della sessione precedente.
        // NIENTE clearUndoHistory() qui (a differenza del replaceAllContent
        // nel costruttore, che e' il primo caricamento e non ha nulla da
        // annullare): un Load CSD deve restare ANNULLABILE con Undo,
        // tornando al codice precedente - richiesto esplicitamente. Il
        // newTransaction() prima del replaceAllContent chiude qualunque
        // gruppo di modifiche precedente (la digitazione dell'utente) cosi'
        // il Load diventa un singolo passo di undo a se stante: un solo
        // Cmd+Z annulla l'intero caricamento, non solo meta' del nuovo
        // testo. NOTA: questo annulla solo il TESTO nell'editor - la
        // mappatura parametri nella sidebar (parameterPanel) non ha una
        // propria cronologia di undo e resta quella del CSD appena
        // caricato anche dopo un Undo del codice.
        document.newTransaction();
        document.replaceAllContent (audioProcessor.getCsdText());
        parameterPanel.refreshAllFromProcessor();

        // Il replaceAllContent qui sopra ha gia' fatto scattare
        // updateApplyButtonDirtyState() (listener del document), che con
        // editor e processor appena sincronizzati risulta "non
        // modificato" - sovrascriviamo subito con il bordo rosso forzato:
        // vedi il commento su markApplyPendingAfterLoad() in
        // PluginEditor.h sul perche'.
        markApplyPendingAfterLoad();

        // loadSessionFromFile ha collegato la sessione al file (Save
        // sovrascrivera' questo) e azzerato un eventuale avviso precedente.
        updateSessionStatus();

        appendToLog ("--- Session loaded from " + file.getFullPathName()
                     + " (linked as " + audioProcessor.getLinkedCsdDisplayPath() + ") ---");

        // Richiesta esplicita: dopo un Load CSD confermato (Overwrite, o
        // Save poi Overwrite) il codice appena caricato deve gia' essere in
        // esecuzione, come se l'utente avesse premuto Apply subito dopo -
        // performApply() sovrascrive subito dopo il bordo rosso forzato da
        // markApplyPendingAfterLoad() qui sopra con "non modificato" (visto
        // che compileAndStart compila esattamente questo stesso testo),
        // esattamente l'effetto voluto: nessun bordo rosso residuo, il .csd
        // caricato e' gia' quello in esecuzione.
        performApply();
    }
    else
    {
        appendToLog ("--- Failed to load session (empty or unreadable file?) ---");
    }
}

void CsoundAudioProcessorEditor::promptInitializeSession()
{
    // Dialogo nativo a due vie (showNativeTwoButtonAlert, vedi
    // NativeAlertMac.h/.mm - stesso usato da CsoundParameterMappingPanel::
    // removeAllParameters()): "Cancel" primo/default (risponde a Invio,
    // un'azione cosi' distruttiva non deve mai scattare per errore),
    // "Initialize" secondo. A differenza di loadSessionFile() non c'e' un
    // terzo bottone "Save": l'utente non ha scelto un file nuovo da cui
    // aspettarsi una sostituzione, quindi se vuole conservare lo stato
    // attuale deve esportarlo PRIMA a mano (Save as CSD...) - offrire
    // "Save" anche qui aggiungerebbe un passaggio che non corrisponde a
    // nessuna richiesta esplicita.
    const int choice = showNativeTwoButtonAlert (
        "Initialize session?",
        "This will remove all parameters and replace the current code with the default template. "
        "The code change can be undone afterwards; the parameter mapping cannot.",
        "Cancel", "Initialize");

    if (choice == 2)
        performInitializeSession();
}

void CsoundAudioProcessorEditor::performInitializeSession()
{
    // Stesso ordine/commenti di performLoadSessionFile() sopra: il
    // processor e' la fonte di verita' (initializeSession() azzera i 4
    // tipi di slot e sostituisce getCsdText() con defaultCsdText()),
    // document/parameterPanel vanno rilette esplicitamente DOPO.
    audioProcessor.initializeSession();

    document.newTransaction();
    document.replaceAllContent (audioProcessor.getCsdText());
    parameterPanel.refreshAllFromProcessor();

    markApplyPendingAfterLoad();

    appendToLog ("--- Session initialized (default template) ---");

    // initializeSession() ha scollegato la sessione da qualunque file: un
    // eventuale avviso "file mancante/diverso" non ha piu' senso.
    updateSessionStatus();

    // Stessa richiesta esplicita di performLoadSessionFile(): dopo la
    // conferma, il codice di default deve essere gia' in esecuzione.
    performApply();
}

//==============================================================================
// Sessione collegata a un file - modello semplificato "il file e' la verita'",
// vedi SessionFileLabel/SessionWarningBar in PluginEditor.h.
void CsoundAudioProcessorEditor::SessionFileLabel::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();

    // Due sottili linee verticali di separazione (minimal, richiesta
    // esplicita - niente cornice piena): stesso grigio-petrolio della riga
    // sotto la toolbar, cosi' "appartengono" alla barra invece di
    // sembrare un bottone.
    g.setColour (juce::Colour (0xff2a3a44));
    g.drawLine (area.getX() + 0.5f, area.getY() + 4.0f, area.getX() + 0.5f, area.getBottom() - 4.0f, 1.0f);
    g.drawLine (area.getRight() - 0.5f, area.getY() + 4.0f, area.getRight() - 0.5f, area.getBottom() - 4.0f, 1.0f);

    auto inner = area.reduced (14.0f, 2.0f);
    auto nameRow   = inner.removeFromTop (inner.getHeight() * 0.58f);
    auto detailRow = inner;

    // Nome: in evidenza (grassetto), rosso se il file manca, bianco se ci
    // sono modifiche da salvare, attenuato se tutto e' salvato. "•" dopo il
    // nome quando dirty (U+2022 via charToString: un letterale UTF-8 farebbe
    // scattare il jassert di juce::String(const char*)).
    const auto nameColour = missing ? juce::Colour (0xffff6b6b)
                          : dirty   ? juce::Colour (0xffe8eef1)
                                    : juce::Colour (0xffa9b7bf);

    g.setColour (nameColour);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawFittedText (dirty ? fileName + " " + juce::String::charToString (0x2022) : fileName,
                      nameRow.toNearestInt(), juce::Justification::centred, 1);

    // Dettagli: piccoli e attenuati, una riga sola.
    g.setColour (juce::Colour (0xff6f8089));
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawFittedText (detailText, detailRow.toNearestInt(), juce::Justification::centred, 1);
}

CsoundAudioProcessorEditor::SessionWarningBar::SessionWarningBar()
{
    messageLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    messageLabel.setColour (juce::Label::textColourId, juce::Colour (0xff3a2a00));
    messageLabel.setJustificationType (juce::Justification::centredLeft);
    messageLabel.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (messageLabel);
    addAndMakeVisible (relocateButton);
    addAndMakeVisible (saveAsButton);
}

void CsoundAudioProcessorEditor::SessionWarningBar::paint (juce::Graphics& g)
{
    // Giallo "avviso" tenue, con una riga di separazione sotto.
    g.fillAll (juce::Colour (0xfffff3c4));
    g.setColour (juce::Colour (0xffe0c56a));
    g.drawLine (0.0f, (float) getHeight() - 0.5f, (float) getWidth(), (float) getHeight() - 0.5f, 1.0f);
}

void CsoundAudioProcessorEditor::SessionWarningBar::resized()
{
    auto area = getLocalBounds().reduced (10, 5);
    saveAsButton.setBounds (area.removeFromRight (saveAsButton.getBestWidthForHeight (area.getHeight()) + 12));
    area.removeFromRight (6);
    relocateButton.setBounds (area.removeFromRight (relocateButton.getBestWidthForHeight (area.getHeight()) + 12));
    area.removeFromRight (10);
    messageLabel.setBounds (area);
}

void CsoundAudioProcessorEditor::promptRelocateSession()
{
    const auto startingDir = getCsdChooserStartDirectory();

    activeFileChooser = std::make_unique<juce::FileChooser> (
        "Relocate CSD...", startingDir, "*.csd");

    activeFileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (file != juce::File{})
                performLoadSessionFile (file); // carica + collega + applica; azzera "file mancante"
        });
}

bool CsoundAudioProcessorEditor::isSessionDirty() const
{
    // "Modificato" = la sessione com'e' ADESSO nell'editor (testo anche non
    // applicato + struttura dei parametri) differisce dall'ultimo Save/Load.
    // Senza file collegato e' sempre "da salvare".
    if (! audioProcessor.isSessionLinked())
        return true;

    // File collegato NON trovato (barra "file not found" visibile): il
    // contenuto in uso e' la copia incorporata nel progetto e NESSUN file su
    // disco lo contiene - e' da salvare per definizione, anche se identico
    // alla baseline. Senza questo, Load CSD lo considerava "pulito" e lo
    // sostituiva senza chiedere (BUG corretto).
    if (audioProcessor.isLinkedFileMissing())
        return true;

    const auto currentHash = CsoundAudioProcessor::computeTextHash (
        audioProcessor.buildSessionTextFor (document.getAllContent()));
    return currentHash != audioProcessor.getSessionBaselineHash();
}

void CsoundAudioProcessorEditor::updateSessionStatus()
{
    const auto linked  = audioProcessor.getLinkedCsdFile();
    const bool isLinked = linked != juce::File{};
    const bool missing  = audioProcessor.isLinkedFileMissing();
    const bool dirty    = isSessionDirty();

    // Riga sotto il nome: dove sta il file (relativo alla cartella base se
    // possibile, con la cartella base indicata con "~/Documents/apeCsound")
    // e lo stato in parole - cosi' il "•" ha sempre una spiegazione accanto.
    juce::String detail;

    if (! isLinked)
    {
        detail = "Not saved to a file yet - Save As... to create one";
    }
    else
    {
        const auto base = CsoundAudioProcessor::getBaseFolder();
        const auto folder = linked.getParentDirectory();
        juce::String where = linked.isAChildOf (base)
            ? "apeCsound/" + folder.getRelativePathFrom (base).replaceCharacter ('\\', '/')
            : folder.getFullPathName();

        if (where.endsWith ("/."))
            where = where.dropLastCharacters (2);

        const juce::String state = missing ? "file not found"
                                 : dirty   ? "unsaved changes"
                                           : "saved";

        detail = where + "   -   " + state;
    }

    sessionFileLabel.fileName   = isLinked ? linked.getFileName() : juce::String ("Untitled");
    sessionFileLabel.detailText = detail;
    sessionFileLabel.dirty      = dirty;
    sessionFileLabel.missing    = missing;
    sessionFileLabel.repaint();

    sessionWarningBar.messageLabel.setText (
        "CSD file not found: " + audioProcessor.getLinkedCsdDisplayPath()
        + ". The copy embedded in the project is in use: relocate the file, or save it as a new one.",
        juce::dontSendNotification);

    if (missing != sessionWarningBar.isVisible())
    {
        sessionWarningBar.setVisible (missing);
        resized();
    }
}


void CsoundAudioProcessorEditor::performSaveLinked (std::function<void()> onSaved)
{
    const auto linked = audioProcessor.getLinkedCsdFile();

    if (linked == juce::File{} || audioProcessor.isLinkedFileMissing())
    {
        // Sessione non collegata (istanza nuova, Initialize Session, vecchio
        // stato) OPPURE collegata a un file che non si trova piu' (barra
        // "file not found"): "Save" equivale a "Save As" - non si scrive
        // alla cieca su un path che non esiste piu'. onSaved passa al suo
        // callback, che scatta solo se il file viene davvero scritto.
        promptSaveSession (std::move (onSaved));
        return;
    }

    // Il file su disco e' cambiato da quando lo abbiamo letto/scritto
    // (un altro editor, un git pull...)? Confronto della DATA DI MODIFICA
    // (vedi hasLinkedFileChangedOnDisk) - MAI una sovrascrittura silenziosa
    // in quel caso. Le modifiche fatte QUI (codice/parametri) non c'entrano:
    // quelle sono proprio cio' che si sta per salvare.
    if (audioProcessor.hasLinkedFileChangedOnDisk())
    {
        const int choice = showNativeTwoButtonAlert (
            "File changed on disk",
            linked.getFileName() + " has been modified outside this session since it was loaded. "
            "Overwrite it with the current session?",
            "Cancel", "Overwrite");

        if (choice != 2)
            return;
    }

    const bool ok = audioProcessor.saveSessionToFile (linked, document.getAllContent());

    if (ok)
        updateSessionStatus();

    appendToLog (ok ? ("--- Session saved to " + linked.getFullPathName() + " ---")
                     : "--- Failed to save session (file not writable?) ---");

    // Continuazione (es. "Save" nel dialogo di Load CSD, che poi carica il
    // nuovo file): SOLO se il salvataggio e' riuscito, come in promptSaveSession.
    if (ok && onSaved)
        onSaved();
}

void CsoundAudioProcessorEditor::refreshSessionFromProcessor()
{
    // Caricamento "documento": niente newTransaction()/undo come per Load
    // CSD - si azzera TUTTO (vedi il commento in PluginEditor.h sul perche'
    // le due cronologie vanno svuotate insieme). Il listener del document
    // (bridgeCodeEditIntoSharedUndo) NON deve rispecchiare questo
    // replaceAllContent in sharedUndoManager: lo si spegne per la durata
    // della sostituzione, come fa gia' CodeEditTransactionProxy.
    {
        const juce::ScopedValueSetter<bool> guard (isApplyingCodeUndoRedo, true);
        document.replaceAllContent (audioProcessor.getCsdText());
    }

    document.clearUndoHistory();
    sharedUndoManager.clearUndoHistory();

    parameterPanel.refreshAllFromProcessor();
    updateApplyButtonDirtyState();
    updateSessionStatus();
}

void CsoundAudioProcessorEditor::sessionStateRestored()
{
    // Gia' sul message thread (vedi callAsync in setStateInformation).
    refreshSessionFromProcessor();

    const auto path = audioProcessor.getLinkedCsdDisplayPath();
    appendToLog (path.isNotEmpty() ? ("--- Session restored (linked to " + path + ") ---")
                                   : "--- Session restored ---");
}

void CsoundAudioProcessorEditor::performApply()
{
    // Pulisce la consolle ad ogni Apply: i messaggi/errori della
    // compilazione precedente non hanno piu' senso una volta applicato il
    // nuovo codice, e mischiati ai nuovi renderebbero il log confuso da
    // leggere.
    logConsole.clear();
    appendToLog ("--- Applying edited .csd ---");
    audioProcessor.compileAndStart (document.getAllContent());

    // Dopo compileAndStart il testo applicato (audioProcessor.getCsdText())
    // coincide di nuovo con quello dell'editor: il bordo rosso del bottone
    // scompare - vedi updateApplyButtonDirtyState().
    updateApplyButtonDirtyState();
}

void CsoundAudioProcessorEditor::OpcodeHelpBar::setHelpText (const juce::String& syntax, const juce::String& description)
{
    syntaxText = syntax;
    descriptionText = description;
    repaint();
}

CsoundAudioProcessorEditor::SidebarDivider::SidebarDivider()
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void CsoundAudioProcessorEditor::SidebarDivider::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xffd7dee3));
}

void CsoundAudioProcessorEditor::SidebarDivider::mouseDown (const juce::MouseEvent&)
{
    widthAtDragStart = getCurrentWidth ? getCurrentWidth() : 0;
}

void CsoundAudioProcessorEditor::SidebarDivider::mouseDrag (const juce::MouseEvent& event)
{
    // Trascinare verso sinistra (dx negativo) allarga la sidebar, verso
    // destra la restringe - il divisore sta a sinistra della sidebar,
    // quindi il segno e' invertito rispetto al semplice spostamento del
    // mouse. Il clamp vero e proprio (sidebarMinWidth/sidebarEditorMinWidth)
    // e' responsabilita' di CsoundAudioProcessorEditor::resized(), non di
    // questo componente - onDrag passa solo la larghezza "richiesta".
    if (onDrag)
        onDrag (widthAtDragStart - event.getDistanceFromDragStartX());
}

CsoundAudioProcessorEditor::ConsoleDivider::ConsoleDivider()
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void CsoundAudioProcessorEditor::ConsoleDivider::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xffd7dee3));
}

void CsoundAudioProcessorEditor::ConsoleDivider::mouseDown (const juce::MouseEvent&)
{
    heightAtDragStart = getCurrentHeight ? getCurrentHeight() : 0;
}

void CsoundAudioProcessorEditor::ConsoleDivider::mouseDrag (const juce::MouseEvent& event)
{
    // Vedi il commento identico su SidebarDivider::mouseDrag sopra: stessa
    // idea, sull'asse Y - il divisore sta SOPRA la consolle (ancorata al
    // bordo inferiore), quindi trascinare verso l'alto (dy negativo)
    // allarga la consolle, verso il basso la restringe.
    if (onDrag)
        onDrag (heightAtDragStart - event.getDistanceFromDragStartY());
}

void CsoundAudioProcessorEditor::OpcodeHelpBar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Stesso tono "post-it" del popup di autocompletamento, cosi' le due
    // forme di help inline (barra fissa + popup flottante) si riconoscono
    // come parte della stessa funzionalita'.
    g.setColour (juce::Colour (0xfffdf6e3));
    g.fillRect (bounds);
    g.setColour (juce::Colour (0xffd7c89a));
    g.drawLine (0.0f, (float) bounds.getHeight() - 0.5f, (float) bounds.getWidth(), (float) bounds.getHeight() - 0.5f, 1.0f);

    auto area = bounds.reduced (10, 0);

    if (syntaxText.isEmpty() && descriptionText.isEmpty())
    {
        g.setColour (juce::Colour (0xff8a7a55));
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::italic)));
        g.drawText ("Place the cursor on an opcode or type one to view inline help.", area, juce::Justification::centredLeft, true);
        return;
    }

    const auto monoFont = juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));

    // Font::getStringWidth/-Float non sono disponibili in questa versione
    // di JUCE: la via raccomandata per misurare il testo e' GlyphArrangement.
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (monoFont, syntaxText, 0.0f, 0.0f);
    const auto syntaxWidth = juce::jmin ((int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()),
                                          area.getWidth() / 2);

    auto syntaxArea = area.removeFromLeft (syntaxWidth);
    g.setFont (monoFont);
    g.setColour (juce::Colour (0xff5b4636));
    g.drawFittedText (syntaxText, syntaxArea, juce::Justification::centredLeft, 1);

    area.removeFromLeft (10);

    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.setColour (juce::Colour (0xff3a3a3a));
    g.drawFittedText (descriptionText, area, juce::Justification::centredLeft, 1);
}

void CsoundAudioProcessorEditor::csoundMessageReceived (const juce::String& message)
{
    appendToLog (message);
}

void CsoundAudioProcessorEditor::csoundEngineStarted()
{
    // Il motore viene (ri)compilato con un'istanza CSOUND* nuova ogni
    // volta (vedi compileAndStart): l'elenco opcode va quindi ripreso da
    // capo ogni volta che riparte, non solo alla creazione dell'editor.
    editor.setOpcodeSignatures (audioProcessor.getOpcodeSignatures());
}

void CsoundAudioProcessorEditor::csoundEngineStopped()
{
}

void CsoundAudioProcessorEditor::appendToLog (const juce::String& text)
{
    logConsole.moveCaretToEnd();
    logConsole.insertTextAtCaret (text + "\n");

    // Tiene la console a un numero limitato di righe: in sessioni lunghe
    // con molti messaggi un TextEditor che cresce senza limite diventa via
    // via piu' lento da ridisegnare/scrollare (indipendentemente dal buffer
    // circolare lato processore, che limita solo quanto viene ricaricato
    // alla (ri)apertura dell'editor, non quanto si accumula mentre e' aperto).
    constexpr int maxLines = 1000;
    auto lines = juce::StringArray::fromLines (logConsole.getText());

    if (lines.size() > maxLines)
    {
        lines.removeRange (0, lines.size() - maxLines);
        logConsole.setText (lines.joinIntoString ("\n") + "\n", false);
        logConsole.moveCaretToEnd();
    }
}
