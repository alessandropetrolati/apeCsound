#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
CsoundAudioProcessorEditor::CsoundAudioProcessorEditor (CsoundAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p)
{
    setLookAndFeel (&lookAndFeel);

    document.replaceAllContent (audioProcessor.getCsdText());
    document.clearUndoHistory();

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

    // Il nome del Component e' come CsoundLookAndFeel sceglie quale icona
    // disegnare (vedi getIconPathForButtonName) - non ha altro effetto.
    // Il motore Csound e' sempre in esecuzione (parte da solo in
    // prepareToPlay): questo bottone non "avvia" nulla, si limita a
    // rimpiazzare il .csd corrente con quello appena modificato nell'editor.
    applyButton.setName ("apply");
    applyButton.onClick = [this]
    {
        appendToLog ("--- Applying edited .csd ---");
        audioProcessor.compileAndStart (document.getAllContent());
    };
    addAndMakeVisible (applyButton);

    clearConsoleButton.setName ("clear");
    clearConsoleButton.onClick = [this] { logConsole.clear(); };
    addAndMakeVisible (clearConsoleButton);

    // Mostra/nasconde il pannello flottante per rinominare i canali Csound
    // dei 64 slot apvts (float/int/bool/choice) e definirne range/curva -
    // vedi CsoundParameterEditor.h/.cpp e toggleParameterPanel().
    paramsButton.setName ("params");
    paramsButton.onClick = [this] { toggleParameterPanel(); };
    addAndMakeVisible (paramsButton);

    // Pannello nascosto finche' non si preme paramsButton - nessun velo
    // dietro di esso: editor/consolle restano sempre interagibili.
    addChildComponent (parameterPanel);
    parameterPanel.onCloseButtonClicked = [this] { toggleParameterPanel(); };

    // Save/Load Session su file (.csd), indipendenti dal progetto della
    // DAW - vedi il commento su saveSessionButton/loadSessionButton in
    // PluginEditor.h sul perche'.
    saveSessionButton.setName ("saveSession");
    saveSessionButton.onClick = [this] { promptSaveSession(); };
    addAndMakeVisible (saveSessionButton);

    loadSessionButton.setName ("loadSession");
    loadSessionButton.onClick = [this] { promptLoadSession(); };
    addAndMakeVisible (loadSessionButton);

    audioProcessor.addListener (this);

    // false = niente ResizableCornerComponent in basso a destra: il
    // ridimensionamento resta comunque possibile, ma tramite il bordo
    // nativo della finestra (host/wrapper Standalone), non una maniglia
    // disegnata da noi.
    setResizable (true, false);
    setSize (960, 680);

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
    setLookAndFeel (nullptr);
    audioProcessor.removeListener (this);
}

void CsoundAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // La toolbar e' una barra vera e propria: sfondo bianco (come gli altri
    // pannelli) e una sottile linea di separazione in basso, non solo due
    // bottoni appoggiati sullo sfondo generale.
    g.setColour (juce::Colours::white);
    g.fillRect (toolbarBounds);

    g.setColour (juce::Colour (0xffd7dee3));
    g.drawLine ((float) toolbarBounds.getX(),     (float) toolbarBounds.getBottom() - 0.5f,
                (float) toolbarBounds.getRight(), (float) toolbarBounds.getBottom() - 0.5f, 1.0f);
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
    const auto clearWidth = clearConsoleButton.getBestWidthForHeight (toolbar.getHeight())
                           + CsoundLookAndFeel::getIconAllowance (clearConsoleButton.getName());
    const auto paramsWidth = paramsButton.getBestWidthForHeight (toolbar.getHeight())
                            + CsoundLookAndFeel::getIconAllowance (paramsButton.getName());
    const auto saveWidth = saveSessionButton.getBestWidthForHeight (toolbar.getHeight())
                          + CsoundLookAndFeel::getIconAllowance (saveSessionButton.getName());
    const auto loadWidth = loadSessionButton.getBestWidthForHeight (toolbar.getHeight())
                          + CsoundLookAndFeel::getIconAllowance (loadSessionButton.getName());

    applyButton.setBounds (toolbar.removeFromLeft (applyWidth));
    toolbar.removeFromLeft (8);
    clearConsoleButton.setBounds (toolbar.removeFromLeft (clearWidth));
    toolbar.removeFromLeft (8);
    paramsButton.setBounds (toolbar.removeFromLeft (paramsWidth));
    toolbar.removeFromLeft (8);
    saveSessionButton.setBounds (toolbar.removeFromLeft (saveWidth));
    toolbar.removeFromLeft (8);
    loadSessionButton.setBounds (toolbar.removeFromLeft (loadWidth));

    // Niente inset laterali e niente spazio tra editor e console: solo lo
    // spazio verticale tra toolbar ed editor resta. Editor e consolle
    // occupano SEMPRE questa stessa area, a prescindere dal pannello
    // parametri (vedi sotto): non vengono piu' sostituiti da esso.
    area.removeFromTop (8);

    auto bottomArea = area.removeFromBottom (180);
    auto helpBarArea = area.removeFromBottom (opcodeHelpBarHeight);

    editor.setBounds (area);
    opcodeHelpBar.setBounds (helpBarArea);
    logConsole.setBounds (bottomArea);

    // Il pannello e' una "finestra" spostabile (vedi CsoundParameterMappingPanel::
    // mouseDown/mouseDrag): la posizioniamo centrata SOLO la prima volta che
    // viene mostrata, non ad ogni resized() - altrimenti ogni ridimensionamento
    // della finestra del plugin (o anche solo il resized() scatenato da
    // toggleParameterPanel() stesso) la rimetterebbe al centro, annullando
    // un trascinamento manuale dell'utente. Dopo il primo posizionamento ci
    // limitiamo a tenerla dentro i bordi se la finestra si e' ristretta.
    const int panelWidth  = juce::jmin (CsoundParameterMappingPanel::preferredWidth,  getWidth()  - 48);
    const int panelHeight = juce::jmin (CsoundParameterMappingPanel::preferredHeight, getHeight() - 48);

    if (! parameterPanelPositioned)
    {
        parameterPanel.setBounds (getLocalBounds().withSizeKeepingCentre (panelWidth, panelHeight));

        if (parameterPanel.isVisible())
            parameterPanelPositioned = true;
    }
    else
    {
        auto bounds = parameterPanel.getBounds().withSize (panelWidth, panelHeight);
        bounds.setPosition (juce::jlimit (0, juce::jmax (0, getWidth()  - bounds.getWidth()),  bounds.getX()),
                             juce::jlimit (0, juce::jmax (0, getHeight() - bounds.getHeight()), bounds.getY()));
        parameterPanel.setBounds (bounds);
    }
}

