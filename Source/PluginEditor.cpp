#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "NativeAlertMac.h"
#include "CsoundShortcuts.h"

// Standalone (macOS e iOS): accesso a StandalonePluginHolder (dialogo
// Audio/MIDI Settings, AudioDeviceSelectorComponent di JUCE) - vedi
// onAudioSettingsRequested e, solo desktop, parentHierarchyChanged().
#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif
#include <cmath>
#include <map>

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
        CodeEditTransactionProxy (juce::CodeDocument& documentIn, bool& guardFlagIn,
                                  std::function<void()> onAppliedIn)
            : document (documentIn), guardFlag (guardFlagIn), onApplied (std::move (onAppliedIn)) {}

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

                // Caret e selezione sul punto rifatto (vedi CodeView::revealLastEdit).
                if (onApplied)
                    onApplied();
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

            // Caret e selezione sul punto annullato (vedi CodeView::revealLastEdit).
            if (onApplied)
                onApplied();

            return true;
        }

        juce::CodeDocument& document;
        bool& guardFlag;
        std::function<void()> onApplied;
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

    // Preferenze dell'UTENTE (non del progetto): poche opzioni di
    // visualizzazione valide per ogni istanza del plugin, es. la sintassi
    // moderna nella barra di help. ~/Library/Application Support/apeCsound/
    // apeCsound.settings su macOS. Niente di cio' che riguarda la sessione
    // (path, codice, parametri) va qui: quello sta nello stato del progetto.
    juce::PropertiesFile& getUserSettings()
    {
        // Un solo PropertiesFile per processo, condiviso con il processor
        // (che ci legge le cartelle SSDIR/SFDIR scelte in Config).
        return CsoundAudioProcessor::getUserSettings();
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


    editor.onEvaluateRequested = [this] (const juce::String& code) { audioProcessor.evaluateCode (code); };

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

    opcodeHelpBar.input.setMultiLine (false);
    opcodeHelpBar.input.setReturnKeyStartsNewLine (false);
    opcodeHelpBar.input.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    opcodeHelpBar.input.setIndents (6, 4);
    opcodeHelpBar.input.setColour (juce::TextEditor::backgroundColourId,     juce::Colours::transparentBlack); // il placeholder lo disegna la barra sotto (resta visibile anche col focus)
    opcodeHelpBar.input.setColour (juce::TextEditor::textColourId,           juce::Colour (0xffe6f1f3));
    opcodeHelpBar.input.setColour (juce::TextEditor::outlineColourId,        juce::Colours::transparentBlack);
    opcodeHelpBar.input.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    opcodeHelpBar.input.setColour (juce::TextEditor::highlightColourId,      juce::Colour (0xff2ec4b6).withAlpha (0.35f));
    opcodeHelpBar.input.setColour (juce::CaretComponent::caretColourId,      juce::Colour (0xffe6f1f3));
    opcodeHelpBar.input.onTextChange = [this] { opcodeHelpBar.repaint(); }; // placeholder via/compare
    opcodeHelpBar.input.onReturnKey = [this] { submitConsoleInput(); };
    // Esc: la barra torna all'help e il focus all'editor di codice.
    opcodeHelpBar.input.onEscapeKey = [this] { setEvaluateLineVisible (false); };
    opcodeHelpBar.onEvaluateModeChanged = [this] (bool on) { setEvaluateLineVisible (on); };
    opcodeHelpBar.input.onKey = [this] (const juce::KeyPress& key)
    {
        const int n = consoleInputHistory.size();
        if (n == 0 || (key.getKeyCode() != juce::KeyPress::upKey && key.getKeyCode() != juce::KeyPress::downKey))
            return false;

        consoleInputHistoryPos = juce::jlimit (0, n, consoleInputHistoryPos + (key.getKeyCode() == juce::KeyPress::upKey ? -1 : 1));
        opcodeHelpBar.input.setText (consoleInputHistoryPos < n ? consoleInputHistory[consoleInputHistoryPos] : juce::String(), false);
        opcodeHelpBar.input.moveCaretToEnd();
        return true;
    };

    // Codice <-> pannello Parameters: caret su chnget "x" -> la card "x" si
    // illumina; tocco sul nome nella card -> l'editor seleziona il "x" del
    // primo chnget (o della prima occorrenza) e lo porta in vista.
    editor.onCaretChannelChanged = [this] (const juce::String& name) { parameterPanel.setHighlightedChannel (name); };
    parameterPanel.onChannelNameClicked = [this] (const juce::String& name)
    {
        const auto text = document.getAllContent();
        const auto quoted = "\"" + name + "\"";
        int best = -1;

        for (int at = text.indexOf (quoted); at >= 0; at = text.indexOf (at + 1, quoted))
        {
            if (best < 0) best = at;
            const int lineStart = text.substring (0, at).lastIndexOfChar ('\n') + 1;
            const auto lineText = text.substring (lineStart, at);
            if (lineText.contains ("chnget")) { best = at; break; }
        }

        if (best < 0)
            return;

        editor.setHighlightedRegion ({ best + 1, best + 1 + name.length() });
        editor.scrollToKeepCaretOnScreen();
    };

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
    addChildComponent (audioSettingsView);

    // Browser interno dei .csd (solo iOS, vedi CsdBrowserView.h).
    csdBrowser.onLog = [this] (const juce::String& line) { appendToLog (line); };
    addChildComponent (csdBrowser);

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

    // Esc chiude la barra Cerca/Sostituisci anche con il focus nell'editor
    // di codice (dove altrimenti aprirebbe l'autocompletamento).
    editor.onEscapeKey = [this]
    {
        if (! findBar.isVisible())
            return false;

        closeFindBar();
        return true;
    };
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

    pauseButton.setName ("pause");
    pauseButton.getProperties().set ("circular", true);
    pauseButton.onClick = [this]
    {
        audioProcessor.setCsoundPaused (! audioProcessor.isCsoundPaused());
        updatePauseButton();
    };
    addAndMakeVisible (pauseButton);
    updatePauseButton();

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
    clearConsoleButton.onClick = [this] { logConsole.clear(); consoleLineCount = 0; };
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

    // Esempi di fabbrica in apeCsound/CSD/Examples al primo avvio (o quando
    // cambia la versione dello zip), senza sovrascrivere nulla. Differito: non
    // rallenta l'apertura della finestra.
    juce::MessageManager::callAsync ([] { CsoundAudioProcessor::ensureExamplesInstalled(); });

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

    // Pannello Graphs (nascosto di default) e divisore verticale tra
    // consolle e grafici - vedi showingGraphs in PluginEditor.h.
    addChildComponent (graphsPanel);
    addChildComponent (graphsDivider);
    graphsDivider.getCurrentWidth = [this] { return graphsWidth; };
    graphsDivider.onDrag = [this] (int newWidth)
    {
        graphsWidth = newWidth;
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
    parameterPanel.onAboutRequested       = [this]
    {
        const int numOpcodes = audioProcessor.getOpcodeSignatures().size();
        aboutView.card.engineLine1 = "Csound " + CsoundAudioProcessor::getCsoundVersionString()
                                   + (numOpcodes > 0 ? " - " + juce::String (numOpcodes) + " opcodes" : juce::String());
        aboutView.card.engineLine2 = audioProcessor.getCsoundBuildInfo();
        aboutView.show();
    };
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

    // Config > Extra Folders, tutte le piattaforme. iOS: la cartella scelta
    // col picker (iCloud Drive, "Sul mio iPad"...) viene salvata con il suo
    // bookmark security-scoped e aperta a ogni compilazione (vedi
    // CsoundAudioProcessor::openUserFolder); con SFDIR li', i file scritti da
    // Csound si vedono direttamente nell'app File.
    parameterPanel.onChooseCsoundFolder = [this] (int which) { chooseCsoundFolder (which); };
   #if ! JUCE_IOS
    // Reveal: non su iOS (non c'e' un Finder; la voce non viene mostrata).
    parameterPanel.onRevealCsoundFolder = [] (int which)
    {
        // La cartella in uso: quella scelta, altrimenti quella di apeCsound.
        auto folder = CsoundAudioProcessor::getUserFolderSetting (csoundFolderKey (which));

        if (folder == juce::File{})
            folder = CsoundAudioProcessor::getCsoundFolder (csoundFolderEnvName (which));

        folder.revealToUser();
    };
   #endif
    parameterPanel.onRemoveCsoundFolder = [this] (int which) { setCsoundFolder (which, {}); };
    parameterPanel.isCsoundFolderSet = [] (int which)
    {
        return CsoundAudioProcessor::getUserFolderSetting (csoundFolderKey (which)) != juce::File{};
    };
    parameterPanel.getCsoundFolderPath = [] (int which)
    {
        const auto folder = CsoundAudioProcessor::getUserFolderSetting (csoundFolderKey (which));
       #if JUCE_IOS
        // Path iOS lunghissimi (/private/var/mobile/Library/Mobile Documents/
        // com~apple~CloudDocs/...): solo le ultime due cartelle.
        return ".../" + folder.getParentDirectory().getFileName() + "/" + folder.getFileName();
       #else
        return folder.getFullPathName();
       #endif
    };

   #if JucePlugin_Build_Standalone
    // Standalone: la barra gialla di JUCE "Audio input is muted to avoid
    // feedback loop" (NotificationArea di StandaloneFilterWindow, con il
    // bottone che apre il vecchio dialogo audio) viene sostituita dalla
    // nostra InputMutedBar. MainContentComponent crea QUESTO editor e solo
    // DOPO legge getProcessorHasPotentialFeedbackLoop() per decidere se
    // mostrare la sua barra: azzerando il flag qui la barra JUCE non compare
    // mai. Il "mute" resta attivo e funzionante (lo applica lo holder).
    // z-order 0 (in fondo): la barra occupa solo la sua striscia sotto la
    // toolbar, e cosi' le viste sovrapposte (Audio/MIDI Settings, About,
    // Guide, menu) restano SOPRA anche quando compare mentre sono aperte.
    addChildComponent (inputMutedBar, 0);
    inputMutedBar.onVisibilityChanged = [this] { resized(); };

    if (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            // Valore originale ricordato per tutto il processo: se l'editor
            // viene ricreato il flag dello holder e' gia' stato azzerato.
            static const bool originalFeedbackFlag = holder->getProcessorHasPotentialFeedbackLoop();
            standaloneInputMayFeedback = originalFeedbackFlag;
            holder->processorHasPotentialFeedbackLoop = false;

            if (standaloneInputMayFeedback)
            {
                inputMutedBar.onMessageClicked = [this]
                {
                    if (parameterPanel.onAudioSettingsRequested)
                        parameterPanel.onAudioSettingsRequested();
                };
                inputMutedBar.attach (&holder->getMuteInputValue());
            }
        }
    }

    // Solo nella versione standalone (macOS e iOS): dispositivo audio,
    // sample rate, buffer e ingressi MIDI dal menu principale (su desktop la
    // barra del titolo e' nativa e il bottone "Options" di JUCE non c'e').
    // In AU/VST3/AUv3 il callback resta nullo e la voce non compare.
    if (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        parameterPanel.onAudioSettingsRequested = [this]
        {
            auto* holder = juce::StandalonePluginHolder::getInstance();

            if (holder == nullptr || holder->processor == nullptr)
                return;

            // Stessi limiti di canali di StandalonePluginHolder::showAudioSettingsDialog().
            int maxIns = 0, maxOuts = 0;

            if (holder->channelConfiguration.size() > 0)
            {
                const auto& config = holder->channelConfiguration.getReference (0);
                maxIns  = juce::jmax (0, (int) config.numIns);
                maxOuts = juce::jmax (0, (int) config.numOuts);
            }

            if (auto* bus = holder->processor->getBus (true, 0))
                maxIns = juce::jmax (0, bus->getDefaultLayout().size());

            if (auto* bus = holder->processor->getBus (false, 0))
                maxOuts = juce::jmax (0, bus->getDefaultLayout().size());

            audioSettingsView.show (holder->deviceManager, maxIns, maxOuts,
                                    standaloneInputMayFeedback ? &holder->getMuteInputValue() : nullptr,
                                    []
                                    {
                                        // Salva subito dispositivo/canali/buffer/MIDI (come
                                        // SLSettings::updateAudioSetupSettings): su iOS l'app puo'
                                        // essere chiusa senza passare dal salvataggio in uscita.
                                        if (auto* h = juce::StandalonePluginHolder::getInstance())
                                            h->saveAudioDeviceState();
                                    });
        };
   #endif

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

    // Clic sulla capsula del file: menu File, in gruppi separati nell'ordine
    // classico New / Open / Save, poi i file di supporto e la manutenzione:
    //   1. Init Session Clear, Init Session Template
    //   2. Load..., Open Example...
    //   3. Save, Save as..., Reveal in Finder (desktop)
    //   4. Import Files..., Manage Files...
    //   5. Restore Examples
    // Gli id restano quelli storici (vedi lo switch sotto).
    sessionFileLabel.onClick = [this]
    {
        std::vector<CsoundActionSheetItem> items;

        auto add = [&items] (int id, const juce::String& text, CsoundActionSheetIcon icon,
                             const juce::String& shortcut = {}, bool enabled = true)
        {
            CsoundActionSheetItem item;
            item.id = id; item.text = text; item.icon = icon;
            item.shortcut = shortcut; item.enabled = enabled;
            items.push_back (item);
        };

        // 1. Nuova sessione
        add (5, "Init Session Clear", CsoundActionSheetIcon::newDocument,
             CsoundShortcuts::label (CsoundShortcuts::newSession()));
        add (4, "Init Session Template", CsoundActionSheetIcon::newDocument);

        // 2. Apri (Open Example: chooser aperto in apeCsound/CSD/Examples)
        items.push_back (CsoundActionSheetItem::separator());
        add (3, "Load...", CsoundActionSheetIcon::load,
             CsoundShortcuts::label (CsoundShortcuts::load()));
        add (9, "Open Example...", CsoundActionSheetIcon::book);

        // 3. Salva (Save as: chooser aperto in apeCsound/CSD/User)
        items.push_back (CsoundActionSheetItem::separator());
        add (1, "Save", CsoundActionSheetIcon::save,
             CsoundShortcuts::label (CsoundShortcuts::save()));
        add (2, "Save as...", CsoundActionSheetIcon::save,
             CsoundShortcuts::label (CsoundShortcuts::saveAs()));
       #if ! JUCE_IOS
        // Nel gruppo Save: riguarda il .csd collegato. Mostra il .csd collegato nel file manager di sistema; disabilitata
        // se la sessione non ha un file o il file non esiste piu'.
        {
           #if JUCE_MAC
            const juce::String revealText ("Reveal in Finder");
           #elif JUCE_WINDOWS
            const juce::String revealText ("Show in Explorer");
           #else
            const juce::String revealText ("Show in Folder");
           #endif
            add (6, revealText, CsoundActionSheetIcon::folder, {},
                 audioProcessor.getLinkedCsdFile().existsAsFile());
        }
       #endif

        // 4. File di supporto
        items.push_back (CsoundActionSheetItem::separator());
        // Copia file nelle cartelle di Csound, scelte dall'estensione (audio ->
        // SSDIR, .udo/.inc -> INCDIR, analisi -> SADIR). Su iOS e' l'unico
        // modo (il container dell'App Group non si vede in File).
        add (7, "Import Files...", CsoundActionSheetIcon::waveform);
        // Manage Files...: la cartella apeCsound (CSD, SSDIR, SFDIR, SADIR,
        // INCDIR). iOS: browser interno con Import / Export / Delete (l'app
        // File non la mostra), vedi manageFiles(). Desktop: aperta nel file
        // manager di sistema (Finder / Explorer / xdg-open).
       #if JUCE_IOS
        add (8, "Manage Files...", CsoundActionSheetIcon::folder);
       #else
        add (12, "Manage Files...", CsoundActionSheetIcon::folder);
       #endif

        // 5. Manutenzione
        items.push_back (CsoundActionSheetItem::separator());
        add (10, "Restore Examples", CsoundActionSheetIcon::resetDefault);

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
                case 9: safeThis->promptOpenExample();      break;
                case 10: safeThis->promptRestoreExamples(); break;
                case 4: safeThis->promptInitializeSession (true);  break;
                case 5: safeThis->promptInitializeSession (false); break;
                case 7: safeThis->importSamples(); break;
               #if JUCE_IOS
                case 8: safeThis->manageFiles(); break;
               #endif
               #if ! JUCE_IOS
                case 6:
                {
                    const auto linked = safeThis->audioProcessor.getLinkedCsdFile();
                    if (linked.existsAsFile())
                        linked.revealToUser();
                    break;
                }
                case 12:
                {
                    // Crea le cartelle se mancano, poi apre apeCsound/
                    // (startAsProcess su una cartella: Finder / Explorer / xdg-open).
                    for (auto* env : { "SSDIR", "SFDIR", "SADIR", "INCDIR" })
                        CsoundAudioProcessor::getCsoundFolder (env);
                    CsoundAudioProcessor::getUserCsdFolder().createDirectory();
                    CsoundAudioProcessor::getBaseFolder().startAsProcess();
                    break;
                }
               #endif
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
    parameterPanel.onToggleGraphsRequested     = [this] { toggleGraphs(); };
    parameterPanel.isGraphsVisible             = [this] { return showingGraphs; };

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
    refreshErrorMarkers (false); // errori della compilazione in corso, se l'editor viene riaperto

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

    // Scorciatoie (vedi CsoundShortcuts.h). Risalgono fin qui dal campo
    // focalizzato (editor di codice, campi dei parametri): CodeView non
    // consuma i tasti con Cmd/Ctrl.
    namespace SC = CsoundShortcuts;

    if (key == SC::apply())          { performApply(); return true; }
    if (key == SC::find())           { openFindBar(); return true; }
    if (key == SC::showParameters()) { toggleParameterPanel(); return true; }
    if (key == SC::showConsole())    { toggleConsole(); return true; }
    if (key == SC::evaluateLine())   { setEvaluateLineVisible (! opcodeHelpBar.isEvaluateMode()); return true; }
    if (key == SC::showGraphs())     { toggleGraphs(); return true; }

    if (key == SC::largerText() || key == SC::cmd ('+') || key == SC::cmd ('=', true))
    {
        if (parameterPanel.onFontSizeChangeRequested) parameterPanel.onFontSizeChangeRequested (+1);
        return true;
    }

    if (key == SC::smallerText())
    {
        if (parameterPanel.onFontSizeChangeRequested) parameterPanel.onFontSizeChangeRequested (-1);
        return true;
    }

    // File: come tutte le altre, in standalone E nei plugin. Arrivano qui
    // solo se la finestra di apeCsound ha il focus e l'host le passa al
    // plugin; altrimenti restano alla DAW.
    if (key == SC::save())       { performSaveLinked(); return true; }
    if (key == SC::saveAs())     { promptSaveSession(); return true; }
    if (key == SC::load())       { promptLoadSession(); return true; }
    if (key == SC::newSession()) { promptInitializeSession (false); return true; } // Init Session Clear

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
        // Barra Cerca/Sostituisci aperta: Esc la chiude, da qualunque punto
        // (campi, bottoni Match case/Whole word...).
        if (findBar.isVisible())
        {
            closeFindBar();
            return true;
        }

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
        sharedUndoManager.perform (new CodeEditTransactionProxy (document, isApplyingCodeUndoRedo,
                                                                 [this] { editor.revealLastEdit(); }));
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
    audioSettingsView.setBounds (area);
    csdBrowser.setBounds (area);

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

    // Pausa (bypass) subito a destra di Apply, stesso diametro dei bottoni rotondi.
    toolbar.removeFromLeft (8);
    pauseButton.setBounds (toolbar.removeFromLeft (panelToolbarButtonDiameter)
                                  .withSizeKeepingCentre (panelToolbarButtonDiameter, panelToolbarButtonDiameter));


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

    // Barra "ingresso audio muto" (solo standalone, vedi InputMutedBar).
    if (inputMutedBar.isVisible())
        inputMutedBar.setBounds (area.removeFromTop (sessionWarningBarHeight));

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

    // Fascia in basso: consolle e/o pannello Graphs (a destra della
    // consolle). Nascosti entrambi: editor/opcodeHelpBar recuperano TUTTO lo
    // spazio, niente resta riservato/vuoto.
    clearConsoleButton.setVisible (showingConsole);
    graphsDivider.setVisible (showingConsole && showingGraphs);

    if (showingConsole || showingGraphs)
    {
        // Altezza ridimensionabile (consoleDivider), mai sotto
        // consoleMinHeight, mai cosi' alta da lasciare a editor/opcodeHelpBar
        // meno di consoleEditorMinHeight.
        const int maxConsoleHeight = juce::jmax (consoleMinHeight,
                                                  area.getHeight() - consoleEditorMinHeight - consoleDividerHeight - opcodeHelpBarHeight);
        consoleHeight = juce::jlimit (consoleMinHeight, maxConsoleHeight, consoleHeight);

        auto bottomArea = area.removeFromBottom (consoleHeight);
        auto consoleDividerArea = area.removeFromBottom (consoleDividerHeight);

        consoleDivider.setBounds (consoleDividerArea.expanded (0, dividerGrabMargin)); // presa alta, striscia stretta
        consoleDivider.toFront (false);

        if (showingGraphs)
        {
            if (showingConsole)
            {
                const int maxGraphsWidth = juce::jmax (graphsMinWidth,
                                                        bottomArea.getWidth() - graphsConsoleMinWidth - sidebarDividerWidth);
                graphsWidth = juce::jlimit (graphsMinWidth, maxGraphsWidth, graphsWidth);

                graphsPanel.setBounds (bottomArea.removeFromRight (graphsWidth));
                graphsDivider.setBounds (bottomArea.removeFromRight (sidebarDividerWidth).expanded (dividerGrabMargin, 0));
                graphsDivider.toFront (false);
            }
            else
            {
                graphsPanel.setBounds (bottomArea);
            }
        }

        if (showingConsole)
        {
            logConsole.setBounds (bottomArea);

            // clearConsoleButton galleggia in overlap sopra l'angolo in alto
            // a destra di logConsole (richiesto esplicitamente).
            const auto consoleTopRight = bottomArea.getTopRight();
            clearConsoleButton.setBounds (consoleTopRight.x - clearConsoleButtonMargin*2 - clearConsoleButtonDiameter,
                                          consoleTopRight.y + clearConsoleButtonMargin*2,
                                          clearConsoleButtonDiameter, clearConsoleButtonDiameter);
        }
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

    // I divisori qui sopra fanno toFront() a ogni layout: le viste
    // sovrapposte aperte (Audio/MIDI Settings, About, Guide, menu) vanno
    // riportate sopra, altrimenti un resized() mentre sono aperte - es. la
    // barra "input muted" che compare dopo Mute dal pannello audio - le
    // lascerebbe sotto ai divisori.
    keepOverlaysOnTop();
}

void CsoundAudioProcessorEditor::keepOverlaysOnTop()
{
    for (juce::Component* overlay : { static_cast<juce::Component*> (&aboutView),
                                      static_cast<juce::Component*> (&guideView),
                                      static_cast<juce::Component*> (&audioSettingsView),
                                      static_cast<juce::Component*> (&csdBrowser) })
        if (overlay->isVisible())
            overlay->toFront (false);

    // Menu (CsoundActionSheet) aperti: sempre in cima a tutto.
    for (auto* child : getChildren())
        if (dynamic_cast<CsoundActionSheet*> (child) != nullptr)
            child->toFront (false);
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

void CsoundAudioProcessorEditor::submitConsoleInput()
{
    const auto text = opcodeHelpBar.input.getText().trim();
    if (text.isEmpty())
        return;

    consoleInputHistory.removeString (text);
    consoleInputHistory.add (text);
    while (consoleInputHistory.size() > 50)
        consoleInputHistory.remove (0);
    consoleInputHistoryPos = consoleInputHistory.size();

    audioProcessor.evaluateCode (text);
    opcodeHelpBar.input.setText ({});
}

void CsoundAudioProcessorEditor::setEvaluateLineVisible (bool show)
{
    opcodeHelpBar.setEvaluateMode (show);
    if (show)
    {
        opcodeHelpBar.input.grabKeyboardFocus();
    }
    else
    {
        opcodeHelpBar.input.setText ({});
        if (opcodeHelpBar.input.hasKeyboardFocus (true))
            editor.grabKeyboardFocus();
    }
}

void CsoundAudioProcessorEditor::toggleConsole()
{
    showingConsole = ! showingConsole;

    // Stesso schema di toggleParameterPanel() sopra: editor/opcodeHelpBar si
    // allargano per recuperare lo spazio (vedi resized()), nessun overlay.
    logConsole.setVisible (showingConsole);
    consoleDivider.setVisible (showingConsole || showingGraphs);

    resized();
    repaint();
}

void CsoundAudioProcessorEditor::toggleGraphs()
{
    showingGraphs = ! showingGraphs;

    // visible/hidden: il pannello avvia/ferma da se' la registrazione di
    // display/dispfft (CsoundGraphsPanel::visibilityChanged).
    graphsPanel.setVisible (showingGraphs);
    consoleDivider.setVisible (showingConsole || showingGraphs);

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
    // ~/Documents/apeCsound/CSD/User (vedi getCsdChooserStartDirectory); come nome
    // si propone quello del file collegato, se c'e', altrimenti Untitled.csd.
    const auto linked = audioProcessor.getLinkedCsdFile();
    const auto proposedName = linked != juce::File{} ? linked.getFileName() : juce::String ("Untitled.csd");
    const auto startingFile = getCsdChooserStartDirectory (true).getChildFile (proposedName);

    auto withPicker = [this, startingFile, onSaved]
    {
        chooseCsdFile ("Save as...", startingFile, true,
            [this, onSaved] (const juce::File& chosen)
            {
                if (chosen == juce::File{})
                    return; // annullato dall'utente - onSaved NON scatta

                auto file = chosen;

               #if ! JUCE_IOS
                // Non su iOS: il file scelto dal picker e' l'unico a cui abbiamo
                // accesso (bookmark); cambiargli nome lo renderebbe inaccessibile.
                if (! file.hasFileExtension ("csd"))
                    file = file.withFileExtension ("csd");
               #endif

                saveSessionTo (file, onSaved);
            });
    };

   #if JUCE_IOS
    // iOS: browser interno (vedi CsdBrowserView.h) - il picker non vede la
    // cartella di apeCsound nel contenitore dell'App Group.
    csdBrowser.onSave = [this, onSaved] (const juce::File& file) { saveSessionTo (file, onSaved); };
    csdBrowser.onOtherLocations = withPicker;
    csdBrowser.show (CsdBrowserView::Mode::save, "Save as", CsoundAudioProcessor::getCsdFolder(),
                     CsoundAudioProcessor::getUserCsdFolder(), proposedName);
   #else
    withPicker();
   #endif
}

void CsoundAudioProcessorEditor::saveSessionTo (const juce::File& file, std::function<void()> onSaved)
{
    const bool ok = audioProcessor.saveSessionToFile (file, document.getAllContent());

    // saveSessionToFile ha collegato la sessione al file e azzerato
    // l'eventuale avviso (file mancante/diverso): la barra, se era
    // visibile, sparisce.
    if (ok)
        updateSessionStatus();

    appendToLog (ok ? ("--- Session saved to " + file.getFullPathName()
                       + " (linked as " + audioProcessor.getLinkedCsdDisplayPath() + ") ---")
                     : "--- Failed to save session (file not writable?) ---");

    // Scatta SOLO se il file e' stato scritto davvero - usato da
    // loadSessionFile() per incatenare "salva, poi procedi col Load" quando
    // l'utente sceglie "Save" nel dialogo di conferma (vedi il commento su
    // promptSaveSession() in PluginEditor.h).
    if (ok && onSaved)
        onSaved();
}

void CsoundAudioProcessorEditor::parentHierarchyChanged()
{
   #if JucePlugin_Build_Standalone && ! JUCE_IOS
    // Solo standalone desktop: StandaloneFilterWindow disegna una barra del
    // titolo JUCE (con il bottone "Options"); la sostituiamo con quella
    // nativa del sistema. Differita (callAsync) perche' cambiare barra
    // ricrea il peer della finestra, e qui siamo dentro il reparenting.
    if (nativeTitleBarApplied
        || audioProcessor.wrapperType != juce::AudioProcessor::wrapperType_Standalone
        || findParentComponentOfClass<juce::DocumentWindow>() == nullptr)
        return;

    nativeTitleBarApplied = true;
    juce::Component::SafePointer<CsoundAudioProcessorEditor> safeThis (this);

    juce::MessageManager::callAsync ([safeThis]
    {
        if (safeThis == nullptr)
            return;

        auto* window = safeThis->findParentComponentOfClass<juce::DocumentWindow>();

        if (window == nullptr)
            return;

        window->setUsingNativeTitleBar (true);

        // Il bottone "Options" (privato di StandaloneFilterWindow) resta
        // figlio della finestra e finirebbe sopra la toolbar: lo nascondiamo.
        // Le sue funzioni sono nel menu (Audio/MIDI Settings...).
        for (auto* child : window->getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*> (child))
                if (button->getButtonText() == "Options")
                    button->setVisible (false);
    });
   #endif
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

const char* CsoundAudioProcessorEditor::csoundFolderKey (int which)
{
    switch (which)
    {
        case 1:  return CsoundAudioProcessor::kOutputFilesFolderKey;
        case 2:  return CsoundAudioProcessor::kExtraAnalysisFolderKey;
        case 3:  return CsoundAudioProcessor::kExtraIncludeFolderKey;
        default: return CsoundAudioProcessor::kExtraSamplesFolderKey;
    }
}

juce::String CsoundAudioProcessorEditor::csoundFolderEnvName (int which)
{
    switch (which)
    {
        case 1:  return "SFDIR";
        case 2:  return "SADIR";
        case 3:  return "INCDIR";
        default: return "SSDIR";
    }
}

juce::String CsoundAudioProcessorEditor::csoundFolderTitle (int which)
{
    switch (which)
    {
        case 1:  return "Output Files Folder";
        case 2:  return "Extra Analysis Folder";
        case 3:  return "Extra Include Folder";
        default: return "Extra Samples Folder";
    }
}

void CsoundAudioProcessorEditor::setCsoundFolder (int which, const juce::File& folder, const juce::MemoryBlock& bookmark)
{
    CsoundAudioProcessor::setUserFolderSetting (csoundFolderKey (which), folder, bookmark);

    // Il menu Config resta aperto (voci keepOpen): percorso e spunte si aggiornano.
    CsoundActionSheet::repaintOpenSheets (*this);

    appendToLog ("--- " + csoundFolderTitle (which) + ": "
                 + (folder == juce::File{} ? juce::String ("default (apeCsound folder)") : folder.getFullPathName())
                 + " - takes effect at the next Apply ---");
}

void CsoundAudioProcessorEditor::chooseCsoundFolder (int which)
{
    // Scelta (o cambio) della cartella: le altre azioni - Reveal / Remove -
    // stanno nella pagina del menu Config > Extra Folders > <cartella>.
    const auto current = CsoundAudioProcessor::getUserFolderSetting (csoundFolderKey (which));
    const auto startFolder = current != juce::File{} ? current
                           : CsoundAudioProcessor::getCsoundFolder (csoundFolderEnvName (which));

    activeFileChooser = std::make_unique<juce::FileChooser> (csoundFolderTitle (which), startFolder,
                                                            juce::String(), true, false, this);

    activeFileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [this, which] (const juce::FileChooser& chooser)
                                    {
                                        const auto folder = chooser.getResult();

                                       #if JUCE_IOS
                                        // Fuori dalla sandbox: senza accesso aperto isDirectory()
                                        // fallirebbe. Serve il bookmark della URL del picker.
                                        auto url = chooser.getURLResult();
                                        const auto bookmark = SecurityScopedFile::bookmarkFromChooserURL (url);

                                        if (folder != juce::File{} && bookmark.getSize() > 0)
                                            setCsoundFolder (which, folder, bookmark);
                                        else if (folder != juce::File{})
                                            appendToLog ("--- Could not get access to " + folder.getFullPathName() + " ---");
                                       #else
                                        if (folder.isDirectory())
                                            setCsoundFolder (which, folder);
                                       #endif
                                    });
}

namespace
{
    // Tipi accettati da Import Files... e cartella di destinazione: UNICA
    // tabella, usata sia per il filtro del dialogo sia per lo smistamento.
    struct ImportGroup { const char* folder; const char* label; juce::StringArray exts; };

    const std::vector<ImportGroup>& importGroups()
    {
        static const std::vector<ImportGroup> groups {
            { "CSD/User", "sessions",       { "csd" } },
            { "SSDIR",    "audio",          { "wav", "wave", "aif", "aiff", "aifc", "flac", "ogg", "caf", "mp3" } },
            { "INCDIR",   "#include",       { "udo", "inc", "txt" } },
            { "SADIR",    "analysis",       { "pvx", "ats", "lpc", "het", "cv" } },
        };
        return groups;
    }

    // "*.csd;*.wav;..." per il FileChooser.
    juce::String importFilePatterns()
    {
        juce::StringArray patterns;
        for (const auto& g : importGroups())
            for (const auto& e : g.exts)
                patterns.add ("*." + e);
        return patterns.joinIntoString (";");
    }

    // "sessions .csd -> CSD/User; audio .wav .aif ... -> SSDIR; ..."
    juce::String importTypesDescription()
    {
        juce::StringArray parts;
        for (const auto& g : importGroups())
        {
            juce::StringArray exts;
            for (const auto& e : g.exts)
                exts.add ("." + e);
            parts.add (juce::String (g.label) + " " + exts.joinIntoString (" ") + " -> " + g.folder);
        }
        return parts.joinIntoString ("; ");
    }
}

juce::String CsoundAudioProcessorEditor::importDestinationFor (const juce::String& fileName)
{
    // Destinazione dall'estensione (vedi importGroups). Vuota = tipo non
    // gestito (saltato).
    const auto ext = fileName.fromLastOccurrenceOf (".", false, false).toLowerCase();

    for (const auto& g : importGroups())
        if (g.exts.contains (ext))
            return g.folder;

    return {};
}

void CsoundAudioProcessorEditor::importSamples (const juce::File& intoFolder)
{
    // Stesso schema di chooseCsdFile: dialogo nativo con l'editor come parent
    // (indispensabile nell'AUv3 iOS), chooser tenuto in vita come membro.
    //
    // Import Files...: il dialogo mostra solo i tipi supportati (vedi
    // importGroups), su tutte le piattaforme. Su iOS il filtro diventa una
    // lista di UTType ricavati dalle estensioni: anche quelle senza un tipo
    // di sistema (.udo, .pvx...) hanno un tipo "dinamico" legato
    // all'estensione, lo stesso che iOS assegna ai file. Il picker iOS non
    // mostra un titolo: l'elenco dei tipi va in console (titolo corto anche
    // su desktop).
    //
    // Manage Files > Import Files Here (intoFolder, iOS): nessun filtro,
    // qualunque file va nella cartella scelta.
    const bool sortByType = intoFolder == juce::File{};
    const juce::String patterns = sortByType ? importFilePatterns() : juce::String ("*");

    if (sortByType)
        appendToLog ("--- Import Files: " + importTypesDescription() + " ---");

    activeFileChooser = std::make_unique<juce::FileChooser> ("Import Files...", juce::File{}, patterns, true, false, this);

    const int flags = juce::FileBrowserComponent::openMode
                    | juce::FileBrowserComponent::canSelectFiles
                    | juce::FileBrowserComponent::canSelectMultipleItems;

    juce::Component::SafePointer<CsoundAudioProcessorEditor> safeThis (this);

    activeFileChooser->launchAsync (flags, [safeThis, intoFolder] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        const auto urls = chooser.getURLResults();

        if (urls.isEmpty())
            return; // annullato

        std::map<juce::String, int> importedPerFolder;
        juce::StringArray failed, skipped;

        for (const auto& url : urls)
        {
            auto name = juce::URL::removeEscapeChars (url.getFileName());

            if (name.isEmpty())
                name = url.getLocalFile().getFileName();

            // Cartella esplicita (Manage Files, iOS): tutto li', qualunque
            // tipo. Altrimenti dall'estensione.
            const auto folderName = intoFolder != juce::File{}
                                  ? intoFolder.getRelativePathFrom (CsoundAudioProcessor::getBaseFolder())
                                  : importDestinationFor (name);

            if (folderName.isEmpty())
            {
                skipped.add (name);
                continue;
            }

            const auto destFolder = intoFolder != juce::File{} ? intoFolder
                                  : folderName == "CSD/User" ? CsoundAudioProcessor::getUserCsdFolder()
                                                             : CsoundAudioProcessor::getCsoundFolder (folderName);
            destFolder.createDirectory();
            const auto dest = destFolder.getChildFile (juce::File::createLegalFileName (name));

            // La URL del picker porta l'accesso security-scoped (iCloud
            // Drive, altri provider): la si legge come stream, non come File.
            std::unique_ptr<juce::InputStream> in (url.createInputStream (
                juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)));

            if (in == nullptr)
            {
                failed.add (name);
                continue;
            }

            dest.deleteFile(); // stesso nome: sostituisce

            juce::FileOutputStream out (dest);

            if (! out.openedOk() || out.writeFromInputStream (*in, -1) < 0)
            {
                failed.add (name);
                continue;
            }

            out.flush();
            ++importedPerFolder[folderName];
        }

        for (const auto& [folderName, count] : importedPerFolder)
            safeThis->appendToLog ("--- Imported " + juce::String (count) + " file(s) into apeCsound/" + folderName + " ---");

        if (! skipped.isEmpty())
            safeThis->appendToLog ("--- Skipped (not a .csd, audio, #include or analysis file): " + skipped.joinIntoString (", ") + " ---");

        if (! failed.isEmpty())
            safeThis->appendToLog ("--- Could not import: " + failed.joinIntoString (", ") + " ---");

        // Manage Files aperto: mostra i file appena copiati.
        if (safeThis->csdBrowser.isVisible())
            safeThis->csdBrowser.refreshContents();
    });
}

#if JUCE_IOS
void CsoundAudioProcessorEditor::manageFiles()
{
    // "Manage Files..." (solo iOS): la cartella apeCsound (CSD, SSDIR, SFDIR,
    // SADIR, INCDIR) sta nel contenitore dell'App Group, che l'app File non
    // mostra. Il browser (CsdBrowserView, modalita' manage) e' la porta da e
    // verso File: Import copia nella cartella aperta, Export copia i file
    // scelti in una cartella scelta col picker. Stesso comportamento
    // nell'app e nell'AUv3 (i file sono nell'App Group condiviso).
    CsoundAudioProcessor::getCsoundFolder ("SFDIR"); // crea le cartelle se mancano
    CsoundAudioProcessor::getCsoundFolder ("SSDIR");
    CsoundAudioProcessor::getCsoundFolder ("SADIR");
    CsoundAudioProcessor::getCsoundFolder ("INCDIR");
    CsoundAudioProcessor::getUserCsdFolder().createDirectory();

    csdBrowser.onImport = [this] (const juce::File& folder) { importSamples (folder); };
    csdBrowser.onExport = [this] (const juce::Array<juce::File>& files) { copyFilesToChosenFolder (files); };
    csdBrowser.show (CsdBrowserView::Mode::manage, "Manage Files",
                     CsoundAudioProcessor::getBaseFolder(), CsoundAudioProcessor::getBaseFolder());
}

void CsoundAudioProcessorEditor::copyFilesToChosenFolder (const juce::Array<juce::File>& files)
{
    activeFileChooser = std::make_unique<juce::FileChooser> ("Export to...", juce::File{}, juce::String(), true, false, this);

    juce::Component::SafePointer<CsoundAudioProcessorEditor> safeThis (this);

    activeFileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                    [safeThis, files] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        const auto folderUrl = chooser.getURLResult();

        if (folderUrl.isEmpty())
            return; // annullato

        int copied = 0;
        juce::StringArray failed;

        for (const auto& file : files)
        {
            // URL figlia della cartella scelta: eredita il suo bookmark
            // security-scoped (stesso schema del Save as su iOS, vedi
            // chooseCsdFile); createOutputStream apre e chiude l'accesso.
            const auto dest = folderUrl.getChildURL (file.getFileName());
            juce::FileInputStream in (file);
            auto out = dest.createOutputStream();

            if (! in.openedOk() || out == nullptr)
            {
                failed.add (file.getFileName());
                continue;
            }

            // FileOutputStream scrive in coda a un file esistente: si
            // riparte da zero (sostituisce).
            if (auto* fos = dynamic_cast<juce::FileOutputStream*> (out.get()))
            {
                fos->setPosition (0);
                fos->truncate();
            }

            if (out->writeFromInputStream (in, -1) < 0)
            {
                failed.add (file.getFileName());
                continue;
            }

            out->flush();
            ++copied;
        }

        safeThis->appendToLog ("--- Exported " + juce::String (copied) + " file(s) to "
                               + chooser.getResult().getFullPathName() + " ---");

        if (! failed.isEmpty())
            safeThis->appendToLog ("--- Could not export: " + failed.joinIntoString (", ") + " ---");
    });
}
#endif


