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

        // Se il document non ha (piu') nulla da annullare/rifare le due
        // cronologie si sono disallineate: meglio un passo a vuoto che
        // annullare una transazione sbagliata. Non dovrebbe succedere (vedi
        // setMaxNumberOfStoredUnits nel costruttore del processor), e'
        // solo una rete di sicurezza.
        bool perform() override
        {
            if (! firstPerform)
            {
                const juce::ScopedValueSetter<bool> guard (guardFlag, true);

                if (document.getUndoManager().canRedo())
                    document.getUndoManager().redo();
                else
                    jassertfalse;
            }

            firstPerform = false;
            return true;
        }

        bool undo() override
        {
            const juce::ScopedValueSetter<bool> guard (guardFlag, true);

            if (document.getUndoManager().canUndo())
                document.getUndoManager().undo();
            else
                jassertfalse;

            return true;
        }

        juce::CodeDocument& document;
        bool& guardFlag;
        bool firstPerform = true;

        JUCE_DECLARE_NON_COPYABLE (CodeEditTransactionProxy)
    };

    // Azione di sharedUndoManager per una sostituzione di sessione (vedi
    // CsoundAudioProcessorEditor::replaceSessionUndoably): stesso schema
    // "prima perform() no-op" di CodeEditTransactionProxy, perche' la
    // sostituzione e' gia' avvenuta quando l'azione viene registrata.
    struct SessionReplaceAction final : public juce::UndoableAction
    {
        SessionReplaceAction (std::function<void()> redoFn, std::function<void()> undoFn)
            : redoIt (std::move (redoFn)), undoIt (std::move (undoFn)) {}

        bool perform() override
        {
            if (! firstPerform && redoIt)
                redoIt();

            firstPerform = false;
            return true;
        }

        bool undo() override
        {
            if (undoIt)
                undoIt();
            return true;
        }

        std::function<void()> redoIt, undoIt;
        bool firstPerform = true;

        JUCE_DECLARE_NON_COPYABLE (SessionReplaceAction)
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
        // La cartella viene creata QUI, solo quando serve davvero (punto 10).
        auto folder = CsoundAudioProcessor::getBaseFolder();
        folder.createDirectory();
        return folder;
    }

    // Preferenze dell'UTENTE (non del progetto): poche opzioni di
    // visualizzazione valide per ogni istanza del plugin, es. la sintassi
    // moderna nella barra di help. ~/Library/Application Support/apeCsound/
    // apeCsound.settings su macOS. Niente di cio' che riguarda la sessione
    // (path, codice, parametri) va qui: quello sta nello stato del progetto.
    juce::PropertiesFile& getUserSettings()
    {
        static std::unique_ptr<juce::PropertiesFile> settings = []
        {
            juce::PropertiesFile::Options options;
            options.applicationName     = ProjectInfo::projectName;
            options.filenameSuffix      = "settings";
            options.folderName          = ProjectInfo::projectName;
            options.osxLibrarySubFolder = "Application Support";
            options.storageFormat       = juce::PropertiesFile::storeAsXML;
            return std::make_unique<juce::PropertiesFile> (options);
        }();

        return *settings;
    }

    const char* const kModernSyntaxHelpKey = "modernSyntaxHelp";
    const char* const kEditorFontSizeKey   = "editorFontSize"; // punti; consolle = -2
    const char* const kEditorDarkThemeKey  = "editorDarkTheme"; // palette scura dell'editor (default true)
    constexpr int kDefaultEditorFontSize = 15, kMinEditorFontSize = 9, kMaxEditorFontSize = 32;

    // Palette della toolbar (dal mockup "R2 - Capsula centrale"): fondo
    // quasi nero, bottoni teal chiaro con icona/testo scuri, capsula
    // centrale scura con bordo sottile.
    const juce::Colour kToolbarBg        { 0xff161b21 };
    const juce::Colour kToolbarLine      { 0xff2a333d };
    const juce::Colour kToolbarAccent    { 0xff4aa3b8 };
    const juce::Colour kToolbarOnAccent  { 0xff0b1116 };
    const juce::Colour kCapsuleBg        { 0xff222a33 };
    const juce::Colour kCapsuleBorder    { 0xff313c47 };
    const juce::Colour kCapsuleName      { 0xffe6edf2 };
    const juce::Colour kCapsulePath      { 0xff8a9caa };
    const juce::Colour kCapsuleState     { 0xff8fd0df };
    const juce::Colour kCapsuleDirty     { 0xfff0b429 };
    const juce::Colour kCapsuleMissing   { 0xffff6b6b };

    // Manuale di riferimento Csound 7 (csound.com/manual e' la versione 7.x;
    // csound.com/docs/manual e' ancora la 6.x).
    const char* const kCsoundManualUrl = "https://csound.com/manual/";
}

