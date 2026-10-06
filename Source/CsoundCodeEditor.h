#pragma once

#include <JuceHeader.h>
#include <map>
#include <functional>
#include "CsoundOpcodeHelp.h"

/**
    juce::CodeEditorComponent specializzato per Csound: aggiunge auto-indent
    "intelligente" stile Xcode / VS Code, basato sulla struttura del
    linguaggio (non solo sulla whitespace della riga precedente):

    - instr ... endin
    - opcode ... endop
    - if/elseif ... then ... else ... endif
    - while/until ... do ... od

    Alla pressione di Invio, il livello di indentazione della nuova riga
    viene ricalcolato scandendo il documento dall'inizio fino alla riga
    corrente (blocchi aperti = +1 livello, blocchi chiusi = -1 livello).

    Il riallineamento "live" delle keyword di chiusura (endin/endif/od/
    endop/else/elseif) e' implementato tramite juce::CodeDocument::Listener
    invece che tramite l'override di insertTextAtCaret: in questo modo
    scatta per QUALSIASI modifica che produce quel testo su quella riga
    (digitazione carattere per carattere, digitazione "a blocchi" via IME,
    autocompletamento, undo/redo...), e non solo per il percorso specifico
    che passa da insertTextAtCaret.

    Offre anche help inline + autocompletamento sugli opcode, in due parti
    distinte:

    - L'help completo (sintassi + descrizione) NON e' un popup: viene
      notificato tramite onOpcodeHelpChanged, che PluginEditor usa per
      riempire una barra fissa tra l'editor e la consolle. Scatta quando
      il caret si trova sulla parola di un opcode conosciuto - che ci sia
      arrivato digitando, cliccando sopra con il mouse, o spostando la
      selezione con le freccie. Per gli opcode della tabella scritta a
      mano (CsoundOpcodeHelpData, verificata contro il Csound Reference
      Manual "classico") mostra sintassi/descrizione in prosa; per TUTTI
      gli altri opcode (l'intero elenco riportato dal motore Csound
      realmente collegato via setOpcodeSignatures) sintetizza una riga di
      sintassi nello stesso formato del manuale, con nomi di argomento
      generici ma dal prefisso di rate corretto (es. "a1 oscil k2, k3 [,
      i4, i5]") cosi' la copertura non e' limitata al sottoinsieme scritto
      a mano.
    - L'autocompletamento invece resta un vero popup flottante vicino al
      caret (SuggestionPopup), mostrato solo mentre si digita una parola
      parziale (>= 2 caratteri) che e' prefisso di uno o piu' nomi di
      opcode conosciuti. Si naviga con le freccie Su/Giu o passandoci sopra
      col mouse, si accetta con Invio, Tab, o un click (sempre sul
      suggerimento evidenziato); Esc, o la perdita del focus (es. un click
      fuori dall'editor), lo chiudono.

    Accetta anche il drag and drop di uno slot dal pannello parametri
    (CsoundParameterMappingPanel::ParamRow, vedi PluginEditor): rilasciando
    uno slot con un canale assegnato sulla riga puntata dal mouse, viene
    inserita una nuova riga "kNomeCanale chnget "Nome Canale"" (nome
    variabile = "k" + nome canale senza spazi) PRIMA di quella riga - vedi
    isInterestedInDragSource/itemDropped.
*/
class CsoundCodeEditor final : public juce::CodeEditorComponent,
                                public juce::DragAndDropTarget,
                                private juce::CodeDocument::Listener
{
public:
    CsoundCodeEditor (juce::CodeDocument& document, juce::CodeTokeniser* tokeniser);
    ~CsoundCodeEditor() override;

    void handleReturnKey() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void focusLost (juce::Component::FocusChangeType cause) override;

    // juce::DragAndDropTarget - vedi il commento in testa alla classe.
    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDragEnter (const SourceDetails& dragSourceDetails) override;
    void itemDragMove (const SourceDetails& dragSourceDetails) override;
    void itemDragExit (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

    void paintOverChildren (juce::Graphics& g) override;

    /** Da chiamare (sul message thread) ogni volta che il motore Csound
        (ri)parte, con l'elenco completo e aggiornato degli opcode
        realmente disponibili in quella istanza (vedi
        CsoundAudioProcessor::getOpcodeSignatures). Alimenta sia l'help
        inline sia l'autocompletamento; finche' non viene chiamato,
        autocompletamento e help si limitano al sottoinsieme scritto a
        mano in CsoundOpcodeHelpData. */
    void setOpcodeSignatures (const juce::Array<CsoundLiveOpcodeInfo>& signatures);

    /** Chiamato (sul message thread) ogni volta che il testo di help da
        mostrare nella barra dedicata cambia: entrambe le stringhe vuote
        significa "nessun opcode selezionato, nascondi/svuota la barra". */
    std::function<void (const juce::String& syntax, const juce::String& description)> onOpcodeHelpChanged;

private:
    // juce::CodeDocument::Listener
    void codeDocumentTextInserted (const juce::String& newText, int insertIndex) override;
    void codeDocumentTextDeleted (int startIndex, int endIndex) override;

    static juce::String stripCommentAndTrim (const juce::String& lineText);
    static bool startsWithDedentKeyword (const juce::String& trimmedLine);
    static bool opensBlock (const juce::String& trimmedLine);

    int indentDepthBeforeLine (int lineIndex) const;
    void reindentLine (int lineIndex);

    // Resto frazionario dell'ultimo scroll a rotellina/gesture, accumulato
    // tra un evento e il successivo - vedi mouseWheelMove().
    float wheelScrollRemainder = 0.0f;

    // Riga evidenziata mentre si trascina uno slot sopra l'editor (-1 =
    // nessun drag in corso) - solo feedback visivo, vedi itemDragMove/
    // paintOverChildren.
    int dragHighlightLine = -1;

    // Autocompletamento sugli opcode -------------------------------------
    // Componente minimale che non e' MAI focalizzabile (niente
    // setWantsKeyboardFocus, niente grabKeyboardFocus) - vedi la saga del
    // focus stealing di REAPER in PluginEditor - ma intercetta il mouse
    // (hover/click) per permettere di selezionare un suggerimento senza
    // passare dalla tastiera.
    struct SuggestionPopup final : public juce::Component
    {
        SuggestionPopup();

        void paint (juce::Graphics& g) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent& event) override;
        juce::Point<int> computeSize() const;
        int rowIndexAtY (int y) const;

        juce::StringArray suggestionNames;
        juce::String suggestionPrefix;
        int selectedIndex = 0;

        // Chiamati con l'indice di riga sotto il mouse (o -1 se nessuno):
        // CsoundCodeEditor li usa per sincronizzare selectedIndex (hover)
        // e per accettare il suggerimento (click).
        std::function<void (int)> onHoverIndex;
        std::function<void (int)> onClickIndex;

        static constexpr int lineHeight = 18;
    };

    void updateOpcodeHelp();
    void pushSignatureHelp (const juce::String& word);
    void pushSignatureHelpForSelectedSuggestion();
    void showSuggestions (const juce::String& prefix);
    void hideSuggestions();
    void clearOpcodeHelp();
    void positionSuggestionPopup();
    void acceptSuggestion (const juce::String& fullName);
    void moveSuggestionSelection (int delta);
    bool forceShowSuggestions();

    static juce::String wordEndingAt (const juce::String& lineText, int indexInLine);
    static juce::String wordAroundCaret (const juce::String& lineText, int indexInLine);
    static juce::String formatSignatureLine (const CsoundLiveOpcodeInfo& info);

    SuggestionPopup suggestionPopup;

    // Nome (minuscolo) -> una riga di firma per ogni variante di tipo
    // riportata dal motore (un opcode polimorfico, es. oscil a-rate/
    // k-rate, compare piu' volte in csoundNewOpcodeList). Alimentato da
    // setOpcodeSignatures(); vuoto finche' il motore non e' mai partito.
    std::map<juce::String, juce::StringArray> liveSignatureLines;

    // Elenco (ordinato, senza duplicati) di tutti i nomi di opcode
    // conosciuti, usato per l'autocompletamento: inizialmente solo quelli
    // della tabella scritta a mano, poi sostituito/ampliato dall'elenco
    // reale del motore non appena disponibile.
    juce::StringArray allOpcodeNames;

    // Suggerimenti attualmente mostrati (vuoto se il popup non e' visibile):
    // Tab completa al primo.
    juce::StringArray currentSuggestions;

    // True se onOpcodeHelpChanged e' stato chiamato per ultimo con del
    // testo non vuoto - serve a decidere se Esc deve "consumare" la
    // pressione del tasto (c'era qualcosa da chiudere) o lasciarla
    // propagare (es. all'host).
    bool helpBarHasContent = false;

    juce::CodeDocument& codeDocument;
    bool isReindenting = false; // guardia anti-reentry (reindentLine modifica il documento)

    static constexpr int indentSpaces = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundCodeEditor)
};