// Cartella di partenza dei file chooser:
//  - Save As -> SEMPRE apeCsound/CSD/User (anche se la sessione viene da un
//    esempio o da un'altra cartella: gli esempi di fabbrica restano intatti);
//  - Load / Relocate -> cartella del file collegato, se esiste, altrimenti
//    apeCsound/CSD (si vedono sia User sia Examples).
// Le cartelle vengono create qui, solo quando servono.
juce::File CsoundAudioProcessorEditor::getCsdChooserStartDirectory (bool forSave) const
{
    CsoundAudioProcessor::getUserCsdFolder().createDirectory(); // crea anche CSD

    if (forSave)
        return CsoundAudioProcessor::getUserCsdFolder();

    const auto linked = audioProcessor.getLinkedCsdFile();

    if (linked != juce::File{} && linked.getParentDirectory().isDirectory())
        return linked.getParentDirectory();

    return CsoundAudioProcessor::getCsdFolder();
}

void CsoundAudioProcessorEditor::openCsdWithBrowserOrPicker (const juce::String& title, const juce::File& startDir)
{
    auto withPicker = [this, title, startDir]
    {
        chooseCsdFile (title, startDir, false,
            [this] (const juce::File& file)
            {
                if (file != juce::File{}) // non annullato dall'utente
                    loadSessionFile (file);
            });
    };

   #if JUCE_IOS
    // iOS: browser interno (vedi CsdBrowserView.h) - il picker non vede la
    // cartella di apeCsound nel contenitore dell'App Group. Un file collegato
    // fuori da CSD (es. iCloud) fa partire il browser dalla radice CSD.
    csdBrowser.onOpen = [this] (const juce::File& file) { loadSessionFile (file); };
    csdBrowser.onOtherLocations = withPicker;
    csdBrowser.show (CsdBrowserView::Mode::open, title.trimCharactersAtEnd ("."),
                     CsoundAudioProcessor::getCsdFolder(), startDir);
   #else
    withPicker();
   #endif
}