//==============================================================================
CsoundAudioProcessorEditor::CsoundAudioProcessorEditor (CsoundAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    // Tastiera a schermo iOS: riduce il layout allo spazio visibile (vedi
    // keyboardInset in PluginEditor.h). Sulle altre piattaforme non arriva
    // mai nessuna notifica.
    IOSKeyboard::initialise();
    keyboardWatcher.onChange = [this] { updateKeyboardInset(); };
    IOSKeyboard::getBroadcaster().addChangeListener (&keyboardWatcher);

    // getEditorDraft(), non getCsdText(): riapre l'editor sul testo che
    // l'utente aveva lasciato, anche se non ancora applicato (vedi
    // CsoundAudioProcessor::setEditorDraft) - BUG corretto: chiudere e
    // riaprire la finestra del plugin perdeva le modifiche non applicate.
    //
    // Il documento e la cronologia vivono nel processor e sopravvivono alla
    // chiusura della finestra: si ricarica il testo (e si azzera la
    // cronologia) SOLO alla prima apertura o se nel frattempo l'host ha
    // sostituito la sessione (consumeDocumentResyncRequest), o per sicurezza
    // se il documento non coincide con la bozza. Altrimenti si riprende
    // esattamente da dove si era, undo compreso (richiesta esplicita).
    if (audioProcessor.consumeDocumentResyncRequest()
        || document.getAllContent() != audioProcessor.getEditorDraft())
    {
        document.replaceAllContent (audioProcessor.getEditorDraft());
        document.clearUndoHistory();
        sharedUndoManager.clearUndoHistory();
    }

    sessionStatusDebouncer.callback = [this] { updateSessionStatus(); };

    // Secondo listener indipendente sullo stesso document (vedi il
    // commento in PluginEditor.h): aggiunto DOPO il replaceAllContent qui
    // sopra apposta, altrimenti il caricamento iniziale del testo
    // (identico a audioProcessor.getCsdText() per definizione) scatenerebbe
    // una chiamata a updateApplyButtonDirtyState() inutile - a questo punto
    // comunque risulterebbe "non modificato", quindi non cambia nulla nella
    // pratica, ma e' piu' chiaro cosi'.
    document.addListener (this);

    addAndMakeVisible (editor);

    logConsole.setMultiLine (true);
    logConsole.setReadOnly (true);
    logConsole.setScrollbarsShown (true);
    logConsole.setCaretVisible (false);
    // Console scura "da terminale" di proposito, a contrasto col resto
    // dell'interfaccia chiara: accento teal sul testo per restare in tema.
    logConsole.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff10181f));
    logConsole.setColour (juce::TextEditor::textColourId, juce::Colour (0xff8fd9e0));
    // Niente cornice: LookAndFeel_V4 disegna un bordo di 1 px (outline) e
    // una riga in fondo nel colore outline, che sullo sfondo scuro della
    // consolle sembravano un margine a sinistra/in basso.
    logConsole.setColour (juce::TextEditor::outlineColourId,        juce::Colour (0xff10181f));
    logConsole.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff10181f));
    logConsole.setBorder (juce::BorderSize<int> (0));
    logConsole.setIndents (8, 6); // inset del testo a sinistra / in alto (richiesta esplicita)
    addAndMakeVisible (logConsole);

    // Dimensione del testo di editor e consolle: preferenza utente
    // persistente (Config > Larger/Smaller Text).
    applyEditorFontSize (getUserSettings().getIntValue (kEditorFontSizeKey, kDefaultEditorFontSize));
    applyEditorTheme (getUserSettings().getBoolValue (kEditorDarkThemeKey, true)); // scuro di default

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
    editor.onOpcodeHelpChanged = [this] (const juce::String& syntax, const juce::String& description, const juce::String& category)
    {
        opcodeHelpBar.setHelpText (syntax, description, category);
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

    // Vista "About" (vedi AboutView in PluginEditor.h): nascosta, aperta dal
    // menu hamburger tramite parameterPanel.onAboutRequested.
    addChildComponent (aboutView);
    addChildComponent (guideView);

    // Cerca/Sostituisci (vedi FindReplaceBar in PluginEditor.h). Bottone
    // lente circolare come clearConsoleButton (icona "find" in
    // CsoundLookAndFeel); quando la barra e' aperta resta "acceso"
    // (toggle state -> buttonOnColourId).
    findButton.setName ("find");
    findButton.getProperties().set ("circular", true);
    //findButton.setTooltip ("Find / Replace in the code editor");
    findButton.onClick = [this] { toggleFindBar(); };
    addAndMakeVisible (findButton);
    addChildComponent (findBar);

    findBar.findField.onTextChange    = [this] { updateFindQuery(); };
    findBar.findField.onReturnKey     = [this] { editor.selectNextSearchMatch(); };
    findBar.findField.onEscapeKey     = [this] { closeFindBar(); };
    findBar.replaceField.onReturnKey  = [this] { editor.replaceCurrentSearchMatch (findBar.replaceField.getText()); };
    findBar.replaceField.onEscapeKey  = [this] { closeFindBar(); };
    findBar.matchCaseButton.onClick   = [this] { updateFindQuery(); };
    findBar.wholeWordButton.onClick   = [this] { updateFindQuery(); };
    findBar.nextButton.onClick        = [this] { editor.selectNextSearchMatch(); };
    findBar.prevButton.onClick        = [this] { editor.selectPreviousSearchMatch(); };
    findBar.replaceButton.onClick     = [this] { editor.replaceCurrentSearchMatch (findBar.replaceField.getText()); };
    findBar.replaceAllButton.onClick  = [this]
    {
        const int replaced = editor.replaceAllSearchMatches (findBar.replaceField.getText());
        if (replaced > 0)
            appendToLog ("--- Replaced " + juce::String (replaced) + " occurrence(s) ---");
    };
    findBar.closeButton.onClick       = [this] { closeFindBar(); };
    editor.onSearchResultsChanged     = [this] { updateFindCount(); };
    updateSessionStatus();

    // Il nome del Component e' come CsoundLookAndFeel sceglie quale icona
    // disegnare (vedi getIconPathForButtonName) - non ha altro effetto.
    // Il motore Csound e' sempre in esecuzione (parte da solo in
    // prepareToPlay): questo bottone non "avvia" nulla, si limita a
    // rimpiazzare il .csd corrente con quello appena modificato nell'editor.
    applyButton.setName ("apply");
    applyButton.getProperties().set ("circular", true); // capsula (vedi CsoundLookAndFeel)
    applyButton.onClick = [this] { performApply(); };

    // Stile toolbar: teal chiaro con icona/testo scuri (vedi kToolbarAccent).
    for (juce::Button* b : { static_cast<juce::Button*> (&applyButton),
                             static_cast<juce::Button*> (&findButton),
                             static_cast<juce::Button*> (&parameterPanel.getMenuButton()) })
    {
        b->setColour (juce::TextButton::buttonColourId,   kToolbarAccent);
        b->setColour (juce::TextButton::buttonOnColourId, kToolbarAccent.darker (0.2f));
        b->setColour (juce::TextButton::textColourOffId,  kToolbarOnAccent);
        b->setColour (juce::TextButton::textColourOnId,   kToolbarOnAccent);
    }
    applyButton.setTooltip ("Apply: recompile and run Csound code (red outline indicates unsaved changes to the running code).");
    addAndMakeVisible (applyButton);

    // Bozza non applicata ripresa dal processor: bordo rosso subito.
    updateApplyButtonDirtyState();

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
    parameterPanel.onAboutRequested       = [this] { aboutView.show(); };
    parameterPanel.isDarkEditorEnabled = [this] { return getUserSettings().getBoolValue (kEditorDarkThemeKey, true); };
    parameterPanel.onToggleDarkEditorRequested = [this]
    {
        applyEditorTheme (! getUserSettings().getBoolValue (kEditorDarkThemeKey, true));
    };
    parameterPanel.onFontSizeChangeRequested = [this] (int delta)
    {
        const int current = getUserSettings().getIntValue (kEditorFontSizeKey, kDefaultEditorFontSize);
        applyEditorFontSize (juce::jlimit (kMinEditorFontSize, kMaxEditorFontSize, current + delta));
    };
    parameterPanel.onGuideRequested       = [this] { guideView.show(); };

    // Sintassi moderna nella barra di help: preferenza utente persistente
    // (vedi getUserSettings()), applicata all'editor subito e a ogni toggle.
    editor.setModernSyntaxHelp (getUserSettings().getBoolValue (kModernSyntaxHelpKey, false));
    parameterPanel.isModernSyntaxEnabled = [this] { return editor.isModernSyntaxHelp(); };
    parameterPanel.onToggleModernSyntaxRequested = [this]
    {
        const bool enable = ! editor.isModernSyntaxHelp();
        editor.setModernSyntaxHelp (enable);
        getUserSettings().setValue (kModernSyntaxHelpKey, enable);
        getUserSettings().saveIfNeeded();
    };
    parameterPanel.onOpenManualRequested = [] { juce::URL (kCsoundManualUrl).launchInDefaultBrowser(); };

    // Canali: Csound segue la DAW (default) o il .csd - vedi
    // CsoundAudioProcessor::setFollowCsdChannels (salvato nello stato del
    // progetto, ricompila subito).
    parameterPanel.isFollowingCsdChannels = [this] { return audioProcessor.isFollowingCsdChannels(); };
    parameterPanel.onToggleFollowCsdChannelsRequested = [this]
    {
        audioProcessor.setFollowCsdChannels (! audioProcessor.isFollowingCsdChannels());
    };

    // Clic sulla capsula del file: Save / Save as... (stesse azioni del
    // menu principale, vedi performSaveLinked/promptSaveSession).
    sessionFileLabel.onClick = [this]
    {
        std::vector<CsoundActionSheetItem> items;
        {
            CsoundActionSheetItem item;
            item.id = 1; item.text = "Save"; item.icon = CsoundActionSheetIcon::save;
            items.push_back (item);
        }
        {
            CsoundActionSheetItem item;
            item.id = 2; item.text = "Save as..."; item.icon = CsoundActionSheetIcon::save;
            items.push_back (item);
        }
        {
            CsoundActionSheetItem item;
            item.id = 3; item.text = "Load..."; item.icon = CsoundActionSheetIcon::load;
            items.push_back (item);
        }
        items.push_back (CsoundActionSheetItem::separator());
        {
            CsoundActionSheetItem item;
            item.id = 4; item.text = "Init Session Template"; item.icon = CsoundActionSheetIcon::newDocument;
            items.push_back (item);
        }
        {
            CsoundActionSheetItem item;
            item.id = 5; item.text = "Init Session Clear"; item.icon = CsoundActionSheetIcon::newDocument;
            items.push_back (item);
        }

        juce::Component::SafePointer<CsoundAudioProcessorEditor> safeThis (this);

        CsoundActionSheet::show (*this, "", std::move (items), [safeThis] (int result)
        {
            if (safeThis == nullptr)
                return;

            switch (result)
            {
                case 1: safeThis->performSaveLinked();      break;
                case 2: safeThis->promptSaveSession();      break;
                case 3: safeThis->promptLoadSession();      break;
                case 4: safeThis->promptInitializeSession (true);  break;
                case 5: safeThis->promptInitializeSession (false); break;
                default: break;
            }
        });
    };

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

    // Nessun focus da tastiera automatico all'apertura (richiesta esplicita,
    // su tutte le piattaforme): su iOS farebbe comparire subito la tastiera
    // a schermo, su desktop il focus arriva con il primo clic sull'editor.
}

CsoundAudioProcessorEditor::~CsoundAudioProcessorEditor()
{
    IOSKeyboard::getBroadcaster().removeChangeListener (&keyboardWatcher);
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
    g.setColour (kToolbarBg);
    g.fillRect (toolbarBounds);

    g.setColour (kToolbarLine);
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

    // getNumActionsInCurrentTransaction() == 0: questa e' la PRIMA
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
    //
    // == 0, NON == 1 (BUG corretto con CodeView): questo listener scatta
    // DENTRO UndoableAction::perform(), cioe' PRIMA che juce::UndoManager
    // aggiunga l'azione alla transazione (vedi UndoManager::perform: prima
    // action->perform(), poi actions.add). Quindi per la PRIMA azione di
    // una transazione fresca il conteggio e' 0 (newTransaction ancora
    // pendente), per la seconda e' 1, e cosi' via. Con "== 1" il proxy
    // nasceva solo alla SECONDA azione: con CodeEditorComponent ogni
    // battuta produceva due azioni (deleteSection vuoto + insert) e la
    // cosa passava inosservata, con CodeView (una sola azione per
    // battuta) le transazioni di una sola azione - una lettera, un paste,
    // un backspace - restavano SENZA proxy e le due cronologie si
    // disallineavano ("perde la storia").
    if (document.getUndoManager().getNumActionsInCurrentTransaction() == 0)
    {
        sharedUndoManager.beginNewTransaction();
        sharedUndoManager.perform (new CodeEditTransactionProxy (document, isApplyingCodeUndoRedo));
    }
}

void CsoundAudioProcessorEditor::applyEditorFontSize (int points)
{
    points = juce::jlimit (kMinEditorFontSize, kMaxEditorFontSize, points);
    getUserSettings().setValue (kEditorFontSizeKey, points);

    editor.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), (float) points, juce::Font::plain));
    logConsole.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), (float) juce::jmax (8, points - 2), juce::Font::plain));
    logConsole.applyFontToAllText (logConsole.getFont());
    editor.scrollToKeepCaretOnScreen();
}

