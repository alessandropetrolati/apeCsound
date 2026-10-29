#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <regex>
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

    XY PAD, stesso meccanismo:

        ; @xy "Cutoff" "Resonance"

    due parametri Float (X, Y) mostrati come un'unica card con un pad
    bidimensionale invece di due slider. Restano due parametri distinti
    per la DAW (automazione, snapshot); solo la card cambia. Se uno dei
    due non e' un parametro Float assegnato, il commento e' ignorato. Il
    pad sta nel gruppo/ordine del canale X.

    I file Cabbage (widget "xypad channel("x","y")") e CsoundQt (BSBController
    con objectName/objectName2) diventano pad automaticamente: le loro
    sezioni <Cabbage>/<bsbPanel> restano nel file, quindi i pad si
    ricavano da li' ad ogni Apply, senza toccare il codice ne' il mapping.
*/
namespace CsoundParameterGroups
{
    struct Placement
    {
        int group = -1;   // indice in Parsed::groupNames; -1 = nessun gruppo
        int order = 0;    // posizione del primo chnget nel testo
    };

    struct XYPad { juce::String x, y; };

    struct Parsed
    {
        juce::StringArray groupNames;                  // nell'ordine di prima apparizione
        std::map<juce::String, Placement> channels;    // canale -> gruppo/ordine (primo chnget)
        std::map<juce::String, Placement> written;     // canale -> gruppo/ordine (primo chnset): le card del Monitor
                                                       // di un canale scritto sotto un @group vanno in quel gruppo
        std::vector<XYPad> xyPads;                     // "; @xy "X" "Y"" nell'ordine del codice

        bool hasGroups() const { return ! groupNames.isEmpty(); }
    };

    /** Estrae i nomi tra virgolette (o le parole separate da spazi/virgole)
        dal resto di un commento "@xy ...". */
    inline juce::StringArray parseNameList (const juce::String& rest)
    {
        juce::StringArray names;
        if (rest.containsChar ('"'))
        {
            int p = 0;
            while (true)
            {
                const int open = rest.indexOfChar (p, '"');
                if (open < 0) break;
                const int close = rest.indexOfChar (open + 1, '"');
                if (close < 0) break;
                names.add (rest.substring (open + 1, close));
                p = close + 1;
            }
        }
        else
        {
            names.addTokens (rest, " ,\t", "");
            names.removeEmptyStrings();
        }
        return names;
    }


