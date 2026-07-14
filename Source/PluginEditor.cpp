#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CsoundAudioProcessorEditor::CsoundAudioProcessorEditor (CsoundAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p)
{
    document.replaceAllContent (audioProcessor.getCsdText());
    document.clearUndoHistory();

    editor.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain));
    addAndMakeVisible (editor);

    logConsole.setMultiLine (true);
    logConsole.setReadOnly (true);
    logConsole.setScrollbarsShown (true);
    logConsole.setCaretVisible (false);
    logConsole.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1a1a1a));
    logConsole.setColour (juce::TextEditor::textColourId, juce::Colour (0xffd0d0d0));
    logConsole.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    addAndMakeVisible (logConsole);

    runButton.onClick = [this]
    {
        appendToLog ("--- Compilazione ed esecuzione ---");
        audioProcessor.compileAndStart (document.getAllContent());
    };
    addAndMakeVisible (runButton);

    stopButton.onClick = [this]
    {
        audioProcessor.stopEngine();
    };
    addAndMakeVisible (stopButton);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    // Riflette lo stato attuale del motore (utile se l'editor viene chiuso
    // e riaperto mentre il plugin e' gia' in esecuzione).
    const bool running = audioProcessor.isEngineRunning();
    runButton.setEnabled (! running);
    stopButton.setEnabled (running);
    statusLabel.setText (running ? "In esecuzione" : "Fermo", juce::dontSendNotification);

    audioProcessor.addListener (this);

    setResizable (true, true);
    setSize (960, 680);
}

CsoundAudioProcessorEditor::~CsoundAudioProcessorEditor()
{
    audioProcessor.removeListener (this);
}

void CsoundAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void CsoundAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto toolbar = area.removeFromTop (36);
    runButton.setBounds (toolbar.removeFromLeft (90));
    toolbar.removeFromLeft (6);
    stopButton.setBounds (toolbar.removeFromLeft (90));
    toolbar.removeFromLeft (12);
    statusLabel.setBounds (toolbar);

    area.removeFromTop (8);

    auto logArea = area.removeFromBottom (180);
    area.removeFromBottom (8);

    editor.setBounds (area);
    logConsole.setBounds (logArea);
}

void CsoundAudioProcessorEditor::csoundMessageReceived (const juce::String& message)
{
    appendToLog (message);
}

void CsoundAudioProcessorEditor::csoundEngineStarted()
{
    runButton.setEnabled (false);
    stopButton.setEnabled (true);
    statusLabel.setText ("In esecuzione", juce::dontSendNotification);
}

void CsoundAudioProcessorEditor::csoundEngineStopped()
{
    runButton.setEnabled (true);
    stopButton.setEnabled (false);
    statusLabel.setText ("Fermo", juce::dontSendNotification);
}

void CsoundAudioProcessorEditor::appendToLog (const juce::String& text)
{
    logConsole.moveCaretToEnd();
    logConsole.insertTextAtCaret (text + "\n");
}
