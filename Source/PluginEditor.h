#pragma once

#include <JuceHeader.h>
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
    plugin): mostra invece, al POSTO della consolle (stessa area, non di
    fianco al codice), CsoundParameterMappingPanel, dove si definisce il
    "rename" (nome canale Csound) e il range/curva dei 32 slot apvts - il
    codice resta visibile sopra. I VALORI restano affidati all'automazione
    host o a una UI dedicata futura, qui si editano solo i metadata per
    slot.

    Trascinando la maniglia "#N" di una riga del pannello sull'editor di
    codice si inserisce automaticamente un chnget per quel canale (vedi
    CsoundCodeEditor e CsoundParameterMappingPanel::ParamRow) - per questo
    l'intero editor eredita anche juce::DragAndDropContainer, il mixin che
    coordina drag and drop tra componenti della stessa finestra.
*/
class CsoundAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          public juce::DragAndDropContainer,
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

    // Pannello del mapping parametri (rename canale + range/curva per
    // slot): nascosto di default, sostituisce logConsole (stessa identica
    // area, vedi resized()) quando si preme paramsButton - vedi
    // showingParameterPanel e toggleParameterPanel() in PluginEditor.cpp.
    // Solo uno dei due (pannello o consolle) e' visibile per volta.
    CsoundParameterMappingPanel parameterPanel { audioProcessor };
    bool showingParameterPanel = false;
    void toggleParameterPanel();

    // La toolbar e' una barra dedicata (sfondo + separatore disegnati in
    // paint(), bounds calcolati in resized()) che contiene questi due
    // bottoni, con icone disegnate da CsoundLookAndFeel in base al nome
    // (Component::setName) assegnato nel costruttore.
    juce::Rectangle<int> toolbarBounds;
    juce::TextButton applyButton        { "Apply" };
    juce::TextButton clearConsoleButton { "Clear console" };
    juce::TextButton paramsButton       { "Parameters..." };

    static constexpr int toolbarHeight = 44;

    CsoundLookAndFeel lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundAudioProcessorEditor)
};