void CsoundAudioProcessorEditor::promptLoadSession()
{
    openCsdWithBrowserOrPicker ("Load...", getCsdChooserStartDirectory (false));
}

void CsoundAudioProcessorEditor::promptOpenExample()
{
    // Al primo uso gli esempi potrebbero non essere ancora estratti.
    CsoundAudioProcessor::ensureExamplesInstalled();

    auto dir = CsoundAudioProcessor::getExamplesFolder();
    if (! dir.isDirectory())
        dir = getCsdChooserStartDirectory (false);

    openCsdWithBrowserOrPicker ("Open Example...", dir);
}

void CsoundAudioProcessorEditor::promptRestoreExamples()
{
    juce::Component::SafePointer<CsoundAudioProcessorEditor> safeThis (this);

    showNativeTwoButtonAlertAsync (
        "Restore examples?",
        "The original examples will be copied again into apeCsound/CSD/Examples, "
        "replacing any changes made to them. Your own files in CSD/User are not touched.",
        "Cancel", "Restore",
        [safeThis] (int choice)
    {
        if (safeThis == nullptr || choice != 2)
            return;

        const bool ok = CsoundAudioProcessor::installExamples (true);
        safeThis->appendToLog (ok ? "--- Examples restored in " + CsoundAudioProcessor::getExamplesFolder().getFullPathName() + " ---"
                                  : juce::String ("--- Could not restore the examples ---"));
    }, this);
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

CsoundAudioProcessorEditor::InputMutedBar::InputMutedBar()
{
    messageLabel.setText ("Audio input is muted to avoid feedback.  Audio/MIDI Settings " + juce::String::charToString ((juce::juce_wchar) 0x203a),
                          juce::dontSendNotification);
    messageLabel.setInterceptsMouseClicks (false, false); // il tocco arriva alla barra (mouseUp)
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    unmuteButton.setMouseCursor (juce::MouseCursor::NormalCursor);
    messageLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    messageLabel.setColour (juce::Label::textColourId, juce::Colour (0xff3a2a00));
    messageLabel.setJustificationType (juce::Justification::centredLeft);
    messageLabel.setMinimumHorizontalScale (0.7f);
    addAndMakeVisible (messageLabel);

    unmuteButton.onClick = [this] { muted.setValue (false); };
    addAndMakeVisible (unmuteButton);
}

void CsoundAudioProcessorEditor::InputMutedBar::mouseUp (const juce::MouseEvent& e)
{
    // Tocco sulla barra (fuori da "Unmute"): apre Audio/MIDI Settings.
    if (e.mouseWasClicked() && onMessageClicked)
        onMessageClicked();
}

CsoundAudioProcessorEditor::InputMutedBar::~InputMutedBar()
{
    muted.removeListener (this);
}

void CsoundAudioProcessorEditor::InputMutedBar::attach (juce::Value* muteValue)
{
    muted.removeListener (this);

    if (muteValue == nullptr)
    {
        setVisible (false);
        return;
    }

    muted.referTo (*muteValue);
    muted.addListener (this);
    valueChanged (muted);
}

void CsoundAudioProcessorEditor::InputMutedBar::valueChanged (juce::Value&)
{
    const bool show = (bool) muted.getValue();

    if (show != isVisible())
    {
        setVisible (show);

        if (onVisibilityChanged)
            onVisibilityChanged();
    }
}

void CsoundAudioProcessorEditor::InputMutedBar::paint (juce::Graphics& g)
{
    // Stesso stile della SessionWarningBar: giallo tenue + riga sotto.
    g.fillAll (juce::Colour (0xfffff3c4));
    g.setColour (juce::Colour (0xffe0c56a));
    g.drawLine (0.0f, (float) getHeight() - 0.5f, (float) getWidth(), (float) getHeight() - 0.5f, 1.0f);
}

void CsoundAudioProcessorEditor::InputMutedBar::resized()
{
    auto area = getLocalBounds().reduced (10, 5);
    unmuteButton.setBounds (area.removeFromRight (unmuteButton.getBestWidthForHeight (area.getHeight()) + 12));
    area.removeFromRight (10);
    messageLabel.setBounds (area);
}

void CsoundAudioProcessorEditor::promptRelocateSession()
{
    const auto startingDir = getCsdChooserStartDirectory (false);

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
    updatePauseButton(); // Apply toglie sempre la pausa, anche se la compilazione fallisce

    // Dopo compileAndStart il testo applicato (audioProcessor.getCsdText())
    // coincide di nuovo con quello dell'editor: il bordo rosso del bottone
    // scompare - vedi updateApplyButtonDirtyState().
    updateApplyButtonDirtyState();
}

CsoundAudioProcessorEditor::OpcodeHelpBar::OpcodeHelpBar()
{
    addChildComponent (input);
}

void CsoundAudioProcessorEditor::OpcodeHelpBar::resized()
{
    auto area = getLocalBounds();
    area.removeFromRight (toggleWidth);
    area.removeFromLeft (22); // prompt ">"
    input.setBounds (area);
}

void CsoundAudioProcessorEditor::OpcodeHelpBar::setEvaluateMode (bool on)
{
    if (evaluateMode == on)
        return;
    evaluateMode = on;
    input.setVisible (on);
    repaint();
}

void CsoundAudioProcessorEditor::OpcodeHelpBar::mouseDown (const juce::MouseEvent& e)
{
    if (toggleArea().contains (e.getPosition()) && onEvaluateModeChanged)
        onEvaluateModeChanged (! evaluateMode);
}

void CsoundAudioProcessorEditor::OpcodeHelpBar::mouseMove (const juce::MouseEvent& e)
{
    const bool over = toggleArea().contains (e.getPosition());
    if (over != toggleHovered) { toggleHovered = over; repaint (toggleArea()); }
    setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void CsoundAudioProcessorEditor::OpcodeHelpBar::mouseExit (const juce::MouseEvent&)
{
    if (toggleHovered) { toggleHovered = false; repaint (toggleArea()); }
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

    // Icona ">_" a destra (commuta help <-> Evaluate): accesa in Evaluate.
    auto paintToggle = [this, &g] (juce::Colour fg, juce::Colour bgHover)
    {
        const auto t = toggleArea();
        if (toggleHovered || evaluateMode)
        {
            g.setColour (evaluateMode ? juce::Colour (0xff2ec4b6).withAlpha (0.25f) : bgHover);
            g.fillRoundedRectangle (t.reduced (4, 3).toFloat(), 4.0f);
        }
        g.setColour (fg);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold)));
        g.drawText (">_", t, juce::Justification::centred, false);
    };

    if (evaluateMode)
    {
        g.setColour (juce::Colour (0xff162129));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xff2ec4b6));
        g.fillRect (bounds.removeFromLeft (4));
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold)));
        g.drawText (">", juce::Rectangle<int> (4, 0, 18, getHeight()), juce::Justification::centred, false);

        // Placeholder disegnato QUI (non con setTextToShowWhenEmpty, che
        // JUCE nasconde appena il campo ha il focus): resta visibile col
        // caret che lampeggia, sparisce al primo carattere.
        if (input.isEmpty())
        {
            g.setColour (juce::Colour (0xff5d7682));
            g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
            g.drawText ("score event (i 3 0 1) or orchestra code (schedule(3, 0, 1))",
                        input.getBounds().withTrimmedLeft (input.getLeftIndent() + 2), juce::Justification::centredLeft, true);
        }

        paintToggle (juce::Colour (0xff2ec4b6), juce::Colours::white.withAlpha (0.08f));
        return;
    }

    // Stesso tono "post-it" del popup di autocompletamento, cosi' le due
    // forme di help inline (barra fissa + popup flottante) si riconoscono
    // come parte della stessa funzionalita'.
    g.setColour (juce::Colour (0xfffdf6e3));
    g.fillRect (bounds);
    g.setColour (juce::Colour (0xffd7c89a));
    g.drawLine (0.0f, (float) bounds.getHeight() - 0.5f, (float) bounds.getWidth(), (float) bounds.getHeight() - 0.5f, 1.0f);
    paintToggle (juce::Colour (0xff8a7a55), juce::Colour (0xffd7c89a).withAlpha (0.5f));

    auto area = bounds.reduced (10, 0);
    area.removeFromRight (toggleWidth - 6);

    // Errore Csound sulla riga del caret (vedi CsoundCodeEditor::kErrorHelpCategory):
    // barra rossa con "Error - line N" e il messaggio di Csound.
    if (categoryText == CsoundCodeEditor::kErrorHelpCategory)
    {
        g.setColour (juce::Colour (0xfffdecec));
        g.fillRect (bounds);
        g.setColour (juce::Colour (0xffe5484d));
        g.fillRect (bounds.removeFromLeft (4));
        g.drawLine (0.0f, (float) getHeight() - 0.5f, (float) getWidth(), (float) getHeight() - 0.5f, 1.0f);
        paintToggle (juce::Colour (0xffb3261e), juce::Colour (0xffe5484d).withAlpha (0.15f));

        const auto boldFont = juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (boldFont, syntaxText, 0.0f, 0.0f);
        const int titleWidth = juce::jmin ((int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 2, area.getWidth() / 3);

        g.setFont (boldFont);
        g.setColour (juce::Colour (0xffb3261e));
        g.drawFittedText (syntaxText, area.removeFromLeft (titleWidth), juce::Justification::centredLeft, 1);
        area.removeFromLeft (10);

        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.setColour (juce::Colour (0xff5c1a16));
        g.drawFittedText (descriptionText, area, juce::Justification::centredLeft, 1);
        return;
    }

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

void CsoundAudioProcessorEditor::csoundDiagnosticsChanged()
{
    refreshErrorMarkers (true);
}

void CsoundAudioProcessorEditor::refreshErrorMarkers (bool scrollToFirst)
{
    const auto& diagnostics = audioProcessor.getCodeDiagnostics();

    if (diagnostics.isEmpty())
    {
        editor.clearErrorMarkers();
        return;
    }

    // Le righe di Csound si riferiscono al testo compilato: se nel frattempo
    // l'editor e' stato modificato non corrispondono piu' (restano in console).
    if (document.getAllContent() != audioProcessor.getCsdText())
        return;

    const bool hadMarkers = editor.hasErrorMarkers();

    juce::Array<std::pair<int, juce::String>> markers;

    for (const auto& d : diagnostics)
        markers.add ({ d.line - 1, d.message }); // Csound: 1-based, CodeView: 0-based

    editor.setErrorMarkers (markers);

    // Porta in vista il primo errore solo se prima non c'erano segni (dopo
    // Apply), per non far saltare la vista a ogni PERF ERROR successivo.
    if (scrollToFirst && ! hadMarkers)
    {
        const int first = editor.getFirstErrorLine();

        if (first >= 0 && (first < editor.getFirstLineOnScreen()
                           || first >= editor.getFirstLineOnScreen() + editor.getNumLinesOnScreen() - 1))
            editor.scrollToLine (juce::jmax (0, first - 3));
    }
}

void CsoundAudioProcessorEditor::updatePauseButton()
{
    const bool paused = audioProcessor.isCsoundPaused();

    // Sempre l'icona della pausa (il "play" e' Apply): lo stato si vede dal fondo ambra.
    pauseButton.setName ("pause");
    pauseButton.setToggleState (paused, juce::dontSendNotification);
    pauseButton.setTooltip (paused ? "Csound is paused (bypass): click to resume. Apply also resumes."
                                   : "Pause Csound (bypass: the input passes through unchanged).");

    // In pausa: fondo ambra, ben visibile.
    // (Il bottone "acceso" usa buttonOnColourId.)
    pauseButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe0a030));

    pauseButton.repaint();
}