void CsoundAudioProcessorEditor::applyEditorTheme (bool dark)
{
    getUserSettings().setValue (kEditorDarkThemeKey, dark);

    if (dark)
    {
        // Palette scura: colori impostati direttamente sul componente
        // (hanno la precedenza su quelli della LookAndFeel).
        const juce::Colour bg (0xff1b2027), gutter (0xff161a20), text (0xffe6edf3), muted (0xff6f7a85);
        editor.setColour (CodeView::backgroundColourId,     bg);
        editor.setColour (CodeView::defaultTextColourId,    text);
        editor.setColour (CodeView::lineNumberBackgroundId, gutter);
        editor.setColour (CodeView::lineNumberTextId,       muted);
        editor.setColour (CodeView::highlightColourId,      kToolbarAccent.withAlpha (0.35f));
        editor.setColour (CodeView::caretColourId,          kToolbarAccent);
        editor.setColourScheme (CsoundTokeniser::getDarkColourScheme());
    }
    else
    {
        // Palette chiara: si torna ai colori della LookAndFeel.
        for (int id : { (int) CodeView::backgroundColourId, (int) CodeView::defaultTextColourId,
                        (int) CodeView::lineNumberBackgroundId, (int) CodeView::lineNumberTextId,
                        (int) CodeView::highlightColourId, (int) CodeView::caretColourId })
            editor.removeColour (id);

        editor.setColourScheme (tokeniser.getDefaultColourScheme());
    }

    editor.repaint();
}

void CsoundAudioProcessorEditor::updateKeyboardInset()
{
    // Sovrapposizione fra il bordo inferiore di questa finestra e la
    // tastiera, in coordinate dello schermo (punti): e' lo spazio da
    // lasciare libero in fondo. Mai oltre meta' finestra, cosi' il layout
    // resta sensato anche con tastiere molto alte su finestre piccole.
    const auto keyboard = IOSKeyboard::getFrameOnScreen();
    int inset = 0;

    if (! keyboard.isEmpty())
    {
        const auto screenBounds = getScreenBounds();
        inset = juce::jlimit (0, getHeight(), screenBounds.getBottom() - keyboard.getY());
    }

    if (inset != keyboardInset)
    {
        keyboardInset = inset;
        resized();
        editor.scrollToKeepCaretOnScreen(); // caret/selezione nello spazio rimasto visibile
    }
}

void CsoundAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Overlay "About": sempre a tutta finestra (anche se nascosto, cosi' e'
    // gia' dimensionato quando lo si apre).
    aboutView.setBounds (area);
    guideView.setBounds (area);

    toolbarBounds = area.removeFromTop (toolbarHeight);

    auto toolbar = toolbarBounds.reduced (12, 8);

    // Dimensioni proporzionate al contenuto (testo + icona), non larghezze
    // fisse arbitrarie: TextButton::getBestWidthForHeight misura il testo
    // con il font reale della LookAndFeel, CsoundLookAndFeel::getIconAllowance
    // aggiunge lo spazio occupato dall'icona.
    const auto applyWidth = applyButton.getBestWidthForHeight (toolbar.getHeight())
                           + CsoundLookAndFeel::getIconAllowance (applyButton.getName())
                           + 12; // margini interni piu' ampi della capsula (vedi drawButtonText)

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

    // Lente (Cerca/Sostituisci) a destra, subito prima del burger
    // (richiesta esplicita).
    toolbar.removeFromRight (12);
    findButton.setBounds (toolbar.removeFromRight (panelToolbarButtonDiameter)
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
    sessionFileLabel.setBounds (toolbar.reduced (14, 0)
                                       .withSizeKeepingCentre (juce::jmax (0, toolbar.getWidth() - 28), panelToolbarButtonDiameter));

    // Barra "file non trovato" (vedi SessionWarningBar in PluginEditor.h):
    // a tutta larghezza subito sotto la toolbar, SOLO quando visibile -
    // altrimenti non sottrae spazio a editor/sidebar/consolle.
    if (sessionWarningBar.isVisible())
        sessionWarningBar.setBounds (area.removeFromTop (sessionWarningBarHeight));

    // Barra Cerca/Sostituisci, solo quando aperta.
    if (findBar.isVisible())
        findBar.setBounds (area.removeFromTop (findBarHeight));

    // Niente inset laterali: solo lo spazio verticale tra toolbar ed editor
    // resta. Sidebar ancorata A DESTRA, su tutta l'altezza rimanente (editor
    // + opcodeHelpBar + consolle si restringono insieme, non solo l'editor)
    // quando visibile - vedi il commento in testa a parameterPanel in
    // PluginEditor.h sul perche' (richiesta esplicita: editor sempre
    // accessibile/editabile sia per editing che per il drag della maniglia,
    // mai coperto da un overlay).
    // (Nessuno spazio tra toolbar ed editor: richiesta esplicita, il testo
    // parte subito sotto la riga della toolbar.)

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
        sidebarDivider.setBounds (dividerArea.expanded (dividerGrabMargin, 0)); // presa larga, striscia stretta
        sidebarDivider.toFront (false);
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
        consoleDivider.setBounds (consoleDividerArea.expanded (0, dividerGrabMargin)); // presa alta, striscia stretta
        consoleDivider.toFront (false);

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

    // Tastiera a schermo (iOS, vedi keyboardInset): si contrae SOLO il
    // blocco editor + barra di help, in modo che finisca sopra la tastiera;
    // la consolle (e tutto il resto) resta al suo posto, coperta.
    if (keyboardInset > 0)
    {
        const int visibleBottom = getHeight() - keyboardInset;
        const int minEditorHeight = opcodeHelpBarHeight + 3 * editor.getLineHeight();

        if (area.getBottom() > visibleBottom)
            area.setBottom (juce::jmax (area.getY() + minEditorHeight, visibleBottom));
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

    chooseCsdFile ("Save as...", startingFile, true,
        [this, onSaved] (const juce::File& chosen)
        {
            auto file = chosen;

            if (file == juce::File{})
                return; // annullato dall'utente - onSaved NON scatta

           #if ! JUCE_IOS
            // Non su iOS: il file scelto dal picker e' l'unico a cui abbiamo
            // accesso (bookmark); cambiargli nome lo renderebbe inaccessibile.
            if (! file.hasFileExtension ("csd"))
                file = file.withFileExtension ("csd");
           #endif

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

void CsoundAudioProcessorEditor::chooseCsdFile (const juce::String& title, const juce::File& initialFile, bool saveMode,
                                                 std::function<void (const juce::File&)> onResult)
{
    // Dialogo NATIVO di sistema (NSOpenPanel/NSSavePanel su macOS,
    // UIDocumentPickerViewController su iOS) con QUESTO editor come
    // parentComponent (ultimo argomento). Indispensabile su iOS: in
    // un'estensione AUv3 JUCE non puo' aprire una finestra top-level
    // (sandbox) e, senza parent, il picker finiva fuori dalla gerarchia
    // dell'host - "in background" - lasciando la UI del plugin bloccata in
    // attesa. Con il parent, JUCE lo presenta come page sheet dal view
    // controller della view del plugin (vedi displayNativeWindowModally in
    // juce_NativeModalWrapperComponent_ios.h), dentro l'host.
    activeFileChooser = std::make_unique<juce::FileChooser> (title, initialFile, "*.csd",
                                                            true,   // dialogo nativo
                                                            false,  // i pacchetti non sono cartelle
                                                            this);  // parent: l'editor del plugin

    const int flags = saveMode
        ? (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
           | juce::FileBrowserComponent::warnAboutOverwriting)
        : (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles);

    const auto proposedName = initialFile.isDirectory() ? juce::String ("Untitled.csd") : initialFile.getFileName();

    activeFileChooser->launchAsync (flags, [this, onResult, saveMode, proposedName] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        auto url  = chooser.getURLResult();

        // iOS, Save: in alcuni casi (es. radice di iCloud Drive) il document
        // picker restituisce la CARTELLA di destinazione invece del file
        // esportato (visto in console: "cannot open for writing: .../
        // com~apple~CloudDocs (Is a directory)"). Il file e' la cartella +
        // il nome proposto; la URL figlia eredita l'accesso della cartella
        // (stesso schema di NP2: fc.getURLResult().getChildURL (fileName)).
        if (saveMode && file != juce::File{} && file.isDirectory())
        {
            file = file.getChildFile (proposedName);
            url  = url.getChildURL (proposedName);
        }

        // iOS: la URL del picker porta il bookmark security-scoped del file
        // scelto (anche fuori dalla sandbox: iCloud Drive, altri provider).
        // Va consegnata al processor PRIMA di leggere/scrivere quel file -
        // vedi SecurityScopedFile.h e CsoundAudioProcessor::makeFileURL.
        if (file != juce::File{})
            audioProcessor.setFileURL (file, url);

        if (onResult)
            onResult (file);
    });
}

void CsoundAudioProcessorEditor::promptLoadSession()
{
    const auto startingDir = getCsdChooserStartDirectory();

    chooseCsdFile ("Load...", startingDir, false,
        [this] (const juce::File& file)
        {
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
    juce::Component::SafePointer<CsoundAudioProcessorEditor> safeThis (this);

    showNativeThreeButtonAlertAsync ("Unsaved changes", message, "Save", "Don't Save", "Cancel",
                                     [safeThis, file] (int result)
    {
        if (safeThis == nullptr)
            return;

        if (result == 1)
            safeThis->performSaveLinked ([safeThis, file] { if (safeThis != nullptr) safeThis->performLoadSessionFile (file); });
        else if (result == 2)
            safeThis->performLoadSessionFile (file);
    }, this);
}

void CsoundAudioProcessorEditor::performLoadSessionFile (const juce::File& file)
{
    // Nessuna copia/importazione nella cartella base (scelta esplicita): un
    // .csd fuori da ~/Documents/apeCsound resta dov'e', collegato con
    // path ASSOLUTO. Se poi il file non si trova piu' (altra macchina, file
    // spostato), il progetto suona comunque la copia incorporata e
    // l'editor mostra la barra "file non trovato" con Save As... - vedi
    // SessionWarningBar in PluginEditor.h.
    // Annullabile come un solo passo (testo + struttura + collegamento):
    // vedi replaceSessionUndoably.
    if (! replaceSessionUndoably ([this, file] { return audioProcessor.loadSessionFromFile (file); }))
    {
        // Diagnostica (es. iOS: file non leggibile/non ancora scaricato da
        // iCloud, o senza <CsoundSynthesizer>): prima falliva in silenzio.
        appendToLog ("--- Failed to load " + file.getFullPathName()
                     + " (unreadable, empty, or no <CsoundSynthesizer> block) ---");
        return;
    }

    {
        // replaceSessionUndoably ha gia' riletto editor e pannello dal
        // processor (il reset dei 4 tipi di slot in loadSessionFromFile fa
        // si' che un .csd senza parametri svuoti davvero il pannello) e reso
        // il Load annullabile come UN solo passo: Cmd+Z riporta insieme
        // testo, struttura dei parametri e collegamento al file precedenti
        // (non ricompila: bordo rosso su Apply).

        // Il replaceAllContent di replaceSessionUndoably ha gia' fatto scattare
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
}

void CsoundAudioProcessorEditor::promptInitializeSession (bool useTemplate)
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
    juce::Component::SafePointer<CsoundAudioProcessorEditor> safeThis (this);

    showNativeTwoButtonAlertAsync (
        "Initialize session?",
        useTemplate ? "This will replace the current code and parameters with the built-in template. "
                      "The code change can be undone afterwards; the parameter mapping cannot."
                    : "This will remove all parameters and replace the current code with an empty .csd. "
                      "The code change can be undone afterwards; the parameter mapping cannot.",
        "Cancel", "Initialize",
        [safeThis, useTemplate] (int choice)
    {
        if (safeThis != nullptr && choice == 2)
            safeThis->performInitializeSession (useTemplate);
    }, this);
}

void CsoundAudioProcessorEditor::performInitializeSession (bool useTemplate)
{
    // Stesso ordine/commenti di performLoadSessionFile() sopra: il
    // processor e' la fonte di verita' (initializeSession() azzera i 4
    // tipi di slot e sostituisce getCsdText() con defaultCsdText()),
    // document/parameterPanel vanno rilette esplicitamente DOPO.
    // Annullabile come un solo passo (testo + struttura + collegamento):
    // vedi replaceSessionUndoably.
    replaceSessionUndoably ([this, useTemplate] { audioProcessor.initializeSession (useTemplate); return true; });

    markApplyPendingAfterLoad();

    appendToLog (useTemplate ? "--- Session initialized (default template) ---"
                             : "--- Session initialized (empty) ---");

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
    // Capsula scura con bordo sottile (mockup "R2 - Capsula centrale"), su
    // due righe (richiesta esplicita): sopra "[pallino] Nome.csd", un po'
    // alzato; sotto "cartella   stato", piccolo e attenuato. Tutto centrato.
    auto area = getLocalBounds().toFloat();
    const float radius = area.getHeight() * 0.5f;

    g.setColour (hovered ? kCapsuleBg.brighter (0.12f) : kCapsuleBg);
    g.fillRoundedRectangle (area, radius);
    g.setColour (hovered ? kCapsuleBorder.brighter (0.25f) : kCapsuleBorder);
    g.drawRoundedRectangle (area.reduced (0.5f), radius - 0.5f, 1.0f);

    const auto dotColour   = missing ? kCapsuleMissing : dirty ? kCapsuleDirty : kToolbarAccent;
    const auto stateColour = missing ? kCapsuleMissing : dirty ? kCapsuleDirty : kCapsuleState;

    const auto nameFont   = juce::Font (juce::FontOptions (14.0f, juce::Font::bold));
    const auto detailFont = juce::Font (juce::FontOptions (11.0f));

    auto textWidth = [] (const juce::Font& f, const juce::String& t)
    {
        if (t.isEmpty())
            return 0.0f;

        juce::GlyphArrangement ga;
        ga.addLineOfText (f, t, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    };

    const float maxTotal = area.getWidth() - 2.0f * radius;
    auto inner = area.reduced (radius, 0.0f);

    // Riga 1 (spostata leggermente in alto): pallino + nome, e a destra
    // del nome lo stato ("saved" / "unsaved changes" / "file not found").
    {
        const float dotSize = 7.0f, gapDot = 8.0f, gapState = 12.0f;
        const float wState = textWidth (detailFont, stateText);
        const float wName  = juce::jmin (textWidth (nameFont, fileName),
                                         maxTotal - dotSize - gapDot - (wState > 0.0f ? gapState + wState : 0.0f));
        const float total  = dotSize + gapDot + wName + (wState > 0.0f ? gapState + wState : 0.0f);
        float x = area.getCentreX() - total * 0.5f;

        auto row = inner.withHeight (area.getHeight() * 0.5f).translated (0.0f, 1.5f);

        g.setColour (dotColour);
        g.fillEllipse (x, row.getCentreY() - dotSize * 0.5f, dotSize, dotSize);
        x += dotSize + gapDot;

        g.setColour (missing ? kCapsuleMissing : kCapsuleName);
        g.setFont (nameFont);
        g.drawFittedText (fileName, juce::Rectangle<float> (x, row.getY(), wName + 2.0f, row.getHeight()).toNearestInt(),
                          juce::Justification::centredLeft, 1, 1.0f);
        x += wName + gapState;

        if (wState > 0.0f)
        {
            g.setColour (stateColour);
            g.setFont (detailFont);
            g.drawText (stateText, juce::Rectangle<float> (x, row.getY(), wState + 2.0f, row.getHeight()),
                        juce::Justification::centredLeft, false);
        }
    }

    // Riga 2: la cartella su TUTTA la larghezza disponibile della capsula,
    // centrata; se non ci sta viene accorciata con "...".
    if (locationText.isNotEmpty())
    {
        auto row = inner.withHeight (area.getHeight() * 0.5f).withY (area.getCentreY() - 2.0f);

        g.setColour (kCapsulePath);
        g.setFont (detailFont);
        g.drawFittedText (locationText, row.toNearestInt(), juce::Justification::centred, 1, 1.0f);
    }
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

    chooseCsdFile ("Relocate CSD...", startingDir, false,
        [this] (const juce::File& file)
        {
            if (file == juce::File{})
                return;

            // Il contenuto in uso non esiste su nessun file se la sessione
            // era "•" quando il progetto e' stato salvato, o se e' stata
            // modificata dopo il ripristino: chiedere prima di sostituirlo
            // (BUG corretto: prima si sostituiva in silenzio). Annullabile
            // comunque con Undo (vedi replaceSessionUndoably).
            if (audioProcessor.wasRestoredDirty() || currentSessionHash() != audioProcessor.getRestoredSessionHash())
            {
                juce::Component::SafePointer<CsoundAudioProcessorEditor> safeThis (this);

                showNativeTwoButtonAlertAsync (
                    "Replace current content?",
                    "The code and parameter mapping currently in use are not saved to any file. "
                    "Relocating will replace them with the contents of " + file.getFileName() + ". "
                    "Use Save As... instead to keep them.",
                    "Cancel", "Relocate",
                    [safeThis, file] (int choice)
                {
                    if (safeThis != nullptr && choice == 2)
                        safeThis->performLoadSessionFile (file);
                }, this);
                return;
            }

            performLoadSessionFile (file); // carica + collega + applica; azzera "file mancante"
        });
}

bool CsoundAudioProcessorEditor::isSessionDirty() const
{
    // Unica fonte nel processor (vedi CsoundAudioProcessor::isSessionDirty):
    // legge la bozza, che l'editor tiene aggiornata a ogni modifica.
    return audioProcessor.isSessionDirty();
}

juce::String CsoundAudioProcessorEditor::currentSessionHash()
{
    return CsoundAudioProcessor::computeTextHash (audioProcessor.buildSessionTextFor (document.getAllContent()));
}

bool CsoundAudioProcessorEditor::replaceSessionUndoably (const std::function<bool()>& loadIntoProcessor)
{
    const auto before = audioProcessor.captureSessionSnapshot();

    if (! loadIntoProcessor())
        return false;

    const auto after = audioProcessor.captureSessionSnapshot();

    // Testo dell'editor in una transazione DEDICATA del document (chiusa
    // prima e dopo), senza bridge: sara' l'azione qui sotto a fare undo/redo
    // di questa transazione, insieme al resto della sessione - le due
    // cronologie (document e sharedUndoManager) restano allineate.
    // Punto 12: se sia il testo vecchio sia quello nuovo sono vuoti,
    // replaceAllContent non crea nessuna azione nel document - e l'undo/redo
    // qui sotto NON deve toccarlo (annullerebbe una transazione estranea).
    const bool documentChanged = document.getNumCharacters() > 0 || audioProcessor.getCsdText().isNotEmpty();

    {
        const juce::ScopedValueSetter<bool> guard (isApplyingCodeUndoRedo, true);
        document.newTransaction();
        document.replaceAllContent (audioProcessor.getCsdText());
        document.newTransaction();
    }

    // Le lambda NON catturano `this` (l'editor): l'azione vive nella
    // cronologia del PROCESSOR e puo' essere eseguita dopo che questa
    // finestra e' stata chiusa e riaperta (un'altra istanza di editor).
    // Usano solo oggetti del processor e notificano l'interfaccia, se c'e'.
    auto& proc  = audioProcessor;
    auto& doc   = document;
    auto& guard = isApplyingCodeUndoRedo;

    auto applySide = [&proc, &doc, &guard, documentChanged] (const CsoundAudioProcessor::SessionSnapshot& snap, bool isUndo)
    {
        if (documentChanged)
        {
            const juce::ScopedValueSetter<bool> g (guard, true);
            if (isUndo)
                doc.getUndoManager().undo();
            else
                doc.getUndoManager().redo();
        }

        proc.setEditorDraft (doc.getAllContent());
        proc.restoreSessionSnapshot (snap);
        proc.notifySessionEditedByUndoRedo (true);
    };

    sharedUndoManager.beginNewTransaction();
    sharedUndoManager.perform (new SessionReplaceAction (
        [applySide, after]  { applySide (after,  false); },
        [applySide, before] { applySide (before, true);  }));

    parameterPanel.refreshAllFromProcessor();
    updateSessionStatus();
    return true;
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
    juce::String where, state;

    if (! isLinked)
    {
        state = "not saved yet";
    }
    else
    {
        const auto base = CsoundAudioProcessor::getBaseFolder();
        const auto folder = linked.getParentDirectory();
        where = linked.isAChildOf (base)
            ? "apeCsound/" + folder.getRelativePathFrom (base).replaceCharacter ('\\', '/')
            : folder.getFullPathName();

        if (where.endsWith ("/."))
            where = where.dropLastCharacters (2);

        state = missing ? "file not found"
              : dirty   ? "unsaved changes"
                        : "saved";
    }

    sessionFileLabel.fileName     = isLinked ? linked.getFileName() : juce::String ("Untitled");
    sessionFileLabel.locationText = where;
    sessionFileLabel.stateText    = state;
    sessionFileLabel.dirty      = dirty;
    sessionFileLabel.missing    = missing;
    sessionFileLabel.repaint();

    sessionWarningBar.messageLabel.setText (
        "CSD file not found or empty: " + audioProcessor.getLinkedCsdDisplayPath()
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
        juce::Component::SafePointer<CsoundAudioProcessorEditor> safeThis (this);

        showNativeTwoButtonAlertAsync (
            "File changed on disk",
            linked.getFileName() + " has been modified outside this session since it was loaded. "
            "Overwrite it with the current session?",
            "Cancel", "Overwrite",
            [safeThis, linked, onSaved] (int choice)
        {
            if (safeThis != nullptr && choice == 2)
                safeThis->writeLinkedSessionFile (linked, onSaved);
        }, this);
        return;
    }

    writeLinkedSessionFile (linked, onSaved);
}

void CsoundAudioProcessorEditor::writeLinkedSessionFile (const juce::File& linked, std::function<void()> onSaved)
{
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
        document.replaceAllContent (audioProcessor.getEditorDraft()); // bozza ripristinata, se c'era
    }

    document.clearUndoHistory();
    sharedUndoManager.clearUndoHistory();
    audioProcessor.consumeDocumentResyncRequest(); // gia' risincronizzato qui

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

void CsoundAudioProcessorEditor::sessionEditedByUndoRedo (bool)
{
    updateApplyButtonDirtyState();
    updateSessionStatus();
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

void CsoundAudioProcessorEditor::OpcodeHelpBar::setHelpText (const juce::String& syntax, const juce::String& description, const juce::String& category)
{
    syntaxText = syntax;
    descriptionText = description;
    categoryText = category;
    repaint();
}

CsoundAudioProcessorEditor::SidebarDivider::SidebarDivider()
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

void CsoundAudioProcessorEditor::SidebarDivider::paint (juce::Graphics& g)
{
    // Solo la striscia centrale (sidebarDividerWidth) e' disegnata: i
    // margini di presa ai lati restano trasparenti (vedi dividerGrabMargin).
    const auto strip = getLocalBounds().withSizeKeepingCentre (sidebarDividerWidth, getHeight());
    g.setColour (juce::Colour (0xffd7dee3));
    g.fillRect (strip);

    // Segno di trascinamento: pillola verticale al centro.
    g.setColour (juce::Colour (0xff9aa6b1));
    g.fillRoundedRectangle (strip.toFloat().withSizeKeepingCentre (4.0f, 36.0f), 2.0f);
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
    const auto strip = getLocalBounds().withSizeKeepingCentre (getWidth(), consoleDividerHeight);
    g.setColour (juce::Colour (0xffd7dee3));
    g.fillRect (strip);

    g.setColour (juce::Colour (0xff9aa6b1));
    g.fillRoundedRectangle (strip.toFloat().withSizeKeepingCentre (36.0f, 4.0f), 2.0f);
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

    // Categoria del manuale come etichetta a destra (es. "Signal
    // Generators: Basic Oscillators"), come nell'indice di CsoundQt. Solo
    // se c'e' spazio: la descrizione ha la precedenza.
    if (categoryText.isNotEmpty())
    {
        const auto tagFont = juce::Font (juce::FontOptions (11.0f));
        const auto tagText = categoryText.replace (":", ": ");

        juce::GlyphArrangement tagGlyphs;
        tagGlyphs.addLineOfText (tagFont, tagText, 0.0f, 0.0f);
        const int tagWidth = (int) std::ceil (tagGlyphs.getBoundingBox (0, -1, true).getWidth()) + 12;

        if (tagWidth < area.getWidth() / 2)
        {
            auto tagArea = area.removeFromRight (tagWidth).reduced (0, 5);
            g.setColour (juce::Colour (0xffe9dfbf));
            g.fillRoundedRectangle (tagArea.toFloat(), 4.0f);
            g.setColour (juce::Colour (0xff6b5b3a));
            g.setFont (tagFont);
            g.drawText (tagText, tagArea, juce::Justification::centred, true);
            area.removeFromRight (8);
        }
    }

    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.setColour (juce::Colour (0xff3a3a3a));
    g.drawFittedText (descriptionText, area, juce::Justification::centredLeft, 1);
}

void CsoundAudioProcessorEditor::csoundMessageReceived (const juce::String& message)
{
    appendToLog (message);
}

void CsoundAudioProcessorEditor::csoundMessagesReceived (const juce::StringArray& messages)
{
    if (! messages.isEmpty())
        appendToLog (messages.joinIntoString ("\n"));
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

//==============================================================================
// Vista "About" - vedi il commento su AboutView in PluginEditor.h.
namespace
{
    const juce::Colour kAboutCardBg     { 0xff141d24 }; // stesso fondo del menu (CsoundActionSheet)
    const juce::Colour kAboutBorder     { 0xff2a3540 };
    const juce::Colour kAboutText       { 0xffe8eef1 };
    const juce::Colour kAboutTextMuted  { 0xff8a9aa5 };
    const juce::Colour kAboutAccent     { 0xff4aa3b8 };
    const juce::Colour kAboutRowBg      { 0xff1c2730 };
    const juce::Colour kAboutRowHover   { 0xff243441 };

    // Caratteri non ASCII via codepoint: un letterale UTF-8 in
    // juce::String(const char*) fa scattare un jassert.
    juce::String uc (juce::juce_wchar c) { return juce::String::charToString (c); }
}

CsoundAudioProcessorEditor::AboutView::LinkRow::LinkRow (juce::String titleText, juce::String subtitleText, juce::URL target)
    : title (std::move (titleText)), subtitle (std::move (subtitleText)), url (std::move (target))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void CsoundAudioProcessorEditor::AboutView::LinkRow::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour (hovered ? kAboutRowHover : kAboutRowBg);
    g.fillRoundedRectangle (b, 10.0f);

    if (hovered)
    {
        g.setColour (kAboutAccent.withAlpha (0.55f));
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
    }

    auto content = getLocalBounds().reduced (16, 9);

    // Freccia "esterno" a destra: dice che il link apre il browser.
    g.setColour (hovered ? kAboutAccent : kAboutTextMuted);
    g.setFont (juce::Font (juce::FontOptions (16.0f)));
    g.drawFittedText (uc (0x2197), content.removeFromRight (20), juce::Justification::centredRight, 1);

    g.setColour (kAboutText);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawFittedText (title, content.removeFromTop (content.getHeight() / 2), juce::Justification::bottomLeft, 1);

    g.setColour (kAboutTextMuted);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawFittedText (subtitle, content, juce::Justification::topLeft, 1);
}

void CsoundAudioProcessorEditor::AboutView::LinkRow::mouseUp (const juce::MouseEvent& e)
{
    if (contains (e.getPosition()))
        url.launchInDefaultBrowser();
}

CsoundAudioProcessorEditor::AboutView::Card::Card()
    : githubRow ("Source code on GitHub", "github.com/alessandropetrolati/apeCsound",
                 juce::URL ("https://github.com/alessandropetrolati/apeCsound")),
      websiteRow ("apeSoft website", "www.apesoft.it",
                  juce::URL ("https://www.apesoft.it"))
{
    icon = juce::ImageCache::getFromMemory (BinaryData::csicon_png, BinaryData::csicon_pngSize);
    addAndMakeVisible (githubRow);
    addAndMakeVisible (websiteRow);
}

juce::Rectangle<int> CsoundAudioProcessorEditor::AboutView::Card::closeButtonBounds() const
{
    return { getWidth() - 16 - 28, 16, 28, 28 };
}

void CsoundAudioProcessorEditor::AboutView::Card::resized()
{
    githubRow.setBounds  (24, 270, getWidth() - 48, 56);
    websiteRow.setBounds (24, 336, getWidth() - 48, 56);
}

void CsoundAudioProcessorEditor::AboutView::Card::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float radius = 16.0f;
    const int w = getWidth();

    // Fondo + leggero alone accentato in alto + bordo sottile.
    g.setColour (kAboutCardBg);
    g.fillRoundedRectangle (b, radius);

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (b, radius);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (kAboutAccent.withAlpha (0.20f), b.getCentreX(), 0.0f,
                                                 kAboutAccent.withAlpha (0.0f), b.getCentreX(), 180.0f, false));
        g.fillRect (b.withHeight (180.0f));
    }

    g.setColour (kAboutBorder);
    g.drawRoundedRectangle (b.reduced (0.5f), radius, 1.0f);

    // Chiudi (x) in alto a destra.
    {
        const auto cb = closeButtonBounds().toFloat();
        g.setColour (kAboutRowBg);
        g.fillEllipse (cb);
        g.setColour (kAboutTextMuted);
        const float m = cb.getWidth() * 0.34f;
        g.drawLine (cb.getX() + m, cb.getY() + m, cb.getRight() - m, cb.getBottom() - m, 1.6f);
        g.drawLine (cb.getRight() - m, cb.getY() + m, cb.getX() + m, cb.getBottom() - m, 1.6f);
    }

    // Icona dell'app, angoli arrotondati.
    {
        const juce::Rectangle<float> iconArea ((float) (w - 72) * 0.5f, 28.0f, 72.0f, 72.0f);
        if (icon.isValid())
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path p;
            p.addRoundedRectangle (iconArea, 16.0f);
            g.reduceClipRegion (p);
            g.drawImage (icon, iconArea, juce::RectanglePlacement::centred);
        }
    }

    // Nome + versione (da ProjectInfo, cosi' restano sempre allineati al progetto).
    g.setColour (kAboutText);
    g.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
    g.drawFittedText (ProjectInfo::projectName, 0, 112, w, 30, juce::Justification::centred, 1);

    g.setColour (kAboutTextMuted);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawFittedText ("Version " + juce::String (ProjectInfo::versionString), 0, 142, w, 18, juce::Justification::centred, 1);

    // Separatore.
    g.setColour (kAboutBorder);
    g.fillRect (40, 178, w - 80, 1);

    // Autore.
    g.setColour (kAboutTextMuted);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawFittedText ("DEVELOPED BY", 0, 194, w, 14, juce::Justification::centred, 1);

    g.setColour (kAboutText);
    g.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
    g.drawFittedText ("Alessandro Petrolati", 0, 211, w, 22, juce::Justification::centred, 1);

    g.setColour (kAboutAccent);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawFittedText ("apeSoft", 0, 234, w, 20, juce::Justification::centred, 1);

    // Piede.
    g.setColour (kAboutTextMuted);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawFittedText ("Powered by Csound  " + uc (0x00B7) + "  Built with JUCE", 0, 398, w, 16, juce::Justification::centred, 1);
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawFittedText ("Opcode help text from the Csound Reference Manual\n(GNU FDL), via CsoundQt's opcode database",
                      0, 425, w, 14, juce::Justification::centred, 1);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawFittedText (uc (0x00A9) + " " + juce::String (juce::Time::getCurrentTime().getYear()) + " apeSoft",
                      0, 450, w, 16, juce::Justification::centred, 1);
}

void CsoundAudioProcessorEditor::AboutView::Card::mouseUp (const juce::MouseEvent& e)
{
    if (closeButtonBounds().contains (e.getPosition()) && onClose)
        onClose();
}

CsoundAudioProcessorEditor::AboutView::AboutView()
{
    setWantsKeyboardFocus (true);
    card.onClose = [this] { hide(); };
    addAndMakeVisible (card);
}

void CsoundAudioProcessorEditor::AboutView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.45f));
}

void CsoundAudioProcessorEditor::AboutView::resized()
{
    // Scheda a dimensioni fisse, centrata; se la finestra e' piu' piccola
    // viene SCALATA in proporzione (mai tagliata).
    const auto bounds = getLocalBounds();
    const auto cardArea = juce::Rectangle<int> (cardWidth, cardHeight).withCentre (bounds.getCentre());
    card.setBounds (cardArea);

    const float scale = juce::jmin (1.0f,
                                    (float) (bounds.getWidth()  - 24) / (float) cardWidth,
                                    (float) (bounds.getHeight() - 24) / (float) cardHeight);

    card.setTransform (scale < 1.0f
        ? juce::AffineTransform::scale (juce::jmax (0.1f, scale), juce::jmax (0.1f, scale),
                                        (float) cardArea.getCentreX(), (float) cardArea.getCentreY())
        : juce::AffineTransform());
}

void CsoundAudioProcessorEditor::AboutView::mouseUp (const juce::MouseEvent& e)
{
    // Raggiunto solo per i clic sul velo (la scheda intercetta i propri).
    if (e.eventComponent == this)
        hide();
}

bool CsoundAudioProcessorEditor::AboutView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }

    return false;
}

