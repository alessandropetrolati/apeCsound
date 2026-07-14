#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CsoundTokeniser.h"
#include "CsoundCodeEditor.h"

/**
    Editor del plugin/standalone (juce::AudioProcessorEditor): editor con
    syntax highlighting + console log + Run/Stop, agganciata a
    CsoundAudioProcessor. JUCE la istanzia da
    CsoundAudioProcessor::createEditor() sia nella Standalone app sia
    quando/se il plugin verra' aperto in una DAW.

    La logica di binding dei widget (GUI designer, eventualmente da
    rivalutare in futuro) andra' agganciata a
    CsoundAudioProcessor::setControlChannel / getControlChannel, gia'
    pronti qui tramite `audioProcessor`.
*/
class CsoundAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private CsoundAudioProcessor::Listener
{
public:
    explicit CsoundAudioProcessorEditor (CsoundAudioProcessor& p);
    ~CsoundAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    // CsoundAudioProcessor::Listener
    void csoundMessageReceived (const juce::String& message) override;
    void csoundEngineStarted() override;
    void csoundEngineStopped() override;

    void appendToLog (const juce::String& text);

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    CsoundAudioProcessor& audioProcessor;

    juce::CodeDocument document;
    CsoundTokeniser tokeniser;
    CsoundCodeEditor editor { document, &tokeniser };

    juce::TextEditor logConsole;

    juce::TextButton runButton   { "Run" };
    juce::TextButton stopButton  { "Stop" };
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundAudioProcessorEditor)
};
