#pragma once

#include <JuceHeader.h>

/**
    Syntax highlighting per il codice Csound (.orc / .sco / .csd).

    La lista di keyword/opcode e' scritta a mano ma copre un sottoinsieme
    molto ampio degli opcode standard di Csound (generatori, inviluppi,
    filtri, delay/riverbero, spazializzazione, FFT/phase vocoder, granular,
    physical modelling, MIDI, tabelle, file I/O, stringhe, utility). Non e'
    esaustiva al 100% (Csound ha centinaia di opcode aggiuntivi via plugin
    di terze parti): un miglioramento futuro possibile e' interrogare a
    runtime la libreria Csound collegata con csoundNewOpcodeList() per
    generare questa lista dinamicamente, cosi' l'editor resta sempre
    allineato alla versione/ai plugin realmente installati.
*/
class CsoundTokeniser final : public juce::CodeTokeniser
{
public:
    CsoundTokeniser();

    int readNextToken (juce::CodeDocument::Iterator& source) override;
    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override;

    enum TokenType
    {
        tokenType_default = 0,
        tokenType_comment,
        tokenType_keyword,
        tokenType_opcode,
        tokenType_identifier,
        tokenType_number,
        tokenType_string,
        tokenType_preprocessor,
        tokenType_punctuation
    };

private:
    static bool isIdentifierStart (juce::juce_wchar c) noexcept;
    static bool isIdentifierBody  (juce::juce_wchar c) noexcept;
    static void skipToEndOfLine   (juce::CodeDocument::Iterator& source);
    static void skipBlockComment  (juce::CodeDocument::Iterator& source);

    juce::StringArray keywords;
    juce::StringArray opcodes;
};
