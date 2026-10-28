#pragma once

#include <JuceHeader.h>

/**
    Scorciatoie da tastiera di apeCsound (Cmd su macOS, Ctrl su Windows e
    Linux: juce::ModifierKeys::commandModifier). Gestite in
    CsoundAudioProcessorEditor::keyPressed; i menu ne mostrano il testo.

    Uguali in standalone e nei plugin: in un plugin arrivano solo quando la
    finestra di apeCsound ha il focus e l'host passa i tasti al plugin
    (es. REAPER: "Send all keyboard input to plug-in"); i tasti non gestiti
    tornano alla DAW.
*/
namespace CsoundShortcuts
{
    inline juce::KeyPress cmd (int keyCode, bool shift = false)
    {
        return juce::KeyPress (keyCode, shift ? juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier
                                              : juce::ModifierKeys::commandModifier, 0);
    }

    inline juce::KeyPress apply()          { return cmd (juce::KeyPress::returnKey); }
    inline juce::KeyPress find()           { return cmd ('F'); }
    inline juce::KeyPress showParameters() { return cmd ('1'); }
    inline juce::KeyPress showConsole()    { return cmd ('2'); }
    inline juce::KeyPress showGraphs()     { return cmd ('3'); }
    inline juce::KeyPress showChannels()   { return cmd ('4'); }
    inline juce::KeyPress largerText()     { return cmd ('='); }
    inline juce::KeyPress smallerText()    { return cmd ('-'); }
    inline juce::KeyPress undo()           { return cmd ('Z'); }
    inline juce::KeyPress redo()           { return cmd ('Z', true); }

    // Editor di codice, come Xcode (gestite in CsoundCodeEditor::keyPressed).
    inline juce::KeyPress toggleComment()  { return cmd ('/'); }
    inline juce::KeyPress evaluateLine()   { return cmd ('E'); } // riga Evaluate nella barra di help
    inline juce::KeyPress evaluate()       { return juce::KeyPress (juce::KeyPress::returnKey, juce::ModifierKeys::shiftModifier, 0); } // live coding (Cmd+Return e' Apply)
    inline juce::KeyPress indent()         { return cmd (']'); }
    inline juce::KeyPress unindent()       { return cmd ('['); }
    // Re-indent (Xcode: Ctrl+I). Su Windows/Linux Ctrl e' anche il "command".
    inline juce::KeyPress reindent()       { return juce::KeyPress ('I', juce::ModifierKeys::ctrlModifier, 0); }

    /** Cmd+/ anche con tastiere dove "/" richiede Shift (es. italiana:
        Shift+7): si guarda anche il carattere prodotto, non solo il tasto. */
    inline bool isToggleComment (const juce::KeyPress& key)
    {
        const auto mods = key.getModifiers();
        return mods.isCommandDown() && ! mods.isAltDown()
            && (key.getKeyCode() == '/' || key.getTextCharacter() == '/');
    }

    // File.
    inline juce::KeyPress save()           { return cmd ('S'); }
    inline juce::KeyPress saveAs()         { return cmd ('S', true); }
    inline juce::KeyPress load()           { return cmd ('O'); }
    inline juce::KeyPress newSession()     { return cmd ('N'); }

    /** Testo per i menu ("⌘S" su macOS, "Ctrl+S" altrove); vuoto su iOS. */
    inline juce::String label (const juce::KeyPress& key)
    {
       #if JUCE_IOS
        juce::ignoreUnused (key);
        return {};
       #else
        return key.getTextDescriptionWithIcons();
       #endif
    }
}