    /** Gruppi dai contenitori della GUI Cabbage: "groupbox"/"image" (o
        qualunque widget) con "plant(...)" e un blocco "{ ... }" - il
        nome e' text("...") del contenitore, altrimenti la prima label
        dentro, altrimenti plant("..."). Contenitori SENZA graffe
        (sintassi vecchia): per geometria, un widget sta nel groupbox se
        il suo bounds(x,y,...) cade dentro. Restituisce canale -> gruppo,
        nell'ordine di apparizione (groupOrder). Il codice (@group) ha
        sempre la precedenza: vedi parse(). */
    inline void parseCabbageGroups (const juce::String& csdText,
                                    juce::StringArray& groupOrder,
                                    std::vector<std::pair<juce::String, juce::String>>& channelGroups)
    {
        const auto section = csdText.fromFirstOccurrenceOf ("<Cabbage>", false, false)
                                    .upToFirstOccurrenceOf ("</Cabbage>", false, false);
        if (section.isEmpty())
            return;

        juce::StringArray lines;
        lines.addLines (section);

        auto attr = [] (const juce::String& line, const char* name) -> juce::String
        {
            const std::regex re (std::string ("\\b") + name + R"rx(\s*\(\s*"([^"]*)")rx");
            std::smatch m;
            const std::string src = line.toStdString();
            return std::regex_search (src, m, re) ? juce::String (m[1].str()) : juce::String();
        };
        auto channelsOf = [] (const juce::String& line)
        {
            juce::StringArray out;
            const std::regex re (R"rx(\bchannel\s*\(([^)]*)\))rx");
            const std::regex q (R"rx("([^"]*)")rx");
            std::smatch m;
            const std::string src = line.toStdString();
            if (std::regex_search (src, m, re))
            {
                const std::string inner = m[1].str();
                for (auto it = std::sregex_iterator (inner.begin(), inner.end(), q); it != std::sregex_iterator(); ++it)
                    out.add (juce::String ((*it)[1].str()));
            }
            return out;
        };
        struct Box { float x = 0, y = 0, w = 0, h = 0; bool containsCentreOf (const Box& o) const
                     { const float cx = o.x + o.w * 0.5f, cy = o.y + o.h * 0.5f; return cx >= x && cx <= x + w && cy >= y && cy <= y + h; } };
        auto boundsOf = [] (const juce::String& line, Box& r)
        {
            const std::regex re (R"rx(\bbounds\s*\(\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+))rx");
            std::smatch m;
            const std::string src = line.toStdString();
            if (! std::regex_search (src, m, re)) return false;
            r = { (float) std::atof (m[1].str().c_str()), (float) std::atof (m[2].str().c_str()),
                  (float) std::atof (m[3].str().c_str()), (float) std::atof (m[4].str().c_str()) };
            return true;
        };

        struct Container { juce::String name; juce::String plant; bool braces = false; Box bounds; };
        std::vector<Container> stack;                 // contenitori con graffe aperti
        std::vector<Container> absolute;              // contenitori senza graffe (geometria)
        std::vector<std::pair<juce::String, int>> pending; // (canale, indice contenitore in stack) da nominare
        std::vector<juce::String> stackNamesResolved;

        auto registerGroup = [&groupOrder] (const juce::String& name) { groupOrder.addIfNotAlreadyThere (name); };

        auto finishContainer = [&] (Container& c)
        {
            if (c.name.isEmpty()) c.name = c.plant;
            if (c.name.isEmpty()) return;
            registerGroup (c.name);
        };

        juce::String lastWidgetLine; // per la graffa "{" su una riga a se'

        for (const auto& raw : lines)
        {
            auto line = raw.trim();
            if (line.isEmpty() || line.startsWithChar (';')) continue;

            // "{" da sola sulla riga dopo il widget (stile Cabbage recente):
            // vale come se fosse in coda alla riga precedente.
            if (line == "{" && lastWidgetLine.isNotEmpty())
            {
                line = lastWidgetLine + " {";
                lastWidgetLine.clear();
            }

            if (line.startsWithChar ('}'))
            {
                if (! stack.empty())
                {
                    auto c = stack.back(); stack.pop_back();
                    finishContainer (c);
                    for (auto& pc : pending)
                        if (pc.second == (int) stack.size() && c.name.isNotEmpty())
                            channelGroups.push_back ({ pc.first, c.name });
                    pending.erase (std::remove_if (pending.begin(), pending.end(),
                                                   [&stack] (const auto& pc) { return pc.second == (int) stack.size(); }),
                                   pending.end());
                }
                line = line.substring (1).trim();
                if (line.isEmpty()) continue;
            }

            const auto widget = line.upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("\t", false, false).toLowerCase();
            const bool opensBlock = line.endsWithChar ('{');
            lastWidgetLine = opensBlock ? juce::String() : line;
            const auto text = attr (line, "text");
            const auto plant = attr (line, "plant");

            // label dentro un contenitore senza text(): ne diventa il nome.
            if (widget == "label" && ! stack.empty() && stack.back().name.isEmpty() && text.isNotEmpty())
                stack.back().name = text;

            if (opensBlock)
            {
                Container c; c.name = (widget == "groupbox" || plant.isNotEmpty()) ? text : juce::String(); c.plant = plant; c.braces = true;
                boundsOf (line, c.bounds);
                stack.push_back (c);
                continue;
            }

            // groupbox senza graffe (sintassi vecchia, o plant senza blocco):
            // contenitore per geometria.
            if (widget == "groupbox" && stack.empty())
            {
                Container c; c.name = text.isNotEmpty() ? text : plant; c.plant = plant; c.braces = false;
                if (boundsOf (line, c.bounds) && c.name.isNotEmpty()) { absolute.push_back (c); registerGroup (c.name); }
                continue;
            }

            const auto chans = channelsOf (line);
            if (chans.isEmpty()) continue;

            if (! stack.empty())
            {
                for (const auto& ch : chans)
                    pending.push_back ({ ch, (int) stack.size() - 1 });
                continue;
            }

            Box b;
            if (boundsOf (line, b))
            {
                for (const auto& c : absolute)
                    if (c.bounds.containsCentreOf (b))
                    {
                        for (const auto& ch : chans) channelGroups.push_back ({ ch, c.name });
                        break;
                    }
            }
        }

        // Blocchi mai chiusi: nominati e svuotati lo stesso.
        while (! stack.empty())
        {
            auto c = stack.back(); stack.pop_back();
            finishContainer (c);
            for (auto& pc : pending)
                if (pc.second == (int) stack.size() && c.name.isNotEmpty())
                    channelGroups.push_back ({ pc.first, c.name });
            pending.erase (std::remove_if (pending.begin(), pending.end(),
                                           [&stack] (const auto& pc) { return pc.second == (int) stack.size(); }),
                           pending.end());
        }
    }

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

        // Pad dai widget Cabbage / CsoundQt (vedi sopra) - letti dal testo
        // INTERO, perche' <Cabbage>/<bsbPanel> stanno fuori dall'orchestra.
        {
            const std::string full = csdText.toStdString();

            const std::regex cabbageXy (R"rx(\bxypad\b[^\n]*?\bchannel\s*\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\))rx");
            for (auto it = std::sregex_iterator (full.begin(), full.end(), cabbageXy); it != std::sregex_iterator(); ++it)
                out.xyPads.push_back ({ juce::String ((*it)[1].str()), juce::String ((*it)[2].str()) });

            // Solo DENTRO l'elemento del controller (non oltre </bsbObject>).
            const std::regex qtController (R"rx(<bsbObject[^>]*type="BSBController"[^>]*>((?:(?!</bsbObject>)[\s\S])*)</bsbObject>)rx");
            const std::regex qtNameX (R"rx(<objectName>([^<]*)</objectName>)rx");
            const std::regex qtNameY (R"rx(<objectName2>([^<]*)</objectName2>)rx");
            for (auto it = std::sregex_iterator (full.begin(), full.end(), qtController); it != std::sregex_iterator(); ++it)
            {
                const std::string body = (*it)[1].str();
                std::smatch mx, my;
                if (! std::regex_search (body, mx, qtNameX) || ! std::regex_search (body, my, qtNameY))
                    continue;
                const juce::String x (mx[1].str()), y (my[1].str());
                if (x.isNotEmpty() && y.isNotEmpty() && x != y)
                    out.xyPads.push_back ({ x, y });
            }
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

                if (comment.startsWithIgnoreCase ("@xy"))
                {
                    const auto names = parseNameList (comment.substring (3).trim());
                    if (names.size() >= 2 && names[0].isNotEmpty() && names[1].isNotEmpty() && names[0] != names[1])
                        out.xyPads.push_back ({ names[0], names[1] });
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

            // Tutti i chnset della riga: il nome e' il letterale DOPO la prima
            // virgola dell'opcode (chnset kval, "x" / chnset(kval, "x") /
            // chnset limit(k, 0, 1), "x"): le virgole dentro le parentesi
            // dell'espressione non contano.
            searchFrom = 0;
            while (true)
            {
                const int at = codePart.indexOf (searchFrom, "chnset");
                if (at < 0)
                    break;
                searchFrom = at + 6;

                if (at > 0 && (juce::CharacterFunctions::isLetterOrDigit (codePart[at - 1]) || codePart[at - 1] == '_'))
                    continue;
                int p = at + 6;
                if (p < codePart.length() && juce::CharacterFunctions::isLetterOrDigit (codePart[p]))
                    continue; // chnsetks, chnseti...
                if (p < codePart.length() && codePart[p] == ':')
                {
                    ++p;
                    while (p < codePart.length() && juce::CharacterFunctions::isLetterOrDigit (codePart[p])) ++p;
                }
                while (p < codePart.length() && (codePart[p] == ' ' || codePart[p] == '\t')) ++p;

                int base = 0, depth = 0;
                if (p < codePart.length() && codePart[p] == '(') { base = depth = 1; ++p; }

                bool commaSeen = false;
                juce::String channel;
                for (; p < codePart.length(); ++p)
                {
                    const auto c = codePart[p];
                    if (c == '(') { ++depth; continue; }
                    if (c == ')') { if (--depth < base) break; continue; }
                    if (c == ',' && depth == base) { commaSeen = true; continue; }
                    if (c == '"')
                    {
                        const int close = codePart.indexOfChar (p + 1, '"');
                        if (close < 0) break;
                        if (commaSeen && depth == base) { channel = codePart.substring (p + 1, close); break; }
                        p = close; // stringa nel valore (chnset "testo", "x"): si salta
                    }
                }

                if (channel.isNotEmpty() && out.written.find (channel) == out.written.end())
                    out.written[channel] = { currentGroup, order++ };
            }
        }

        // Gruppi dalla GUI Cabbage (groupbox/plant): SOLO per i canali che il
        // codice non ha gia' messo in un @group. Nuovi gruppi in coda a
        // quelli del codice; dentro, l'ordine dei widget.
        {
            juce::StringArray cabbageOrder;
            std::vector<std::pair<juce::String, juce::String>> cabbageGroups;
            parseCabbageGroups (csdText, cabbageOrder, cabbageGroups);

            int widgetOrder = 1000000; // dopo tutti i chnget del codice
            for (const auto& [channel, groupName] : cabbageGroups)
            {
                auto it = out.channels.find (channel);
                if (it != out.channels.end() && it->second.group >= 0)
                    continue; // il codice decide
                const int gi = groupIndexFor (groupName);
                if (it != out.channels.end())
                    it->second.group = gi;
                else
                    out.channels[channel] = { gi, widgetOrder++ };

                // Meter/display Cabbage dentro un groupbox: anche la card del
                // Monitor va nel gruppo, se il codice non l'ha gia' deciso.
                auto w = out.written.find (channel);
                if (w != out.written.end() && w->second.group < 0)
                    w->second.group = gi;
            }
        }

        return out;
    }
}
