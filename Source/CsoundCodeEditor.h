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
    Quando si digita, a inizio riga, una keyword di chiusura blocco
    (endin/endif/od/endop/else/elseif) la riga viene riallineata subito
    al livello corretto, come fa Xcode quando si digita una parentesi
    graffa di chiusura.
*/
class CsoundCodeEditor final : public juce::CodeEditorComponent
{
public:
    CsoundCodeEditor (juce::CodeDocument& document, juce::CodeTokeniser* tokeniser);

    void handleReturnKey() override;
    void insertTextAtCaret (const juce::String& textToInsert) override;

private:
    static juce::String stripCommentAndTrim (const juce::String& lineText);
    static bool startsWithDedentKeyword (const juce::String& trimmedLine);
    static bool opensBlock (const juce::String& trimmedLine);

    int indentDepthBeforeLine (int lineIndex) const;
    void reindentLine (int lineIndex);

    static constexpr int indentSpaces = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundCodeEditor)
};
