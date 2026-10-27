#pragma once

#include <JuceHeader.h>

/*
    Funzioni "pure" sul testo Csound usate dall'editor (CsoundCodeEditor) e
    dai test unitari (Tests/CsoundUnitTests.cpp): nessuna dipendenza da
    CodeDocument o da componenti.
*/
namespace csoundSyntax
{
    /** "outs opcode ins" (riga di sintassi del manuale) -> "outs:T = opcode(ins)",
        la forma funzionale di Csound 7 con annotazione di tipo sulle uscite.
        Righe gia' in forma di assegnazione, con parentesi o senza il nome
        dell'opcode come token a se' tornano invariate. */
    inline juce::String toModernSyntax (const juce::String& manualLine, const juce::String& opcodeName)
    {
        // "outs opcode ins" -> "outs:T = opcode(ins)" (Csound 7: assegnazione
        // funzionale con annotazione di tipo sulle variabili di uscita).
        // Righe gia' in forma di assegnazione o senza il nome dell'opcode
        // come token a se' (instr, if/then, "0dbfs = iarg"...) restano come sono.
        const auto line = manualLine.trim();

        if (line.isEmpty() || line.containsChar ('=') || line.containsChar ('('))
            return manualLine;

        const auto words = juce::StringArray::fromTokens (line, " ", "");
        int opcodeIdx = -1;

        for (int w = 0; w < words.size(); ++w)
        {
            if (words[w].equalsIgnoreCase (opcodeName))
            {
                opcodeIdx = w;
                break;
            }
        }

        if (opcodeIdx < 0)
            return manualLine;

        juce::String outs, ins;

        for (int w = 0; w < opcodeIdx; ++w)
            outs << (w > 0 ? " " : "") << words[w];

        for (int w = opcodeIdx + 1; w < words.size(); ++w)
            ins << (w > opcodeIdx + 1 ? " " : "") << words[w];

        // Annotazione di tipo per ogni variabile di uscita: "ares" -> "ares:a",
        // "gkres" -> "gkres:k", "kval[]" -> "kval:k[]", "xres" (a oppure k)
        // -> "xres:a/k". Token non riconoscibili (es. "[, a2 [...]]") restano
        // invariati.
        auto annotate = [] (const juce::String& token) -> juce::String
        {
            auto t = token.trim();
            bool isArray = false;

            if (t.endsWith ("[]"))
            {
                isArray = true;
                t = t.dropLastCharacters (2);
            }

            if (t.isEmpty() || ! t.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"))
                return token;

            auto rate = t.startsWith ("g") && t.length() > 1 ? t[1] : t[0];
            juce::String rateText;

            switch (rate)
            {
                case 'a': case 'k': case 'i': case 'S': case 'f': case 'w':
                    rateText = juce::String::charToString (rate);
                    break;
                case 'x':
                    rateText = "a/k";
                    break;
                default:
                    return token;
            }

            return t + ":" + rateText + (isArray ? "[]" : "");
        };

        juce::String result;

        if (outs.isNotEmpty())
        {
            juce::StringArray outTokens;
            outTokens.addTokens (outs, ",", "");

            for (int i = 0; i < outTokens.size(); ++i)
                result << (i > 0 ? ", " : "") << annotate (outTokens[i]);

            result << " = ";
        }

        result << words[opcodeIdx] << "(" << ins.trim() << ")";
        return result;
    }

    /** Nome del canale (letterale tra virgolette) sulla riga di un chnget/chnset/
        chn*: quello che contiene la colonna 'col', altrimenti il primo. Vuoto se
        la riga non contiene un opcode chn* (fuori dai commenti). */
    inline juce::String channelNameInLine (const juce::String& line, int col)
    {

        // Niente nei commenti.
        const int semi = line.indexOfChar (';');
        const auto code = semi >= 0 ? line.substring (0, semi) : line;

        // Un opcode chn* come parola a se' (chnget, chnset, chnget:k, chnmix...).
        bool hasChnOpcode = false;
        for (int at = code.indexOf ("chn"); at >= 0; at = code.indexOf (at + 3, "chn"))
        {
            if (at > 0 && (juce::CharacterFunctions::isLetterOrDigit (code[at - 1]) || code[at - 1] == '_'))
                continue;
            hasChnOpcode = true;
            break;
        }
        if (! hasChnOpcode)
            return {};

        // Letterali tra virgolette: quello che contiene il caret, altrimenti il primo.
        juce::String first;
        int p = 0;
        while (true)
        {
            const int open = code.indexOfChar (p, '"');
            if (open < 0) break;
            const int close = code.indexOfChar (open + 1, '"');
            if (close < 0) break;

            const auto name = code.substring (open + 1, close);
            if (name.isNotEmpty())
            {
                if (col >= open && col <= close + 1)
                    return name;
                if (first.isEmpty())
                    first = name;
            }
            p = close + 1;
        }
        return first;
    }
} // namespace csoundSyntax
