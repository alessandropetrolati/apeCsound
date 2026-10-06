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
    curva per i parametri float, default per i bool, etichette/indice per i
    choice) dei 64 slot apvts, isolati in tab separate dentro il pannello
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
                                          private CsoundAudioProcessor::Listener,
                                          private juce::CodeDocument::Listener
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

    // juce::CodeDocument::Listener: CsoundCodeEditor (vedi "editor" sotto)
    // ha GIA' un proprio listener privato sullo stesso document (per l'help
    // inline) - juce::CodeDocument supporta piu' listener indipendenti sullo
    // stesso documento, quindi questo secondo qui non interferisce, serve
    // solo a scoprire ogni modifica del testo per updateApplyButtonDirtyState()
    // sotto. Non ci interessa il dettaglio dell'inserimento/cancellazione,
    // solo IL FATTO che qualcosa e' cambiato.
    void codeDocumentTextInserted (const juce::String&, int) override { updateApplyButtonDirtyState(); }
    void codeDocumentTextDeleted (int, int) override                  { updateApplyButtonDirtyState(); }

    // Confronta il testo ATTUALE dell'editor con l'ultimo testo applicato/
    // caricato nel processor (audioProcessor.getCsdText()): se sono diversi,
    // il bottone Apply viene circondato da un bordo rosso (vedi
    // CsoundLookAndFeel::drawButtonBackground, che legge la proprieta'
    // dinamica "pendingChanges" sul bottone) per segnalare che il codice e'
    // stato modificato e non coincide piu' con quello in esecuzione/salvato
    // - chiamata ad ogni modifica del document (sopra) e dopo ogni Apply/
    // Load Session, quando i due testi tornano a coincidere.
    void updateApplyButtonDirtyState();

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

    // Pannello del mapping parametri (rename canale + metadata per slot):
    // nascosto di default, mostrato come "finestra" spostabile (vedi il
    // commento in testa alla classe) quando si preme paramsButton - vedi
    // showingParameterPanel e toggleParameterPanel() in PluginEditor.cpp.
    // editor/logConsole restano SEMPRE al loro posto e sempre interagibili:
    // nessun velo li copre, parameterPanel si limita a comparire sopra di
    // essi nello z-order. La posizione iniziale (centrata) viene impostata
    // solo la prima volta che il pannello viene mostrato - vedi
    // parameterPanelPositioned - cosi' un trascinamento dell'utente non
    // viene annullato dai resized() successivi (es. ridimensionando la
    // finestra del plugin).
    CsoundParameterMappingPanel parameterPanel { audioProcessor };
    bool showingParameterPanel = false;
    bool parameterPanelPositioned = false;
    void toggleParameterPanel();

    // La toolbar e' una barra dedicata (sfondo + separatore disegnati in
    // paint(), bounds calcolati in resized()) che contiene questi bottoni,
    // con icone disegnate da CsoundLookAndFeel in base al nome
    // (Component::setName) assegnato nel costruttore.
    juce::Rectangle<int> toolbarBounds;
    juce::TextButton applyButton        { "Apply" };
    juce::TextButton clearConsoleButton { "Clear console" };
    juce::TextButton paramsButton       { "Parameters" };

    // Save/Load Session: scrivono/leggono su disco (FileChooser, extension
    // .csd - un .csd VERO, il mapping dei parametri va in appendice dentro
    // un tag dedicato, vedi CsoundAudioProcessor::saveSessionToFile)
    // l'intero stato - codice Csound + mapping dei 64 parametri -
    // INDIPENDENTEMENTE dal progetto della DAW (che resta comunque salvato/
    // ripristinato come sempre da getStateInformation/setStateInformation).
    // Senza questo, rimuovere il plugin dalla traccia o perdere il progetto
    // avrebbe fatto perdere anche il codice: vedi CsoundAudioProcessor::
    // saveSessionToFile/loadSessionFromFile.
    juce::TextButton saveSessionButton { "Save Session..." };
    juce::TextButton loadSessionButton { "Load Session..." };

    // juce::FileChooser e' asincrono (launchAsync): deve restare in vita
    // finche' il suo callback non e' scattato, quindi va tenuto come
    // membro (non una variabile locale che morirebbe subito) - un solo
    // chooser alla volta basta, Save e Load non possono essere aperti
    // contemporaneamente dalla stessa UI.
    std::unique_ptr<juce::FileChooser> activeFileChooser;
    void promptSaveSession();
    void promptLoadSession();

    static constexpr int toolbarHeight = 44;

    CsoundLookAndFeel lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundAudioProcessorEditor)
};