void CsoundAudioProcessorEditor::AboutView::show()
{
    toFront (true);
    juce::Desktop::getInstance().getAnimator().fadeIn (this, 140);
    grabKeyboardFocus();
}

void CsoundAudioProcessorEditor::AboutView::hide()
{
    juce::Desktop::getInstance().getAnimator().fadeOut (this, 120);
}

//==============================================================================
// Guida rapida - vedi GuideView in PluginEditor.h.
CsoundAudioProcessorEditor::GuideView::Card::Card()
{
    text.setMultiLine (true);
    text.setReadOnly (true);
    text.setScrollbarsShown (true);
    text.setCaretVisible (false);
    text.setPopupMenuEnabled (false);
    text.setColour (juce::TextEditor::backgroundColourId, kAboutCardBg);
    text.setColour (juce::TextEditor::textColourId, kAboutText);
    text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    text.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    text.setColour (juce::TextEditor::highlightColourId, kAboutAccent.withAlpha (0.35f));
    text.setLineSpacing (1.3f);
    text.setWantsKeyboardFocus (false);
    addAndMakeVisible (text);

    // Mini-markup di guideText(): "# Titolo" sezione, "## Titolo"
    // sottosezione, "**grassetto**", `codice` (monospace, accento),
    // "- " elenco puntato. Reso con le run di stile di juce::TextEditor
    // (font/colore impostati prima di ogni insertTextAtCaret): niente
    // WebView, che in un plugin e' fragile e pesante.
    const auto bodyFont  = juce::Font (juce::FontOptions (13.5f));
    const auto boldFont  = juce::Font (juce::FontOptions (13.5f, juce::Font::bold));
    const auto h1Font    = juce::Font (juce::FontOptions (17.0f, juce::Font::bold));
    const auto h2Font    = juce::Font (juce::FontOptions (14.0f, juce::Font::bold));
    const auto codeFont  = juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain));

    auto append = [this] (const juce::String& run, const juce::Font& font, juce::Colour colour)
    {
        text.setFont (font);
        text.setColour (juce::TextEditor::textColourId, colour);
        text.insertTextAtCaret (run);
    };

    // Testo inline: **bold** e `code` dentro una riga normale.
    auto appendInline = [&] (const juce::String& line, const juce::Font& font, juce::Colour colour)
    {
        int i = 0;

        while (i < line.length())
        {
            const int b = line.indexOf (i, "**");
            const int c = line.indexOfChar (i, '`');
            int next = -1;
            bool isBold = false;

            if (b >= 0 && (c < 0 || b < c)) { next = b; isBold = true; }
            else if (c >= 0)                { next = c; }

            if (next < 0)
            {
                append (line.substring (i), font, colour);
                break;
            }

            if (next > i)
                append (line.substring (i, next), font, colour);

            if (isBold)
            {
                const int end = line.indexOf (next + 2, "**");
                if (end < 0) { append (line.substring (next), font, colour); break; }
                append (line.substring (next + 2, end), boldFont, kAboutText);
                i = end + 2;
            }
            else
            {
                const int end = line.indexOfChar (next + 1, '`');
                if (end < 0) { append (line.substring (next), font, colour); break; }
                append (line.substring (next + 1, end), codeFont, kAboutAccent);
                i = end + 1;
            }
        }
    };

    juce::StringArray lines;
    lines.addLines (guideText());

    for (const auto& raw : lines)
    {
        if (raw.startsWith ("## "))
        {
            append (raw.substring (3) + "\n", h2Font, kAboutText);
        }
        else if (raw.startsWith ("# "))
        {
            append ("\n" + raw.substring (2).toUpperCase() + "\n", h1Font, kAboutAccent);
        }
        else if (raw.startsWith ("- "))
        {
            append (juce::String::charToString (0x2022) + "  ", bodyFont, kAboutAccent);
            appendInline (raw.substring (2), bodyFont, kAboutText);
            append ("\n", bodyFont, kAboutText);
        }
        else
        {
            appendInline (raw, bodyFont, raw.isEmpty() ? kAboutText : kAboutTextMuted.brighter (0.35f));
            append ("\n", bodyFont, kAboutText);
        }
    }

    text.moveCaretToTop (false);
}

