#pragma once

#include <JuceHeader.h>
#include <map>
#include <vector>

/**
    Raggruppamento dei parametri nel pannello Parameters, deciso dal CODICE
    del .csd con un commento:

        ; @group Synth
        kcut  chnget "cutoff"
        kwave chnget "wave"      ; anche Bool e Choice: stanno qui, non in fondo
        ; @group Master
        kvol  chnget "volume"

    Regole:
    - "; @group Nome" vale da li' fino al prossimo @group, alla fine dello
      strumento/UDO (endin/endop) o alla fine del codice. Lo stesso nome in
      piu' punti unisce i parametri nello stesso gruppo.
    - Il gruppo di un parametro e' quello del PRIMO chnget del suo canale
      nel testo (un canale letto in piu' strumenti sta dove compare prima).
    - Ordine: i gruppi nell'ordine della prima apparizione; dentro un gruppo
      i parametri nell'ordine dei chnget.
    - Canali senza @group, o che non compaiono in nessun chnget (nomi
      costruiti a runtime, refusi): gruppo "Other", in fondo, nell'ordine
      del pannello (Float, Int, Bool, Choice).
    - Senza nessun @group nel codice: nessuna intestazione, elenco piatto
      come sempre.

    Solo visivo: gli indici dei parametri per la DAW non cambiano. Viene
    ricalcolato a Apply e a Load (sul testo compilato), non mentre si scrive.
*/
namespace CsoundParameterGroups
{
    struct Placement
    {
        int group = -1;   // indice in Parsed::groupNames; -1 = nessun gruppo
        int order = 0;    // posizione del primo chnget nel testo
    };

    struct Parsed
    {
        juce::StringArray groupNames;                  // nell'ordine di prima apparizione
        std::map<juce::String, Placement> channels;    // canale -> gruppo/ordine (primo chnget)

        bool hasGroups() const { return ! groupNames.isEmpty(); }
    };

    /** Analizza il testo (intero .csd o solo l'orchestra). */
    inline Parsed parse (const juce::String& csdText)
    {
        Parsed out;

        // Solo l'orchestra, se il testo e' un .csd completo.
        auto code = csdText.fromFirstOccurrenceOf ("<CsInstruments>", false, false)
                           .upToFirstOccurrenceOf ("</CsInstruments>", false, false);
        if (code.isEmpty())
            code = csdText;

        // Commenti a blocco /* */ via: potrebbero contenere chnget o @group.
        {
            juce::String stripped;
            int pos = 0;
            while (pos < code.length())
            {
                const int open = code.indexOf (pos, "/*");
                if (open < 0) { stripped << code.substring (pos); break; }
                stripped << code.substring (pos, open);
                const int close = code.indexOf (open + 2, "*/");
                if (close < 0) break;
                // Le righe restano le stesse (serve solo l'ordine, non i numeri di riga).
                pos = close + 2;
            }
            code = stripped;
        }

        juce::StringArray lines;
        lines.addLines (code);

        int currentGroup = -1;
        int order = 0;

        auto groupIndexFor = [&out] (const juce::String& name)
        {
            const int existing = out.groupNames.indexOf (name);
            if (existing >= 0)
                return existing;
            out.groupNames.add (name);
            return out.groupNames.size() - 1;
        };

        for (const auto& rawLine : lines)
        {
            const auto line = rawLine.trim();

            // Commento "; @group Nome" (anche ";@group", "// @group").
            {
                juce::String comment;
                if (line.startsWithChar (';'))      comment = line.substring (1).trim();
                else if (line.startsWith ("//"))   comment = line.substring (2).trim();

                if (comment.startsWithIgnoreCase ("@group"))
                {
                    const auto name = comment.substring (6).trim();
                    currentGroup = name.isNotEmpty() ? groupIndexFor (name) : -1;
                    continue;
                }
            }

            // Codice senza il commento di fine riga.
            auto codePart = line;
            {
                const int semi = codePart.indexOfChar (';');
                if (semi >= 0) codePart = codePart.substring (0, semi);
                const int slashes = codePart.indexOf ("//");
                if (slashes >= 0) codePart = codePart.substring (0, slashes);
                codePart = codePart.trim();
            }

            if (codePart.isEmpty())
                continue;

            // Fine di strumento / UDO: il gruppo corrente si chiude.
            {
                const auto firstWord = codePart.upToFirstOccurrenceOf (" ", false, false)
                                               .upToFirstOccurrenceOf ("\t", false, false);
                if (firstWord == "endin" || firstWord == "endop")
                {
                    currentGroup = -1;
                    continue;
                }
            }

            // Tutti i chnget della riga: chnget "x", chnget:k("x"), chnget("x").
            int searchFrom = 0;
            while (true)
            {
                const int at = codePart.indexOf (searchFrom, "chnget");
                if (at < 0)
                    break;

                searchFrom = at + 6;

                // Deve essere una parola a se' (non "chngetks", "mychnget").
                if (at > 0 && (juce::CharacterFunctions::isLetterOrDigit (codePart[at - 1]) || codePart[at - 1] == '_'))
                    continue;

                int p = at + 6;
                if (p < codePart.length() && juce::CharacterFunctions::isLetterOrDigit (codePart[p]))
                    continue; // chngetks, chngeti...

                if (p < codePart.length() && codePart[p] == ':')               // chnget:k
                {
                    ++p;
                    while (p < codePart.length() && juce::CharacterFunctions::isLetterOrDigit (codePart[p])) ++p;
                }

                while (p < codePart.length() && (codePart[p] == ' ' || codePart[p] == '\t')) ++p;
                if (p < codePart.length() && codePart[p] == '(') ++p;
                while (p < codePart.length() && (codePart[p] == ' ' || codePart[p] == '\t')) ++p;

                if (p >= codePart.length() || codePart[p] != '"')
                    continue; // nome non letterale (Sname, sprintf...): non raggruppabile

                const int close = codePart.indexOfChar (p + 1, '"');
                if (close < 0)
                    continue;

                const auto channel = codePart.substring (p + 1, close);

                if (channel.isNotEmpty() && out.channels.find (channel) == out.channels.end())
                    out.channels[channel] = { currentGroup, order++ };
            }
        }

        return out;
    }
}
