#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    audioProcessor.addListener (this);

    // false = niente ResizableCornerComponent in basso a destra: il
    // ridimensionamento resta comunque possibile, ma tramite il bordo
    // nativo della finestra (host/wrapper Standalone), non una maniglia
    // disegnata da noi.
    setResizable (true, false);
    setSize (960, 680);
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

    applyButton.setBounds (toolbar.removeFromLeft (applyWidth));
    toolbar.removeFromLeft (8);
    clearConsoleButton.setBounds (toolbar.removeFromLeft (clearWidth));

    // Niente inset laterali e niente spazio tra editor e console: solo lo
    // spazio verticale tra toolbar ed editor resta.
    area.removeFromTop (8);

    auto logArea = area.removeFromBottom (180);

    editor.setBounds (area);
    logConsole.setBounds (logArea);
}

void CsoundAudioProcessorEditor::csoundMessageReceived (const juce::String& message)
{
    appendToLog (message);
}

void CsoundAudioProcessorEditor::csoundEngineStarted()
{
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