void CsoundAudioProcessorEditor::csoundEngineStarted()
{
    // Ricompilazione = pausa tolta (vedi CsoundAudioProcessor::compileAndStart).
    updatePauseButton();

    // Il motore viene (ri)compilato con un'istanza CSOUND* nuova ogni
    // volta (vedi compileAndStart): l'elenco opcode va quindi ripreso da
    // capo ogni volta che riparte, non solo alla creazione dell'editor.
    editor.setOpcodeSignatures (audioProcessor.getOpcodeSignatures());

    // Gruppi del pannello Parametri ("; @group" nel codice): seguono il
    // testo COMPILATO, quindi si aggiornano qui, a ogni Apply/Load.
    parameterPanel.refreshGrouping();
}

void CsoundAudioProcessorEditor::csoundEngineStopped()
{
}

void CsoundAudioProcessorEditor::appendToLog (const juce::String& text)
{
    // Consolle svuotata (Clear, Apply): si riparte da zero.
    if (logConsole.isEmpty())
        consoleLineCount = 0;

    logConsole.moveCaretToEnd();
    logConsole.insertTextAtCaret (text + "\n");

    for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
        if (*p == '\n')
            ++consoleLineCount;
    ++consoleLineCount;

    // Numero di righe limitato (un TextEditor che cresce senza limite
    // diventa sempre piu' lento). Prima il testo intero veniva riletto e
    // spezzato in righe a OGNI aggiunta e, oltre il limite, riscritto a
    // ogni aggiunta: con molti print/printk il thread dei messaggi restava
    // occupato di continuo e REAPER si bloccava. Ora le righe si contano
    // man mano e la potatura (una sola riscrittura) avviene solo quando si
    // supera il limite di kConsoleTrimSlack righe.
    if (consoleLineCount > kConsoleMaxLines + kConsoleTrimSlack)
    {
        auto lines = juce::StringArray::fromLines (logConsole.getText());
        while (lines.size() > 0 && lines[lines.size() - 1].isEmpty())
            lines.remove (lines.size() - 1);

        if (lines.size() > kConsoleMaxLines)
            lines.removeRange (0, lines.size() - kConsoleMaxLines);

        logConsole.setText (lines.joinIntoString ("\n") + "\n", false);
        logConsole.moveCaretToEnd();
        consoleLineCount = lines.size();
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

    // Motore Csound: libreria statica realmente linkata (versione,
    // precisione, numero di opcode) e banner di avvio (build/commit) -
    // vedi engineLine1/engineLine2, impostate da PluginEditor.
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawFittedText (engineLine1, 0, 158, w, 14, juce::Justification::centred, 1);
    g.drawFittedText (engineLine2, 0, 171, w, 14, juce::Justification::centred, 1);

    // Separatore.
    g.setColour (kAboutBorder);
    g.fillRect (40, 189, w - 80, 1);

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

    // Credits degli esempi di fabbrica (sintesi di CSD/Examples/README_Examples.md).
    g.setColour (kAboutBorder);
    g.fillRect (40, 404, w - 80, 1);

    g.setColour (kAboutTextMuted);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawFittedText ("EXAMPLES", 0, 412, w, 14, juce::Justification::centred, 1);

    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawFittedText ("Cabbage examples by Iain McCurdy (CC BY-NC-SA 4.0),\n"
                      "from the Cabbage collection by Rory Walsh.\n"
                      "CsoundQt examples by Iain McCurdy, Joachim Heintz,\n"
                      "Ren" + uc (0x00E9) + ", Michael Gogins, Tom Banados Russell and others\n"
                      "(GNU GPL/LGPL), from the CsoundQt collection.\n"
                      "Partially adapted for apeCsound. Full credits:\n"
                      "CSD/Examples/README_Examples.md",
                      16, 430, w - 32, 96, juce::Justification::centred, 7, 0.9f);

    // Piede.
    g.setColour (kAboutBorder);
    g.fillRect (40, 536, w - 80, 1);

    g.setColour (kAboutTextMuted);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawFittedText ("Powered by Csound  " + uc (0x00B7) + "  Built with JUCE", 0, 544, w, 16, juce::Justification::centred, 1);
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawFittedText ("Opcode help text from the Csound Reference Manual\n(GNU FDL), via CsoundQt's opcode database",
                      0, 564, w, 28, juce::Justification::centred, 2);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawFittedText (uc (0x00A9) + " " + juce::String (juce::Time::getCurrentTime().getYear()) + " apeSoft",
                      0, 598, w, 16, juce::Justification::centred, 1);
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
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
    addAndMakeVisible (viewport);
}

//==============================================================================
// Testo della guida, impaginato a blocchi.
//
// Mini-markup di guideText():
//   "# Titolo"     sezione            "## Titolo"   sottosezione
//   "- voce"       elenco             "  - voce"    sotto-elenco
//   riga normale   paragrafo          riga vuota    (ignorata: la spaziatura
//                                                    la decidono i blocchi)
//   "**grassetto**" e `codice` dentro le righe.
// Ogni blocco e' un juce::TextLayout con la propria larghezza: gli elenchi
// hanno il rientro sospeso (le righe a capo restano allineate al testo, non
// al pallino) e la colonna di lettura ha una larghezza massima, centrata,
// cosi' a tutto schermo le righe non diventano lunghissime.
namespace
{
    constexpr float kGuideBodySize   = 14.5f;
    constexpr float kGuideCodeSize   = 13.5f;
    constexpr float kGuideH1Size     = 19.0f;
    constexpr float kGuideH2Size     = 15.5f;
    constexpr float kGuideLineGap    = 3.5f;   // spazio extra fra le righe di un blocco
    constexpr float kGuideMaxColumn  = 760.0f; // larghezza massima della colonna di testo
    constexpr float kGuideBulletIndent = 22.0f;
    constexpr float kGuideSubIndent    = 20.0f;

    juce::Colour guideBodyColour() { return kAboutText.withAlpha (0.86f); }

    // **grassetto** e `codice` in un AttributedString.
    void appendGuideInline (juce::AttributedString& as, const juce::String& line,
                            const juce::Font& font, juce::Colour colour)
    {
        const juce::Font bold { juce::FontOptions (font.getHeight(), juce::Font::bold) };
        const juce::Font code { juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), kGuideCodeSize, juce::Font::plain) };

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
                as.append (line.substring (i), font, colour);
                break;
            }

            if (next > i)
                as.append (line.substring (i, next), font, colour);

            if (isBold)
            {
                const int end = line.indexOf (next + 2, "**");
                if (end < 0) { as.append (line.substring (next), font, colour); break; }
                as.append (line.substring (next + 2, end), bold, kAboutText);
                i = end + 2;
            }
            else
            {
                const int end = line.indexOfChar (next + 1, '`');
                if (end < 0) { as.append (line.substring (next), font, colour); break; }
                as.append (line.substring (next + 1, end), code, kAboutAccent.brighter (0.15f));
                i = end + 1;
            }
        }
    }
}