void CsoundAudioProcessorEditor::GuideView::Card::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (kAboutCardBg);
    g.fillRoundedRectangle (bounds, 16.0f);
    g.setColour (kAboutBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 15.5f, 1.0f);

    g.setColour (kAboutText);
    g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    g.drawFittedText ("apeCsound Guide", 24, 16, getWidth() - 48 - 36, 28, juce::Justification::centredLeft, 1);

    g.setColour (kAboutBorder);
    g.drawLine (16.0f, 54.5f, (float) getWidth() - 16.0f, 54.5f, 1.0f);

    // x di chiusura.
    const auto close = closeButtonBounds().toFloat();
    g.setColour (kAboutTextMuted);
    juce::Path x;
    x.startNewSubPath (close.getX() + 9.0f, close.getY() + 9.0f); x.lineTo (close.getRight() - 9.0f, close.getBottom() - 9.0f);
    x.startNewSubPath (close.getRight() - 9.0f, close.getY() + 9.0f); x.lineTo (close.getX() + 9.0f, close.getBottom() - 9.0f);
    g.strokePath (x, juce::PathStrokeType (1.8f));
}

juce::Rectangle<int> CsoundAudioProcessorEditor::GuideView::Card::closeButtonBounds() const
{
    return { getWidth() - 16 - 28, 14, 28, 28 };
}

