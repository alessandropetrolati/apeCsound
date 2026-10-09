#pragma once

#include <JuceHeader.h>
#include <map>
#include <functional>
#include <vector>
#include "CsoundOpcodeHelp.h"
#include "CsoundCodeView.h"
#include "CsoundEditCallout.h" // [EDIT-CALLOUT]

/**
    CodeView (editor di codice nostro, vedi CsoundCodeView.h) specializzato
    per Csound: aggiunge auto-indent
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
*/
class CsoundCodeEditor final : public CodeView,
                                public juce::DragAndDropTarget,
                                private juce::CodeDocument::Listener
{
public:
    CsoundCodeEditor (juce::CodeDocument& document, juce::CodeTokeniser* tokeniser);
    ~CsoundCodeEditor() override;

    void handleReturnKey() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void resized() override;
    void focusLost (juce::Component::FocusChangeType cause) override;
    void caretPositionMoved() override;

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

    /** Sintassi "moderna" (funzionale, con annotazione di tipo di Csound 7)
        nella barra di help: "ares oscil xamp, xcps" diventa
        "ares:a = oscil(xamp, xcps)". Vale per tutte le fonti (manuale,
        motore, UDO). Riaggiorna subito la barra. */
    void setModernSyntaxHelp (bool shouldUseModernSyntax);
    bool isModernSyntaxHelp() const noexcept { return modernSyntaxHelp; }

    /** Conversione di una riga di sintassi del manuale nella forma
        funzionale (pubblica e statica per poterla testare). Righe senza
        la struttura "[outs] opcode [ins]" restano invariate. */
    static juce::String toModernSyntax (const juce::String& manualLine, const juce::String& opcodeName);

    /** Chiamato (sul message thread) ogni volta che il testo di help da
        mostrare nella barra dedicata cambia: tutte le stringhe vuote
        significa "nessun opcode selezionato, nascondi/svuota la barra".
        category e' la categoria del manuale ("Signal Generators:Basic
        Oscillators"), "User-defined opcode" per le UDO del .csd corrente,
        vuota se sconosciuta (opcode riportato solo dal motore). */
    std::function<void (const juce::String& syntax, const juce::String& description, const juce::String& category)> onOpcodeHelpChanged;

    /** Cronologia undo/redo UNICA condivisa con il pannello Parametri (vedi
        sharedUndoManager in PluginEditor): Cmd+Z/Cmd+Shift+Z vengono
        intercettati in keyPressed() PRIMA che la classe base JUCE li
        consumi per il proprio undo testuale interno, e instradati qui
        invece che nella cronologia privata del documento. Se non
        assegnati, il comportamento di default di CodeEditorComponent resta
        inalterato (vedi il fallback in keyPressed()). */
    std::function<void()> onUndoRequested;
    std::function<void()> onRedoRequested;

    // --- Cerca / Sostituisci (richiesta esplicita) ---------------------
    // Pilotato dalla barra Find/Replace di PluginEditor. Tutte le
    // occorrenze sono evidenziate in paintOverChildren (giallo), quella
    // corrente piu' marcata (arancio, con bordo). Le occorrenze si
    // ricalcolano da sole a ogni modifica del documento finche' la ricerca
    // e' attiva (query non vuota). matchCase: maiuscole/minuscole distinte;
    // wholeWord: l'occorrenza non deve essere attaccata a lettere, cifre o
    // '_' (i caratteri degli identificatori Csound).
    void setSearchQuery (const juce::String& query, bool matchCase, bool wholeWord);
    void clearSearch();

    int getNumSearchMatches() const          { return (int) searchMatches.size(); }
    int getCurrentSearchMatchIndex() const   { return currentSearchMatch; } // -1 = nessuna

    // Seleziona (e porta in vista) l'occorrenza successiva/precedente,
    // ricominciando dall'inizio/fine del documento.
    void selectNextSearchMatch();
    void selectPreviousSearchMatch();

    // Sostituiscono come UN solo passo di undo (una transazione del
    // documento, rispecchiata nella cronologia condivisa dal ponte di
    // PluginEditor). replaceCurrent... sostituisce l'occorrenza corrente e
    // passa alla successiva; ritorna false se non ce n'e' una.
    // replaceAll... ritorna il numero di sostituzioni.
    bool replaceCurrentSearchMatch (const juce::String& replacement);
    int  replaceAllSearchMatches (const juce::String& replacement);

    // Chiamata ogni volta che cambiano le occorrenze o quella corrente
    // (per il contatore "3 of 12" della barra).
    std::function<void()> onSearchResultsChanged;

private:
    struct SearchMatch
    {
        int start = 0, end = 0;   // indici di carattere nel documento [start, end)
        int line = 0, column = 0; // per disegnare senza ricalcolare la posizione
    };

    void refreshSearchMatches();                 // ricalcola tutte le occorrenze
    void selectSearchMatch (int index);          // seleziona + porta in vista
    void notifySearchResults() { if (onSearchResultsChanged) onSearchResultsChanged(); }

    juce::String searchQuery;
    bool searchMatchCase = false, searchWholeWord = false;
    std::vector<SearchMatch> searchMatches;
    int currentSearchMatch = -1;
    bool suspendSearchRefresh = false;           // durante replaceAll (un ricalcolo solo alla fine)
    // juce::CodeDocument::Listener
    void codeDocumentTextInserted (const juce::String& newText, int insertIndex) override;
    void codeDocumentTextDeleted (int startIndex, int endIndex) override;

    static juce::String stripCommentAndTrim (const juce::String& lineText);
    static bool startsWithDedentKeyword (const juce::String& trimmedLine);
    static bool opensBlock (const juce::String& trimmedLine);

    int indentDepthBeforeLine (int lineIndex) const;
    void reindentLine (int lineIndex);

    // Menu contestuale: NON il juce::PopupMenu di CodeEditorComponent ma lo
    // stesso CsoundActionSheet degli altri menu dell'app (richiesta
    // esplicita), con Cut/Copy/Paste/Delete/Select All, Undo/Redo
    // (cronologia condivisa, vedi onUndoRequested), "Indent" (re-indenta
    // le righe selezionate, o quella del caret, con la logica di Invio) e
    // "Comment"/"Uncomment" (aggiunge o toglie ";" in testa alle righe).
    // CodeView lo richiama per il tasto destro/ctrl-clic e, col touch, al
    // rilascio dopo una selezione (pressione prolungata/maniglie) o al tap
    // sulla selezione.
    void showContextMenu (juce::Point<int> localPos) override;
    void performContextMenuItem (int itemId);

    // [EDIT-CALLOUT] menu touch non modale (vedi CsoundEditCallout.h)
    void showTouchEditMenu (juce::Rectangle<int> selectionArea) override;
    void hideTouchEditMenu() override;
    CsoundEditCallout editCallout;

    // Intervallo di righe coperto dalla selezione (o la riga del caret):
    // una riga finale "toccata" solo con il caret in colonna 0 e' esclusa.
    juce::Range<int> getSelectedLineRange() const;
    void indentSelectedLines();
    void toggleCommentOnSelectedLines();
    bool areSelectedLinesCommented() const;

    enum ContextMenuItem { ctxCut = 1, ctxCopy, ctxPaste, ctxDelete, ctxSelectAll, ctxUndo, ctxRedo, ctxIndent, ctxComment };

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
    struct Suggestion
    {
        juce::String name;    // nome dell'opcode
        juce::String syntax;  // riga di sintassi completa (formato del manuale)
    };

    struct SuggestionPopup final : public juce::Component
    {
        SuggestionPopup();

        void paint (juce::Graphics& g) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent& event) override;
        juce::Point<int> computeSize() const;
        int rowIndexAtY (int y) const;

        // Una riga per VARIANTE di opcode (come CsoundQt): nome, argomenti
        // di ingresso e rate dell'uscita, es. "oscil   xamp, xcps [, ifn, iphs]  :a".
        juce::StringArray rows;
        juce::String suggestionPrefix;
        int selectedIndex = 0;
        int widestRow = 0; // in caratteri, per computeSize()

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
    // Per VALORE (non per riferimento): la chiamata arriva con un elemento
    // di currentSuggestions, che hideSuggestions() svuota a meta' funzione
    // (un riferimento diventerebbe pendente -> crash sul refcount della String).
    void acceptSuggestion (Suggestion suggestion);
    void mouseDown (const juce::MouseEvent& e) override;
    void moveSuggestionSelection (int delta);
    bool forceShowSuggestions();

    static juce::String wordEndingAt (const juce::String& lineText, int indexInLine);
    static juce::String wordAroundCaret (const juce::String& lineText, int indexInLine);
    static juce::String formatSignatureLine (const CsoundLiveOpcodeInfo& info);

    // UDO ("opcode Nome, outtypes, intypes") definite nel documento
    // corrente, rilette (solo quando il testo e' cambiato) al prossimo
    // aggiornamento dell'help: cosi' anche gli opcode dell'utente hanno
    // una riga nella barra e compaiono nell'autocompletamento.
    struct UserOpcode
    {
        juce::String name, outTypes, inTypes;
        int line = 0;
    };

    void rescanUserOpcodesIfNeeded();
    const UserOpcode* findUserOpcode (const juce::String& word) const;

    juce::Array<UserOpcode> userOpcodes;
    bool userOpcodesDirty = true;

    bool modernSyntaxHelp = false;

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
    // una voce per VARIANTE di sintassi (stile CsoundQt), Invio/Tab/click
    // inseriscono l'INTERA struttura "outlet opcode inlet [, opzionali]".
    juce::Array<Suggestion> currentSuggestions;

    // Tutte le righe di sintassi conosciute per un opcode: UDO del file,
    // manuale (una per variante), o firma dal motore; {name} se ignoto.
    juce::StringArray getSyntaxVariants (const juce::String& name);
    void buildSuggestions (const juce::String& prefixWord, int maxRows);
    static juce::String makeSuggestionRow (const juce::String& name, const juce::String& syntax);

    // "Modalita' parametri" (CsoundQt): dopo l'inserimento di una sintassi
    // completa il primo token della riga e' selezionato; Tab/Shift+Tab
    // passano al token successivo/precedente (separatori: spazio , ( ) [ ]
    // =), digitare lo sostituisce; Esc, Invio, freccie o un click escono.
    void enterParameterMode (int line);
    void exitParameterMode();
    bool selectParameterToken (bool forward);
    bool parameterMode = false;
    int parameterLine = -1;
    juce::String parameterOpcode; // help della barra fisso su questo opcode
    bool selectingParameter = false;

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