CsoundAudioProcessorEditor::GuideView::Card::Content::Content()
{
    // Il contenuto DEVE ricevere gli eventi: il Viewport di JUCE non li
    // intercetta (setInterceptsMouseClicks (false, true)), e la rotellina /
    // trackpad arriva al Viewport solo risalendo da un figlio che la riceve
    // (Component::mouseWheelMove la passa al parent). Con il contenuto
    // "trasparente" la rotellina finiva alla scheda e non scorreva.
    // Il trascinamento col dito resta gestito dal Viewport (ScrollOnDragMode).

    const juce::Font body { juce::FontOptions (kGuideBodySize) };
    const juce::Font h1   { juce::FontOptions (kGuideH1Size, juce::Font::bold) };
    const juce::Font h2   { juce::FontOptions (kGuideH2Size, juce::Font::bold) };

    juce::StringArray lines;
    lines.addLines (GuideView::guideText());

    for (const auto& raw : lines)
    {
        if (raw.trim().isEmpty())
            continue;

        Block block;

        if (raw.startsWith ("## "))
        {
            block.type = Block::Type::h2;
            appendGuideInline (block.text, raw.substring (3), h2, kAboutText);
        }
        else if (raw.startsWith ("# "))
        {
            block.type = Block::Type::h1;
            appendGuideInline (block.text, raw.substring (2), h1, kAboutAccent);
        }
        else if (raw.trimStart().startsWith ("- "))
        {
            block.type = Block::Type::bullet;
            block.level = raw.startsWith (" ") ? 1 : 0;
            appendGuideInline (block.text, raw.trimStart().substring (2), body, guideBodyColour());
        }
        else
        {
            block.type = Block::Type::para;
            appendGuideInline (block.text, raw, body, guideBodyColour());
        }

        block.text.setLineSpacing (kGuideLineGap);
        block.text.setWordWrap (juce::AttributedString::byWord);
        blocks.push_back (std::move (block));
    }
}