void CsoundAudioProcessorEditor::GuideView::Card::resized()
{
    text.setBounds (getLocalBounds().withTrimmedTop (62).reduced (16, 0).withTrimmedBottom (16));
}

void CsoundAudioProcessorEditor::GuideView::Card::mouseUp (const juce::MouseEvent& e)
{
    if (closeButtonBounds().contains (e.getPosition()) && onClose)
        onClose();
}

CsoundAudioProcessorEditor::GuideView::GuideView()
{
    setWantsKeyboardFocus (true);
    card.onClose = [this] { hide(); };
    addAndMakeVisible (card);
}

void CsoundAudioProcessorEditor::GuideView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.45f));
}

void CsoundAudioProcessorEditor::GuideView::resized()
{
    // Scheda grande quanto la finestra meno un margine (il testo scorre).
    card.setBounds (getLocalBounds().reduced (juce::jmin (40, getWidth() / 10), juce::jmin (32, getHeight() / 10)));
}

void CsoundAudioProcessorEditor::GuideView::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent == this)
        hide();
}

bool CsoundAudioProcessorEditor::GuideView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }

    return false;
}

void CsoundAudioProcessorEditor::GuideView::show()
{
    toFront (true);
    juce::Desktop::getInstance().getAnimator().fadeIn (this, 140);
    grabKeyboardFocus();
}

void CsoundAudioProcessorEditor::GuideView::hide()
{
    juce::Desktop::getInstance().getAnimator().fadeOut (this, 120);
}

juce::String CsoundAudioProcessorEditor::GuideView::guideText()
{
    return R"GUIDE(**apeCsound** runs Csound 7 code live inside the DAW. It is a **multichannel effect and instrument at the same time**: the track audio reaches the code through `inch`, MIDI notes through `massign` / `notnum`, and whatever the code writes with `outs` / `outch` goes back to the track, on as many channels as the track has. Write or load a `.csd` in the editor, press **Apply**, and the engine recompiles in place while the track keeps playing.
Every instance exposes a **fixed set of 144 parameters** to the DAW: **64 Float**, **32 Int**, **32 Bool** and **16 Choice** (up to 16 options each). Their indices are fixed - the DAW always sees "Float 1 ... Float 64" and so on - but each one can be configured from the Parameters panel: Csound channel name, range, default, skew, step or options. This is what makes them automatable and recallable like any other plugin parameter.

# Toolbar
- **Apply** - recompiles and restarts Csound with the text in the editor. A red ring means the editor text differs from the running code.
- **File capsule** (centre) - name of the linked `.csd`, its folder and state (saved / unsaved changes / file not found, coloured dot). Click it for **Save**, **Save as...**, **Load...**, **Init Session Template** and **Init Session Clear**.
- **Magnifier** - opens the Find / Replace bar: match case `Aa`, whole word `W`, Replace / Replace All; Enter = next match, Esc closes.
- **Menu** - Show/Hide Parameters and Console, Undo / Redo, Config, apeCsound Guide, Csound Manual, About.

# Session and files
The `.csd` file on disk is the truth. A DAW project stores the path of the linked file, an embedded copy of the code and the parameter mapping, plus the current parameter values.
- **Save** - writes the editor text and the parameter structure (an `<apeCsoundParams>` block after `</CsoundSynthesizer>`) to the linked file.
- **Save as...** - chooses a new file (default folder `~/Documents/apeCsound`) and links it.
- **Load...** - opens a `.csd`; you can also drop a `.csd` on the window. Unsaved changes are protected by a dialog.
- **Init Session Template** - unlinks the file and loads the built-in template (synth + delay with its parameters).
- **Init Session Clear** - unlinks the file and starts from an empty `.csd` (header only, no parameters).
- If the linked file is missing when a project reopens, the embedded copy is used and a yellow bar offers **Relocate...** / **Save As...**.
- The editor shows only the `<CsoundSynthesizer>` block; anything before or after it (Cabbage, CsoundQt panels) is preserved.

# Code editor
- **Auto-indent** for `instr`/`endin`, `opcode`/`endop`, `if`/`then`, `while`/`do`, `until`, loops. Tab = 4 spaces.
- **Right-click** (long press on touch) - Cut, Copy, Paste, Delete, Select All, Undo, Redo, **Indent** (re-indents the selected lines) and **Comment / Uncomment** (toggles `;` at the start of the lines).
- **Opcode help bar** - syntax, description and manual category of the opcode under the caret, for every opcode of the Csound Reference Manual (text from CsoundQt's opcode database, GNU FDL), for the user-defined opcodes of the current file (`opcode Name, outs, ins`) and, for anything else, the type signature reported by the engine.
- **Autocompletion** - type two letters and suggestions appear: Up/Down to choose, Tab or Enter to accept, Esc opens/closes the list.
- **Config > Modern Syntax in Help** - every synopsis in Csound 7 functional form with type annotations, e.g. `ares:a = oscil(xamp, xcps)`.
- **Undo / Redo** - Cmd+Z, Cmd+Shift+Z (or Cmd+Y) and the menu share one linear history between the editor and the Parameters panel; it survives closing and reopening the plugin window.

# Parameters panel
The 144 fixed parameters (64 Float, 32 Int, 32 Bool, 16 Choice) are mapped here to Csound channels read with `chnget`: **Float** (min/max/default/skew/step), **Int** (min/max/default), **Bool** (default), **Choice** (up to 16 options, sends the index). Use **+** on the panel title bar to add one; each row has edit, copy-`chnget` and remove buttons. Unassigned slots are hidden from the panel but still exist for the DAW. Values are pushed to Csound every block with `csoundSetControlChannel`.
- **Drag a parameter into the code** - drag the handle on the left of a row and drop it on a line of the editor: a `chnget` line is inserted there, already indented, with the variable named after the channel (`kCutoff chnget "Cutoff"`) and, above it, a comment with the parameter configuration (`; SLIDER FLOAT: Min=20; Max=20000; Skew=0.3; Step=1`). The **copy** button puts the same text in the clipboard, for pasting by hand.
- Files made with **Cabbage** (`<Cabbage>` section) or **CsoundQt** (`<bsbPanel>` widgets: sliders, knobs, spin boxes, scroll numbers, XY controllers, checkboxes, dropdowns, value buttons) are imported automatically on load: widgets become parameters. CsoundQt `exp` sliders get a matching skew, integer knobs step 1, checkbox `pressedValue` is respected.

# Audio, MIDI and channels
- `sr` always follows the DAW sample rate.
- `ksmps` is respected and equals the plugin latency (reported to the DAW, which compensates it): keep it small (16-64) for live input.
- **Config > Follow CSD nchnls** (default ON) - Csound runs with the `nchnls` / `nchnls_i` declared in the `.csd` header, so the code behaves the same on any track: `outch n` is channel n of the track, channels missing on either side are silent. If the header does not declare them, the track count is used (minimum 2 outputs). Disabled: Csound always follows the track. Mismatches are reported in the console.
- **Track layouts** - the track can have 1 to 16 output channels and 0 to 16 input channels (0 = input bus disabled). Surround layouts (5.1, 7.1...) are compensated so that channel numbers match the track. Changing the track channel count recompiles the engine.
- **MIDI** - notes and controllers from the track reach Csound (`-M0`, `massign`); MIDI generated by Csound (`midiout`) goes back to the DAW.

# Console
Csound messages, compile errors and plugin notes (latency, channels, imports). Identical messages repeated more than three times per second are summarised to keep the UI responsive. **Clear** empties it; it is also cleared on every Apply.

# Config (main menu)
- **Modern Syntax in Help** - functional syntax in the opcode help bar (user preference).
- **Follow CSD nchnls** - see "Audio, MIDI and channels" (saved with the project).

# Shortcuts
- **Cmd+Z / Cmd+Shift+Z (Cmd+Y)** - undo / redo
- **Esc** - autocompletion list, close the Find bar or an overlay
- **Tab / Enter** - accept a suggestion
- **Right-click or long press** - editor menu
- No Cmd+S on purpose: it would collide with the DAW. Use the file capsule or the menu.
)GUIDE";
}