void CsoundAudioProcessorEditor::toggleParameterPanel()
{
    showingParameterPanel = ! showingParameterPanel;

    // "Finestra" spostabile, non overlay: editor/consolle restano sempre al
    // loro posto e sempre interagibili, il pannello compare/scompare sopra
    // di essi senza nulla che li scurisca o blocchi i click.
    parameterPanel.setVisible (showingParameterPanel);

    if (showingParameterPanel)
        parameterPanel.toFront (true); // porta anche la tastiera sul pannello

    paramsButton.setButtonText (showingParameterPanel ? "Hide parameters" : "Parameters");

    resized();
    repaint();
}

void CsoundAudioProcessorEditor::promptSaveSession()
{
    // Un .csd VERO, non un formato proprietario: vedi il commento su
    // CsoundAudioProcessor::saveSessionToFile in PluginProcessor.h - il
    // codice resta testo Csound puro, il mapping dei parametri va in
    // appendice dentro <CsoundStudioParams>.
    const auto startingFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                   .getChildFile ("CsoundStudio Session.csd");

    activeFileChooser = std::make_unique<juce::FileChooser> (
        "Save CsoundStudio Session (.csd)...", startingFile, "*.csd");

    activeFileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();

            if (file == juce::File{})
                return; // annullato dall'utente

            if (! file.hasFileExtension ("csd"))
                file = file.withFileExtension ("csd");

            const bool ok = audioProcessor.saveSessionToFile (file);
            appendToLog (ok ? ("--- Session saved to " + file.getFullPathName() + " ---")
                             : "--- Failed to save session (file not writable?) ---");
        });
}

void CsoundAudioProcessorEditor::promptLoadSession()
{
    const auto startingDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    activeFileChooser = std::make_unique<juce::FileChooser> (
        "Load CsoundStudio Session (.csd)...", startingDir, "*.csd");

    activeFileChooser->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();

            if (file == juce::File{})
                return; // annullato dall'utente

            if (audioProcessor.loadSessionFromFile (file))
            {
                // L'editor di codice e il pannello parametri hanno il
                // proprio stato locale (document/righe), costruito a
                // partire dal processor - vanno rilette esplicitamente
                // ora che loadSessionFromFile ha sostituito quello stato,
                // altrimenti continuerebbero a mostrare la sessione
                // precedente finche' non si cambia tab/si riapre il pannello.
                document.replaceAllContent (audioProcessor.getCsdText());
                document.clearUndoHistory();
                parameterPanel.refreshAllFromProcessor();

                appendToLog ("--- Session loaded from " + file.getFullPathName() + " ---");
            }
            else
            {
                appendToLog ("--- Failed to load session (empty or unreadable file?) ---");
            }
        });
}

void CsoundAudioProcessorEditor::OpcodeHelpBar::setHelpText (const juce::String& syntax, const juce::String& description)
{
    syntaxText = syntax;
    descriptionText = description;
    repaint();
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
        g.drawText ("Click an opcode, or type one, for inline help.", area, juce::Justification::centredLeft, true);
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