int CsoundAudioProcessorEditor::GuideView::Card::Content::layoutForWidth (int width)
{
    columnWidth = juce::jmin ((float) width - 8.0f, kGuideMaxColumn);
    columnX = juce::jmax (4.0f, ((float) width - columnWidth) * 0.5f);

    float y = 6.0f;
    const Block* prev = nullptr;

    for (auto& block : blocks)
    {
        // Spazio prima del blocco, secondo il tipo e il blocco precedente.
        float gap = 0.0f;
        using T = Block::Type;

        if (prev != nullptr)
        {
            switch (block.type)
            {
                case T::h1:     gap = 34.0f; break;
                case T::h2:     gap = 20.0f; break;
                case T::para:   gap = prev->type == T::h1 || prev->type == T::h2 ? 10.0f : 12.0f; break;
                case T::bullet: gap = prev->type == T::bullet ? (block.level > 0 ? 4.0f : 8.0f)
                                                              : 10.0f; break;
            }
        }

        y += gap;

        float indent = 0.0f;
        if (block.type == T::bullet)
            indent = kGuideBulletIndent + (float) block.level * kGuideSubIndent;

        const float w = juce::jmax (40.0f, columnWidth - indent);
        block.layout.createLayout (block.text, w);
        block.area = { columnX + indent, y, w, block.layout.getHeight() };

        y += block.layout.getHeight();

        // Titolo di sezione: linea sottile sotto, con un po' d'aria.
        if (block.type == T::h1)
            y += 10.0f;

        prev = &block;
    }

    laidOutWidth = width;
    return (int) std::ceil (y + 40.0f);
}

void CsoundAudioProcessorEditor::GuideView::Card::Content::paint (juce::Graphics& g)
{
    using T = Block::Type;
    const auto clip = g.getClipBounds().toFloat();

    for (const auto& block : blocks)
    {
        if (block.area.getBottom() + 12.0f < clip.getY() || block.area.getY() - 12.0f > clip.getBottom())
            continue;

        if (block.type == T::bullet && block.layout.getNumLines() > 0)
        {
            // Pallino (o trattino per il sotto-elenco) allineato alla prima riga.
            const auto& firstLine = block.layout.getLine (0);
            const float baseline = block.area.getY() + firstLine.lineOrigin.y;
            const float cx = block.area.getX() - 12.0f;

            if (block.level == 0)
            {
                g.setColour (kAboutAccent);
                g.fillEllipse (cx - 2.5f, baseline - firstLine.ascent * 0.42f - 2.5f, 5.0f, 5.0f);
            }
            else
            {
                g.setColour (kAboutTextMuted);
                g.fillRect (cx - 3.5f, baseline - firstLine.ascent * 0.40f - 0.75f, 7.0f, 1.5f);
            }
        }

        block.layout.draw (g, block.area);

        if (block.type == T::h1)
        {
            g.setColour (kAboutBorder);
            g.fillRect (columnX, block.area.getBottom() + 5.0f, columnWidth, 1.0f);
        }
    }
}

void CsoundAudioProcessorEditor::GuideView::Card::paint (juce::Graphics& g)
{
    // A tutto schermo: niente angoli arrotondati ne' bordo.
    g.fillAll (kAboutCardBg);

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
    viewport.setBounds (getLocalBounds().withTrimmedTop (62).reduced (16, 0).withTrimmedBottom (16));

    // Contenuto largo quanto il viewport (meno la scrollbar) e alto quanto
    // il testo impaginato: e' il Viewport a scorrere.
    const int w = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness() - 4);
    const int h = content.layoutForWidth (w);
    content.setSize (w, h);
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
    // A tutto schermo: lo stesso fondo della scheda anche nelle fasce
    // dell'area non sicura (iOS), niente velo scuro sull'editor sotto.
    g.fillAll (kAboutCardBg);
}

