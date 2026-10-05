#pragma once

#include <JuceHeader.h>

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
*/
class CsoundCodeEditor final : public juce::CodeEditorComponent,
                                private juce::CodeDocument::Listener
{
public:
    CsoundCodeEditor (juce::CodeDocument& document, juce::CodeTokeniser* tokeniser);
    ~CsoundCodeEditor() override;

    void handleReturnKey() override;

private:
    // juce::CodeDocument::Listener
    void codeDocumentTextInserted (const juce::String& newText, int insertIndex) override;
    void codeDocumentTextDeleted (int startIndex, int endIndex) override;

    static juce::String stripCommentAndTrim (const juce::String& lineText);
    static bool startsWithDedentKeyword (const juce::String& trimmedLine);
    static bool opensBlock (const juce::String& trimmedLine);

    int indentDepthBeforeLine (int lineIndex) const;
    void reindentLine (int lineIndex);

    juce::CodeDocument& codeDocument;
    bool isReindenting = false; // guardia anti-reentry (reindentLine modifica il documento)

    static constexpr int indentSpaces = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundCodeEditor)
};