//==============================================================================
// Cerca / Sostituisci - vedi FindReplaceBar in PluginEditor.h.
namespace
{
    const juce::Colour kFindBarBg      { 0xff10181f }; // come la toolbar
    const juce::Colour kFindBarBorder  { 0xff2a3a44 };
    const juce::Colour kFindFieldBg    { 0xff202a33 };
    const juce::Colour kFindFieldLine  { 0xff3a4550 };
    const juce::Colour kFindText       { 0xffe8eef1 };
    const juce::Colour kFindMuted      { 0xff8a9aa5 };
    const juce::Colour kFindAccent     { 0xff4aa3b8 };

    void styleFindField (juce::TextEditor& field, const juce::String& placeholder)
    {
        field.setMultiLine (false);
        field.setReturnKeyStartsNewLine (false);
        field.setFont (juce::FontOptions (13.0f));
        field.setIndents (8, 6);
        field.setColour (juce::TextEditor::backgroundColourId,     kFindFieldBg);
        field.setColour (juce::TextEditor::textColourId,           kFindText);
        field.setColour (juce::TextEditor::outlineColourId,        kFindFieldLine);
        field.setColour (juce::TextEditor::focusedOutlineColourId, kFindAccent);
        field.setColour (juce::TextEditor::highlightColourId,      kFindAccent.withAlpha (0.35f));
        field.setColour (juce::CaretComponent::caretColourId,      kFindText);
        field.setTextToShowWhenEmpty (placeholder, kFindMuted);
    }

    void styleFindButton (juce::TextButton& button, const juce::String& tooltip, bool toggle = false)
    {
        button.setColour (juce::TextButton::buttonColourId,   kFindFieldBg);
        button.setColour (juce::TextButton::buttonOnColourId, kFindAccent);
        button.setColour (juce::TextButton::textColourOffId,  kFindText);
        button.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff0b1116));
        button.setTooltip (tooltip);
        button.setClickingTogglesState (toggle);
        // Clic sui bottoni senza togliere il focus da tastiera al campo di
        // ricerca (Invio continua a fare "Next").
        button.setWantsKeyboardFocus (false);
        button.setMouseClickGrabsKeyboardFocus (false);
    }
}

CsoundAudioProcessorEditor::FindReplaceBar::FindReplaceBar()
{
    styleFindField (findField, "" /*"Find"*/);
    styleFindField (replaceField, "" /*"Replace with"*/);
    addAndMakeVisible (findField);
    addAndMakeVisible (replaceField);

    countLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    countLabel.setColour (juce::Label::textColourId, kFindMuted);
    countLabel.setJustificationType (juce::Justification::centred);
    countLabel.setMinimumHorizontalScale (0.8f);
    addAndMakeVisible (countLabel);

    prevButton.setButtonText (juce::String::charToString (0x2191)); // freccia su
    nextButton.setButtonText (juce::String::charToString (0x2193)); // freccia giu'
    closeButton.setButtonText (juce::String::charToString (0x00D7)); // x

    styleFindButton (prevButton,       "" /*"Previous match"*/);
    styleFindButton (nextButton,       "" /*"Next match (Return in the Find field)"*/);
    styleFindButton (matchCaseButton,  "Match case: distinguish uppercase and lowercase letters", true);
    styleFindButton (wholeWordButton,  "Whole word: match only complete words (not inside longer names)", true);
    styleFindButton (replaceButton,    "" /*"Replace the current match and go to the next one (Return in the Replace field)"*/);
    styleFindButton (replaceAllButton, "" /*"Replace all matches - one undo step"*/);
    styleFindButton (closeButton,      "" /*"Close Find / Replace (Esc)"*/);

    //findField.setTooltip ("Text to find - matches are highlighted in the code editor");
    //replaceField.setTooltip ("Replacement text");
    //countLabel.setTooltip ("Current match / total matches");

    for (auto* b : { &prevButton, &nextButton, &matchCaseButton, &wholeWordButton,
                     &replaceButton, &replaceAllButton, &closeButton })
        addAndMakeVisible (b);
}

void CsoundAudioProcessorEditor::FindReplaceBar::paint (juce::Graphics& g)
{
    g.fillAll (kFindBarBg);
    g.setColour (kFindBarBorder);
    g.drawLine (0.0f, (float) getHeight() - 0.5f, (float) getWidth(), (float) getHeight() - 0.5f, 1.0f);
}

void CsoundAudioProcessorEditor::FindReplaceBar::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    const int h = area.getHeight();

    // Fissi a destra e tra i due campi; i due campi si dividono il resto.
    closeButton.setBounds (area.removeFromRight (h));
    area.removeFromRight (10);
    replaceAllButton.setBounds (area.removeFromRight (44));
    area.removeFromRight (4);
    replaceButton.setBounds (area.removeFromRight (74));
    area.removeFromRight (6);

    const int fixedFindSide = 70 + 4 + h + 2 + h + 8 + 36 + 2 + 32 + 16;
    const int fieldWidth = juce::jmax (80, (area.getWidth() - fixedFindSide) / 2);

    findField.setBounds (area.removeFromLeft (fieldWidth));
    area.removeFromLeft (4);
    countLabel.setBounds (area.removeFromLeft (70));
    area.removeFromLeft (4);
    prevButton.setBounds (area.removeFromLeft (h));
    area.removeFromLeft (2);
    nextButton.setBounds (area.removeFromLeft (h));
    area.removeFromLeft (8);
    matchCaseButton.setBounds (area.removeFromLeft (36));
    area.removeFromLeft (2);
    wholeWordButton.setBounds (area.removeFromLeft (32));
    area.removeFromLeft (16);
    replaceField.setBounds (area);
}

void CsoundAudioProcessorEditor::toggleFindBar()
{
    if (findBar.isVisible())
        closeFindBar();
    else
        openFindBar();
}

void CsoundAudioProcessorEditor::openFindBar()
{
    // Testo selezionato nell'editor (su una sola riga) come query iniziale,
    // come in qualunque editor di codice.
    const auto selection = editor.getHighlightedRegion();
    if (! selection.isEmpty())
    {
        const auto selected = document.getTextBetween (juce::CodeDocument::Position (document, selection.getStart()),
                                                       juce::CodeDocument::Position (document, selection.getEnd()));
        if (! selected.containsChar ('\n'))
            findBar.findField.setText (selected, juce::dontSendNotification);
    }

    findBar.setVisible (true);
    findButton.setToggleState (true, juce::dontSendNotification);
    resized();

    findBar.findField.grabKeyboardFocus();
    findBar.findField.selectAll();
    updateFindQuery();
}

void CsoundAudioProcessorEditor::closeFindBar()
{
    findBar.setVisible (false);
    findButton.setToggleState (false, juce::dontSendNotification);
    editor.clearSearch();
    resized();
   #if ! JUCE_IOS
    editor.grabKeyboardFocus(); // su iOS farebbe comparire la tastiera a schermo
   #endif
}

void CsoundAudioProcessorEditor::updateFindQuery()
{
    editor.setSearchQuery (findBar.findField.getText(),
                           findBar.matchCaseButton.getToggleState(),
                           findBar.wholeWordButton.getToggleState());
    updateFindCount();
}

void CsoundAudioProcessorEditor::updateFindCount()
{
    const int count = editor.getNumSearchMatches();
    const bool hasQuery = findBar.findField.getText().isNotEmpty();

    juce::String text;
    if (hasQuery)
        text = count == 0 ? juce::String ("No results")
                          : juce::String (editor.getCurrentSearchMatchIndex() + 1) + " of " + juce::String (count);

    findBar.countLabel.setText (text, juce::dontSendNotification);
    findBar.countLabel.setColour (juce::Label::textColourId,
                                  hasQuery && count == 0 ? juce::Colour (0xffff6b6b) : kFindMuted);

    for (auto* b : { &findBar.prevButton, &findBar.nextButton,
                     &findBar.replaceButton, &findBar.replaceAllButton })
        b->setEnabled (count > 0);
}