void CsoundAudioProcessorEditor::GuideView::resized()
{
    // A tutto schermo (richiesta esplicita, macOS e iOS). Su iOS si resta
    // dentro l'area sicura (notch, barra di stato, indicatore home).
    auto area = getLocalBounds();

   #if JUCE_IOS
    // Si toglie SOLO la parte della vista che cade davvero sotto notch,
    // barra di stato o indicatore home: area sicura dello schermo
    // intersecata con la posizione della vista. Prima gli insets dello
    // schermo venivano tolti per intero anche quando la vista era gia'
    // dentro l'area sicura, lasciando bordi vuoti (la guida non sembrava
    // a tutto schermo).
    if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
    {
        const auto safeOnScreen = display->safeAreaInsets.subtractedFrom (display->totalArea);
        const auto safeLocal = getLocalArea (nullptr, safeOnScreen).getIntersection (area);

        if (! safeLocal.isEmpty())
            area = safeLocal;
    }
   #endif

    card.setBounds (area);
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
    card.viewport.setViewPosition (0, 0);
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
    return R"GUIDE(**apeCsound** runs Csound 7 code live inside the DAW. Write or load a `.csd`, press **Apply**, and the engine recompiles while the track keeps playing.
The track audio reaches the code through `inch`, MIDI through `massign` / `notnum`, and what the code writes with `outs` / `outch` goes back to the track, on as many channels as the track has.

# Using apeCsound in a DAW
apeCsound is a **MIDI effect**: an audio effect that also receives MIDI (Audio Unit / AUv3 "Music Effect", VST3 "Fx"). There is one plug-in for every use, so it is **always loaded as an effect**, also when the `.csd` is an instrument.
- **As an effect** - insert it on an audio track. The track audio arrives through `inch`.
- **As an instrument** - insert it as an effect on a track and send it MIDI notes. An empty audio track is fine: the code generates the sound from MIDI.
## Sending MIDI to it
- **Logic Pro** - Audio FX slot of any track, or the Instrument slot of a Software Instrument track under "AU MIDI-controlled Effects" (it then receives the track's MIDI).
- **Ableton Live** - put it on a track as an audio effect; on a MIDI track set **MIDI To** to that track, then choose apeCsound in the second menu.
- **Cubase / Nuendo** - insert it on an audio track; on a MIDI track choose apeCsound as the MIDI output.
- **REAPER, Bitwig, Studio One** - the MIDI of the track it is on reaches it.
- **iOS** (AUM, Loopy Pro, Drambo, Cubasis...) - load it in an effect slot and select it as a MIDI destination.
- **Standalone** - choose the MIDI inputs (also Bluetooth MIDI) in Audio/MIDI Settings.

# Toolbar
- **Apply** - recompiles and restarts Csound with the editor text. A red ring means the editor differs from the running code.
- **Pause** - pauses Csound (the audio passes through); Apply restarts it.
- **File capsule** (centre) - name, folder and state of the linked `.csd` (coloured dot: saved, unsaved changes, file not found). Tap it for the File menu.
- **Magnifier** - Find / Replace: match case `Aa`, whole word `W`, Replace, Replace All. Enter = next match, Esc closes.
- **Menu** - Parameters, Console, Graphs, Undo / Redo, Audio/MIDI Settings (standalone), Config, this guide, Csound Manual, About.

# File menu (file capsule)
- **Init Session Clear** - empty `.csd`, no parameters, no linked file.
- **Init Session Template** - the built-in template (synth + delay with its parameters).
- **Load...** - opens a `.csd`. You can also drop a `.csd` on the window.
- **Open Example...** - opens one of the factory examples (`CSD/Examples`).
- **Save** - writes the linked file.
- **Save as...** - saves to a new file and links it. It starts in `CSD/User`.
- **Reveal in Finder** (desktop) - shows the linked `.csd` in Finder / Explorer.
- **Import Files...** - copies files into the apeCsound folders, chosen by type:
  - `.csd` to `CSD/User`
  - audio (wav, aif, flac, ogg, caf, mp3) to `SSDIR`
  - `.udo`, `.inc`, `.txt` to `INCDIR`
  - analysis files (pvx, ats, lpc, het, cv) to `SADIR`
- **Manage Files...** - the apeCsound folder. On desktop it opens in Finder / Explorer; on iOS see "Files on iOS".
- **Restore Examples** - copies the factory examples again (your files in `CSD/User` are not touched).

# Sessions
The `.csd` file on disk is the reference. A DAW project stores the path of the linked file, an embedded copy of the code and of the parameter mapping, and the parameter values.
- **Save** writes the code and the parameter structure (an `<apeCsoundParams>` block after `</CsoundSynthesizer>`).
- Unsaved changes are protected by a dialog before Load or Init.
- If the linked file is missing when a project reopens, the embedded copy is used and a yellow bar offers **Relocate...** and **Save As...**.
- The editor shows only the `<CsoundSynthesizer>` block; anything before or after it (Cabbage, CsoundQt panels) is kept.

# The apeCsound folder
`~/Documents/apeCsound` on desktop; on iOS a folder shared by the app and the AUv3 plug-in.
- `CSD/Examples` - factory examples. `CSD/User` - your sessions.
- `SSDIR` - audio files to read: `diskin2 "loop.wav"`, GEN01. Use just the file name.
- `SFDIR` - files written by Csound (`fout`, `-o`) without a path.
- `SADIR` - analysis files (pvoc, ATS, lpc). `INCDIR` - `#include` files, e.g. shared UDOs.
- Csound first looks in the folder of the linked `.csd`, then in these folders.
- `--env:SSDIR=...` (or SFDIR, SADIR, INCDIR) in `<CsOptions>` overrides them for that `.csd`.
## Config > Extra Folders
Folders of your choice, used together with the apeCsound ones. They take effect at the next Apply.
- **Extra Samples (SSDIR)**, **Extra Analysis Files (SADIR)**, **Extra Include Files (INCDIR)** - one more folder to search.
- **Output Files (SFDIR)** - where Csound writes, instead of `apeCsound/SFDIR`.
- Each entry shows the path, **Change Folder...**, **Reveal in Finder** (desktop) and **Remove (use default)**.
- On iOS the folder is chosen with the system picker (On My iPad, iCloud Drive...). With Output Files set there, recordings appear directly in the Files app.
## Files on iOS
On iOS the apeCsound folder is not visible in the Files app. **Load**, **Open Example** and **Save as** use apeCsound's own browser:
- tap a folder to enter it, a file to open it;
- in Save as, type the name and tap **Save** (tap **Replace** to overwrite);
- the bin icon deletes a file in `CSD/User` (tap **Delete** to confirm);
- **Other Locations...** opens the system picker (iCloud Drive and other providers).
**Manage Files...** shows the whole apeCsound folder:
- tap files to select them, then **Export** copies them to a folder chosen with the system picker (all files of the folder if none is selected);
- **Import Files Here...** copies files into the open folder, of any type;
- the bin icon deletes a file (factory examples excepted).

# Code editor
- **Auto-indent** for `instr`/`endin`, `opcode`/`endop`, `if`/`then`, loops. Tab = 4 spaces.
- **Right-click** (long press on touch) - Cut, Copy, Paste, Delete, Select All, Undo, Redo, **Indent**, **Comment / Uncomment**.
- **Help bar** - syntax and description of the opcode under the caret, also for the UDOs of the file.
- **Autocompletion** - type two letters: Up/Down to choose, Tab or Enter to accept, Esc to close.
- **Config > Modern Syntax in Help** - shows the Csound 7 functional form, e.g. `ares:a = oscil(xamp, xcps)`.
- **Undo / Redo** - one history shared by the editor and the Parameters panel; it survives closing the plug-in window.
## Evaluate (live coding)
**Shift+Return**, or **Evaluate** in the right-click menu, sends code to the engine *while it keeps running*, without the full recompile of Apply: the selection, or - with no selection - the `instr` / `opcode` block around the caret, or the current line. The lines flash when sent.
- Orchestra code is compiled and merged: an `instr` that already exists is **redefined** (notes already playing finish with the old version, new notes use the new one); global code (`gkfb = 0.9`, `gitab = ftgen(...)`, `schedule(3, 0, 1)`) runs at once.
- Lines that start with a score statement (`i 3 0 2`, `f 1 0 1024 10 1`) are sent as real-time events.
- The **Evaluate line** does the same for a quick event or a one-liner: click `>_` at the right of the help bar (or **Cmd+E**), type, Return; Up/Down recall the history, Esc goes back to the help bar.
- Nothing you evaluate is saved anywhere else: the file is the reference. At the next Apply (or when the host reloads the session) only the text in the editor counts, so keep what you want in the code.
- Errors are reported in the console; the engine must be running (Apply first).
## Errors
When Apply fails, the line reported by Csound gets a red dot, a red background and a wavy underline; with the caret on it, the help bar shows the message.
- The mark goes away when you edit the line, and all marks are cleared at the next Apply.
- Csound stops at the first syntax error: fix it and Apply again to see the next one.
- `INIT ERROR` / `PERF ERROR` while running are marked too. Errors in macros or `#include` files stay in the console.

# Parameters
Every instance offers the DAW **144 fixed parameters**: 64 Float, 32 Int, 32 Bool and 16 Choice. The DAW always sees "Float 1 ... Float 64" and so on; in the Parameters panel you give each one a Csound channel name and its range.
- **Float** (min, max, default, skew, step), **Int** (min, max, default), **Bool** (default), **Choice** (up to 16 options; the channel gets 1..N, as in Cabbage).
- **+** on the panel title adds one. Each card has an **edit** (pencil) button that opens its configuration (channel name, range, default, skew, step, options) - it becomes a check mark while editing: press it to apply and go back to the control - while a slider is being edited, the slider itself stays visible under the name and follows Min/Max/Init/Skew/Step as you type - and a **...** menu: **Copy chnget line** (the `chnget` line with a configuration comment, to paste in the code), **Reset to Init Value**, **Randomize** (a random value within the range), **Show in Code**, **Remove** (asks for confirmation; can be undone). The **+** menu has **Reset to INIT Values** and **Randomize All** for the whole panel. Code and panel are linked: put the caret on a `chnget "name"` line and the card of that channel flashes briefly; **Show in Code** in the **...** menu of a card jumps to its `chnget`. One card is in edit mode at a time - opening another closes the previous one - and it is outlined in the colour of its type. Toggles and menus are single-line cards, sliders take two lines.
- **Moved from outside**: when a value changes without you touching the control - DAW automation, `chnset` from the code, a snapshot recall - a dot in the colour of the type flashes next to the parameter name.
- **Validation**: in edit mode a wrong value turns its field red and the caption says why (Min not below Max, Init outside the range, Skew not positive, Step larger than the range, no options, channel name already used by another parameter). The value is kept as typed: fix it before saving.
- The code reads them with `chnget`. `chnset` on the same channel moves the parameter (slider and DAW automation).
- **Drag a card into the code** - drag the handle on its left and drop it on a line of the editor to insert `kCutoff chnget "Cutoff"` with a comment describing the parameter. **Copy chnget line** in the card menu puts the same text in the clipboard.
- A parameter channel is a control channel: do not use the same name with audio variables (`a...`) - the console warns about it.
## Groups
By default the panel lists the parameters by type: all the Float sliders, then Int, Bool and Choice. A comment in the code groups them the way you want, so that the controls of one instrument - its sliders, toggles and menus together - stay under one label:
- `; @group Synth` on its own line, before the `chnget` lines. Every parameter read below it belongs to "Synth", until the next `@group`, or `endin` / `endop`. Also `;@group` and `// @group`; names can contain spaces.
- A Bool or Choice parameter goes where its `chnget` is: next to the sliders of its instrument, not at the end of the list.
- The group of a parameter is decided by the **first** `chnget` of its channel in the code. A channel read in several instruments goes where it appears first - put that line under the `@group` you prefer.
- The same name in several places joins the parameters in one group (e.g. a "Master" group spread over two instruments).
- Groups appear in the order of the code, parameters in the order of their `chnget`. Parameters without a group, or whose channel is not read by any `chnget` (misspelt name, name built at run time), go to **Other**, at the end. Without any `@group` the list stays flat, as before.
- Each group is a frame with a teal header: tap it to collapse or expand the group. **Expand All Groups** and **Collapse All Groups** are in the panel's + menu.
- Groups follow the **compiled** code: they update at Apply and Load, not while typing. They only change the order in the panel: the DAW still sees the parameters by their fixed indices.
- Example: `; @group Synth` / `kcut chnget "Cutoff"` / `iwave chnget "Wave"` / `; @group Delay` / `ktime chnget "Time"` - see `Test/GroupsSnapshotsTest.csd` in the examples.
## Snapshots
The camera button on the right of the Parameters title bar stores and recalls the **values** of all the parameters.
- **Save Snapshot...** - asks for a name and stores the current values.
- Tap a snapshot in the **Recall** list to bring every parameter back to those values (sliders, DAW automation).
- **Edit Snapshots...** - for each snapshot: **Overwrite with Current Values**, **Rename...**, **Delete**.
- Snapshots are saved in the `.csd` (inside `<apeCsoundParams>`) and in the DAW project, so they travel with the session. Saving, renaming and deleting can be undone; recalling only changes the values.
## Cabbage and CsoundQt files
Their widgets (sliders, knobs, checkboxes, menus, XY...) become parameters when the file is loaded. The rest of their GUI is ignored, and the code is not changed:
- replace `invalue` / `cabbageGetValue` with `chnget`, `outvalue` / `cabbageSet...` with `chnset`;
- FLTK, Python, Lua and opcodes not built into apeCsound do not work;
- check the console after Apply. Some files need manual changes, a few cannot work.

# Audio, MIDI and channels
- `sr` always follows the DAW. `ksmps` is respected and equals the plug-in latency (compensated by the DAW): keep it small (16-64) for live input.
- **Channels** - by default Csound follows the track: `nchnls` / `nchnls_i` are the track's outputs / inputs (at least 2 outputs). Tracks can have 1-16 outputs and 0-16 inputs.
- **Config > Follow CSD nchnls** - uses the `nchnls` / `nchnls_i` of the `.csd` instead, so the code behaves the same on any track (missing channels are silent). Saved with the project.
- **MIDI** - notes and controllers reach Csound; MIDI written by Csound (`midiout`) goes back to the DAW.
- **OSC** - the OSC opcodes (`oscinit`, `osclisten`, `oscsend`...) are built in.

## Host transport channels
The DAW transport is available to the code as control channels, with the same names Cabbage uses, so Cabbage files that read them work unchanged. Updated once per audio block:
- `HOST_BPM` - tempo; `TIME_SIG_NUM` / `TIME_SIG_DENOM` - time signature
- `IS_PLAYING`, `IS_RECORDING` - 1 or 0
- `HOST_PPQ_POS` - position in quarter notes (beats); `HOST_PPQ_BAR_START` - beat of the last bar start
- `TIME_IN_SECONDS`, `TIME_IN_SAMPLES` - position of the playhead
Read them with `chnget`, e.g. a tempo-synced delay: `kdel = 60 / chnget:k("HOST_BPM") * 0.75`. In the standalone app, or in a host without transport, they stay at 0 except `HOST_BPM` = 120 and 4/4.

## Audio I/O opcodes
`nchnls` / `nchnls_i` follow the track, so write the code with the **per-channel** opcodes: `aL inch 1`, `aR inch 2`, `outch 1, aL, 2, aR`. They work with any channel count: on a mono track `outch 2` is simply dropped, on a 6-channel track `outch 3`...`outch 6` reach the extra channels. The fixed-width opcodes (`outs`, `ins`, `out`, `outq`, `inq`...) fail on a track with fewer channels and ignore the extra ones; apeCsound warns in the console when the count does not match.

# Console and Graphs
- **Console** - Csound messages, compile errors and apeCsound notes. Repeated messages are summarised to keep the DAW responsive. Clear empties it; it is also cleared at every Apply.
- **Graphs** (menu > Show Graphs) - function tables (f statements, `ftgen`) and `display` / `dispfft` signals, in a panel docked at the bottom. Cleared at every Apply.

# Shortcuts
Cmd on macOS, Ctrl on Windows and Linux.
- **Cmd+Return** - Apply, **Shift+Return** - Evaluate selection / block / line, **Cmd+E** - Evaluate line in the help bar
- **Cmd+S** Save, **Cmd+Shift+S** Save as, **Cmd+O** Load, **Cmd+N** Init Session Clear
- **Cmd+F** - Find / Replace
- **Cmd+1 / 2 / 3** - Parameters, Console, Graphs
- **Cmd+= / Cmd+-** - larger / smaller text
- **Cmd+Z / Cmd+Shift+Z** - Undo / Redo
- **Cmd+/** - comment / uncomment, **Ctrl+I** - re-indent, **Cmd+[ / ]** - shift left / right
- **Esc** - close the autocompletion list, the Find bar or this guide
- In a DAW the plug-in only gets the keys the host passes on (REAPER: "Send all keyboard input to plug-in" in the FX window menu).
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

//==============================================================================
// "Audio/MIDI Settings..." - vedi il commento su AudioSettingsView in PluginEditor.h.
CsoundAudioProcessorEditor::AudioSettingsView::DarkLookAndFeel::DarkLookAndFeel()
    : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::ColourScheme (
          kAboutCardBg,        // windowBackground
          kAboutRowBg,         // widgetBackground
          kAboutCardBg,        // menuBackground
          kAboutBorder,        // outline
          kAboutText,          // defaultText
          kAboutAccent,        // defaultFill
          juce::Colour (0xff0b1116), // highlightedText
          kAboutAccent,        // highlightedFill
          kAboutText))         // menuText
{
    // Bottoni (Test, Control Panel, Reset Device) come i bottoni teal dell'app.
    setColour (juce::TextButton::buttonColourId,   kAboutAccent);
    setColour (juce::TextButton::buttonOnColourId, kAboutAccent.darker (0.2f));
    setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff0b1116));
    setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff0b1116));

    setColour (juce::Label::textColourId,           kAboutTextMuted);
    setColour (juce::ComboBox::backgroundColourId,  kAboutRowBg);
    setColour (juce::ComboBox::textColourId,        kAboutText);
    setColour (juce::ComboBox::outlineColourId,     kAboutBorder);
    setColour (juce::ComboBox::arrowColourId,       kAboutTextMuted);
    setColour (juce::ComboBox::focusedOutlineColourId, kAboutAccent);
    setColour (juce::PopupMenu::backgroundColourId,            kAboutCardBg);
    setColour (juce::PopupMenu::textColourId,                  kAboutText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAboutRowHover);
    setColour (juce::PopupMenu::highlightedTextColourId,       kAboutText);
    setColour (juce::ListBox::backgroundColourId,   kAboutRowBg);
    setColour (juce::ListBox::outlineColourId,      kAboutBorder);
    setColour (juce::ListBox::textColourId,         kAboutText);
    setColour (juce::TextEditor::highlightColourId, kAboutAccent.withAlpha (0.25f)); // righe selezionate nelle liste
    setColour (juce::ToggleButton::textColourId,         kAboutText);
    setColour (juce::ToggleButton::tickColourId,         kAboutAccent);
    setColour (juce::ToggleButton::tickDisabledColourId, kAboutBorder);
    setColour (juce::ScrollBar::thumbColourId,      kAboutAccent.withAlpha (0.55f));
}

void CsoundAudioProcessorEditor::AudioSettingsView::Content::resized()
{
    if (settings != nullptr)
        settings->setBounds (0, 0, getWidth(), settings->getIdealHeight (getWidth()));
}

void CsoundAudioProcessorEditor::AudioSettingsView::Content::updateHeight()
{
    if (settings == nullptr)
        return;

    const int wanted = settings->getIdealHeight (getWidth());

    if (wanted != getHeight())
        setSize (getWidth(), wanted);
}

CsoundAudioProcessorEditor::AudioSettingsView::Card::Card()
{
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false); // solo verticale
   #if JUCE_IOS
    viewport.setScrollBarThickness (6);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all); // scorrimento col dito anche partendo da una riga
   #endif
    addAndMakeVisible (viewport);
}

void CsoundAudioProcessorEditor::AudioSettingsView::Card::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (kAboutCardBg);
    g.fillRoundedRectangle (bounds, 16.0f);
    g.setColour (kAboutBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 15.5f, 1.0f);

    g.setColour (kAboutText);
    g.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    g.drawFittedText ("Audio/MIDI Settings", 24, 16, getWidth() - 48 - 36, 28, juce::Justification::centredLeft, 1);

    g.setColour (kAboutBorder);
    g.drawLine (16.0f, 54.5f, (float) getWidth() - 16.0f, 54.5f, 1.0f);

    const auto close = closeButtonBounds().toFloat();
    g.setColour (kAboutTextMuted);
    juce::Path x;
    x.startNewSubPath (close.getX() + 9.0f, close.getY() + 9.0f); x.lineTo (close.getRight() - 9.0f, close.getBottom() - 9.0f);
    x.startNewSubPath (close.getRight() - 9.0f, close.getY() + 9.0f); x.lineTo (close.getX() + 9.0f, close.getBottom() - 9.0f);
    g.strokePath (x, juce::PathStrokeType (1.8f));
}

juce::Rectangle<int> CsoundAudioProcessorEditor::AudioSettingsView::Card::closeButtonBounds() const
{
    return { getWidth() - 16 - 28, 14, 28, 28 };
}

void CsoundAudioProcessorEditor::AudioSettingsView::Card::resized()
{
    viewport.setBounds (getLocalBounds().withTrimmedTop (62).reduced (12, 0).withTrimmedBottom (16));

    const int contentWidth = viewport.getMaximumVisibleWidth();
    content.setSize (contentWidth, juce::jmax (content.getHeight(), 1));
    content.resized();
    content.updateHeight();
}

void CsoundAudioProcessorEditor::AudioSettingsView::Card::mouseUp (const juce::MouseEvent& e)
{
    if (closeButtonBounds().contains (e.getPosition()) && onClose)
        onClose();
}

CsoundAudioProcessorEditor::AudioSettingsView::AudioSettingsView()
{
    setWantsKeyboardFocus (true);
    card.setLookAndFeel (&darkLookAndFeel); // ereditato da viewport, selettore e popup delle combo
    card.onClose = [this] { hide(); };
    addAndMakeVisible (card);
}

CsoundAudioProcessorEditor::AudioSettingsView::~AudioSettingsView()
{
    card.content.settings.reset();
    card.setLookAndFeel (nullptr);
}

void CsoundAudioProcessorEditor::AudioSettingsView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.45f));
}

void CsoundAudioProcessorEditor::AudioSettingsView::resized()
{
   #if JUCE_IOS
    // iOS: scheda quasi a tutto schermo (righe grandi, contenuto lungo).
    auto area = getLocalBounds().reduced (juce::jmin (24, getWidth() / 20), juce::jmin (16, getHeight() / 20));
    card.setBounds (area.withSizeKeepingCentre (juce::jmin (680, area.getWidth()), area.getHeight()));
   #else
    auto area = getLocalBounds().reduced (juce::jmin (32, getWidth() / 12), juce::jmin (24, getHeight() / 12));
    card.setBounds (area.withSizeKeepingCentre (juce::jmin (cardMaxWidth, area.getWidth()),
                                                juce::jmin (cardMaxHeight, area.getHeight())));
   #endif
}

void CsoundAudioProcessorEditor::AudioSettingsView::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent == this)
        hide();
}

bool CsoundAudioProcessorEditor::AudioSettingsView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }

    return false;
}

void CsoundAudioProcessorEditor::AudioSettingsView::show (juce::AudioDeviceManager& deviceManager, int maxInputs, int maxOutputs,
                                                          juce::Value* muteInput, std::function<void()> onSetupChanged)
{
    auto& content = card.content;

    // Pannello nuovo a ogni apertura (rilegge dispositivi e canali attuali):
    // stesso componente su macOS, iOS, Windows e Linux (vedi CsoundAudioSettings).
    content.settings.reset();
    content.settings = std::make_unique<CsoundAudioSettings> (deviceManager, maxInputs, maxOutputs, muteInput);
    content.settings->onSetupChanged = std::move (onSetupChanged);
    content.settings->onLayoutChanged = [this]
    {
        // Pagina diversa: altezza nuova e ritorno in cima.
        card.content.updateHeight();
        card.content.resized();
        card.viewport.setViewPosition (0, 0);
    };
    content.addAndMakeVisible (*content.settings);

    card.resized();
    card.viewport.setViewPosition (0, 0);

    toFront (true);
    juce::Desktop::getInstance().getAnimator().fadeIn (this, 140);
    grabKeyboardFocus();
}

void CsoundAudioProcessorEditor::AudioSettingsView::hide()
{
    juce::Desktop::getInstance().getAnimator().fadeOut (this, 120);

    // Rilascia il selettore (timer, meter di livello, listener sul device
    // manager): alla prossima apertura se ne crea uno nuovo.
    card.content.settings.reset();
}
