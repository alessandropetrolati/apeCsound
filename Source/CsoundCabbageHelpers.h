#pragma once

/*
    Helper "puri" dell'import dei widget Cabbage (una riga = un widget), usati
    da CsoundAudioProcessor::importCabbageParameters in PluginProcessor.cpp e
    dai test unitari (Tests/CsoundUnitTests.cpp). Solo juce::String +
    std::regex: niente dipendenze dal processor, compilano anche con il solo
    modulo juce_core.
*/

// Import automatico da <Cabbage> - vedi il commento su importCabbageParameters
// in PluginProcessor.h per la mappatura widget -> tipo di parametro. Piccoli
// helper regex isolati qui (std::regex, non juce - JUCE non ha un tokenizer
// per espressioni "nomeCampo(...)" pronto all'uso) che leggono UNA riga alla
// volta: lo stile Cabbage compatto (una dichiarazione di widget per riga) e'
// quello assunto, righe spezzate su piu' linee non sono gestite.
#include <JuceHeader.h>
#include <regex>

namespace cabbageImport
{
    inline bool cabbageMatchSingleChannel (const juce::String& line, juce::String& channelName)
    {
        // Delimitatore custom "re(...)re" (non il default "(...)"): il
        // pattern contiene ")\"" al suo interno (chiusura del gruppo subito
        // seguita dalla virgoletta letterale), che con il delimitatore di
        // default avrebbe chiuso la raw string PRIMA della fine vera - il
        // resto del pattern sarebbe finito fuori dalle virgolette come
        // codice C++ invece che come testo, mandando in errore tutto cio'
        // che segue (esattamente l'errore "undeclared identifier"/"expected
        // ';'" segnalato).
        static const std::regex re (R"re(channel\(\s*"([^"]*)"\s*\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            channelName = juce::String (m[1].str());
            return true;
        }

        return false;
    }

    inline bool cabbageMatchChannelPair (const juce::String& line, juce::String& channelA, juce::String& channelB)
    {
        // Stesso motivo del delimitatore custom spiegato in
        // cabbageMatchSingleChannel sopra - qui il pattern ha ADDIRITTURA
        // due occorrenze di ")\"" al suo interno.
        static const std::regex re (R"re(channel\(\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            channelA = juce::String (m[1].str());
            channelB = juce::String (m[2].str());
            return true;
        }

        return false;
    }

    inline bool cabbageMatchRangeContents (const juce::String& line, juce::String& contents)
    {
        static const std::regex re (R"re(range\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            contents = juce::String (m[1].str());
            return true;
        }

        return false;
    }

    // rangeX(...)/rangeY(...) - SOLO per il widget xypad (vedi il branch
    // "xypad" in importCabbageParameters): a differenza di range(...) sopra
    // (riusato invariato da hslider/vslider/rslider/nslider/vrange/hrange),
    // xypad ha due direttive SEPARATE, una per asse, invece di una singola
    // range() condivisa - regex dedicate, "rangeX\(" non puo' confondersi
    // con "range\(" (richiede la X subito dopo "range", non presente nella
    // sintassi degli altri widget).
    inline bool cabbageMatchRangeXContents (const juce::String& line, juce::String& contents)
    {
        static const std::regex re (R"re(rangeX\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            contents = juce::String (m[1].str());
            return true;
        }

        return false;
    }

    inline bool cabbageMatchRangeYContents (const juce::String& line, juce::String& contents)
    {
        static const std::regex re (R"re(rangeY\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            contents = juce::String (m[1].str());
            return true;
        }

        return false;
    }

    /** latched(0|1) di un button Cabbage: 0 = momentaneo (1 premuto, 0 al
        rilascio), 1 = interruttore (il default di Cabbage). */
    inline bool cabbageMatchLatched (const juce::String& line, bool& latched)
    {
        static const std::regex re (R"re(\blatched\(\s*([0-9.]+)\s*\))re");
        std::smatch m;
        const auto s = line.toStdString();
        if (! std::regex_search (s, m, re))
            return false;
        latched = juce::String (m[1].str()).getDoubleValue() != 0.0;
        return true;
    }

    inline bool cabbageMatchValueNumber (const juce::String& line, double& value)
    {
        static const std::regex re (R"re(value\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            value = juce::String (m[1].str()).trim().getDoubleValue();
            return true;
        }

        return false;
    }

    // min(...)/max(...) - SOLO per il widget encoder (vedi il branch
    // "encoder" in importCabbageParameters): a differenza di
    // hslider/vslider/rslider/nslider/vrange/hrange, che prendono
    // min/max/default/skew/increment tutti insieme da un'unica range(...),
    // l'endless encoder di Cabbage li espone come due proprieta' separate
    // e NIENTE default/skew/increment (fissi, vedi il branch).
    inline bool cabbageMatchMinNumber (const juce::String& line, double& value)
    {
        static const std::regex re (R"re(min\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            value = juce::String (m[1].str()).trim().getDoubleValue();
            return true;
        }

        return false;
    }

    inline bool cabbageMatchMaxNumber (const juce::String& line, double& value)
    {
        static const std::regex re (R"re(max\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            value = juce::String (m[1].str()).trim().getDoubleValue();
            return true;
        }

        return false;
    }

    inline bool cabbageMatchItems (const juce::String& line, juce::StringArray& items)
    {
        // La proprieta' Cabbage con le etichette del combobox e'
        // text("A", "B", ...), NON items(...) (quest'ultimo tentato prima
        // per sbaglio - un combobox Cabbage reale usa sempre text()).
        // Proviamo comunque anche items(...) come fallback, nel caso un
        // file scritto a mano/esportato da un'altra versione lo usi.
        static const std::regex reText  (R"re(text\(([^)]*)\))re");
        static const std::regex reItems (R"re(items\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (! std::regex_search (s, m, reText) && ! std::regex_search (s, m, reItems))
            return false;

        juce::StringArray rawTokens;
        rawTokens.addTokens (juce::String (m[1].str()), ",", "");

        for (auto token : rawTokens)
        {
            token = token.trim();

            if (token.startsWithChar ('"') && token.endsWithChar ('"'))
                token = token.substring (1, token.length() - 1);

            items.add (token);
        }

        return true;
    }

    // Spezza "min, max, default, skew, increment" (il contenuto grezzo di
    // range(...), vedi cabbageMatchRangeContents) in fino a 5 numeri. Il
    // campo "default" (indice 2) puo' contenere un secondo numero separato
    // da ':' - solo per vrange/hrange (vedi il commento in
    // PluginProcessor.h) - che finisce in defaultB; altrimenti defaultB
    // resta identico a values[2].
    //
    // Cabbage accetta range() anche con MENO di 5 campi (tipicamente solo
    // min, max, default - skew e increment sono opzionali con un loro
    // default): qui sovrascriviamo SOLO i campi effettivamente presenti,
    // lasciando gli altri al valore che il chiamante ha gia' messo in
    // "values" prima di chiamare questa funzione (gli inizializzatori
    // {0, 1, 0, 1, 0.001} nei tre punti da cui viene chiamata) - cosi' un
    // range(-10, 10, 6.791) con solo 3 campi imposta min/max/default e
    // lascia skew=1.0/increment=0.001 come se non fossero stati scritti.
    inline bool cabbageParseFiveFieldRange (const juce::String& contents, double values[5], double& defaultB)
    {
        juce::StringArray tokens;
        tokens.addTokens (contents, ",", "");

        if (tokens.isEmpty())
            return false;

        const int fieldCount = juce::jmin (tokens.size(), 5);
        defaultB = values[2]; // valore di partenza se il campo 2 non e' presente o non ha ':'

        for (int i = 0; i < fieldCount; ++i)
        {
            auto token = tokens[i].trim();

            if (i == 2 && token.containsChar (':'))
            {
                const auto colon = token.indexOfChar (':');
                values[2] = token.substring (0, colon).trim().getDoubleValue();
                defaultB  = token.substring (colon + 1).trim().getDoubleValue();
            }
            else
            {
                values[i] = token.getDoubleValue();

                if (i == 2)
                    defaultB = values[2];
            }
        }

        return true;
    }

    // Spezza "min, max, default" (il contenuto grezzo di rangeX(...)/
    // rangeY(...), vedi cabbageMatchRangeXContents/cabbageMatchRangeYContents
    // sopra) in fino a 3 numeri - SOLO per xypad: a differenza di
    // cabbageParseFiveFieldRange sopra, qui non c'e' ne' skew/increment
    // (fissi, vedi il branch "xypad" in importCabbageParameters) ne' la
    // sintassi "default1:default2" di vrange/hrange (ogni asse ha gia' il
    // proprio default separato, tramite rangeX/rangeY distinti). Stessa
    // tolleranza di cabbageParseFiveFieldRange per un numero di campi
    // inferiore al massimo: i campi assenti restano al valore che il
    // chiamante ha gia' messo in "values" prima di chiamare questa funzione.
    inline bool cabbageParseThreeFieldRange (const juce::String& contents, double values[3])
    {
        juce::StringArray tokens;
        tokens.addTokens (contents, ",", "");

        if (tokens.isEmpty())
            return false;

        const int fieldCount = juce::jmin (tokens.size(), 3);

        for (int i = 0; i < fieldCount; ++i)
            values[i] = tokens[i].trim().getDoubleValue();

        return true;
    }

    // Identifica il tipo di widget dal primo token della riga (dopo aver
    // scartato spazi iniziali) - non basta un contains() perche' "hslider"
    // compare anche dentro commenti o altri nomi; serve il token di testa.
    inline juce::String cabbageLeadingIdentifier (const juce::String& line)
    {
        static const std::regex re (R"re(^\s*([A-Za-z_][A-Za-z0-9_]*))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
            return juce::String (m[1].str());

        return {};
    }
    //==========================================================================
    // Bottoni che lanciano eventi: il modo "Cabbage" - il bottone e' un
    // parametro Bool (Momentary o Toggle) e l'evento lo genera il CODICE sul
    // fronte di salita (trigger), cosi' resta visibile, modificabile e
    // portabile. Vedi "Copy Trigger Code" nel menu della card e l'import dei
    // bottoni "event" di CsoundQt.

    /** Nome di variabile Csound dal nome del canale: solo lettere, cifre e
        "_", mai una cifra in testa. */
    inline juce::String csoundVariableSuffix (const juce::String& channel)
    {
        juce::String v;
        for (auto c : channel)
            v << (juce::CharacterFunctions::isLetterOrDigit (c) || c == '_' ? juce::String::charToString (c) : juce::String ("_"));
        if (v.isEmpty() || juce::CharacterFunctions::isDigit (v[0]))
            v = "_" + v;
        return v;
    }

    /** Le righe da incollare in un instr per lanciare un evento quando il
        bottone passa da 0 a 1 (una sola volta per pressione, anche con
        l'automazione della DAW che ripete il valore a ogni blocco). */
    inline juce::String csoundTriggerCode (const juce::String& channel)
    {
        const auto k = "k" + csoundVariableSuffix (channel);
        juce::String code;
        code << k << " chnget \"" << channel << "\"\n"
             << "if trigger:k(" << k << ", 0.5, 0) == 1 then   ; 0 -> 1: once per press\n"
             << "  event \"i\", 10, 0, 1                     ; your instrument, start, duration\n"
             << "endif\n";
        return code;
    }

    struct EventButton { juce::String channel, scoreLine, offLine; };

    /** Bottone "event" di CsoundQt con durata negativa ("i 1 0 -1", nota
        tenuta): CsoundQt al rilascio (o allo spegnimento, se latch) manda
        la stessa riga con il numero di strumento negativo ("i -1 0 -1",
        "i \"-name\" ..."). Stesso algoritmo di QuteButton::performAction.
        Vuota se la durata non e' negativa (nessun OFF). */
    inline juce::String csoundQtTurnOffLine (const juce::String& eventLine)
    {
        juce::StringArray e;
        e.addTokens (eventLine.trim(), " \t", "");
        e.removeEmptyStrings();
        if (e.isEmpty())
            return {};
        if (e[0] == "i")
            e.remove (0);
        else if (e[0].startsWithChar ('i'))
            e.set (0, e[0].substring (1));
        if (e.size() < 3 || e[2].getDoubleValue() >= 0.0 || e[0].isEmpty())
            return {};
        if (e[0].startsWithChar ('"') || e[0].startsWithChar ('\''))
            e.set (0, e[0].substring (0, 1) + "-" + e[0].substring (1));
        else
            e.set (0, "-" + e[0]);
        e.insert (0, "i");
        return e.joinIntoString (" ");
    }

    /** Nome di canale dal testo di un bottone ("Play note" -> "Play_note"),
        vuoto se non resta niente di utile. */
    inline juce::String channelNameFromLabel (const juce::String& label)
    {
        juce::String n;
        for (auto c : label.trim())
            if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '_') n << juce::String::charToString (c);
            else if (c == ' ' || c == '-') n << "_";
        while (n.contains ("__")) n = n.replace ("__", "_");
        n = n.trimCharactersAtStart ("_").trimCharactersAtEnd ("_");
        if (n.isNotEmpty() && juce::CharacterFunctions::isDigit (n[0]))
            n = "_" + n;
        return n;
    }

    /** Una riga di score come stringa Csound: {{...}} se possibile. */
    inline juce::String csoundScoreLiteral (const juce::String& scoreLine)
    {
        const auto line = scoreLine.trim().replace ("\n", " ").replace ("\r", " ");
        return line.contains ("}}") ? "\"" + line.replace ("\"", "\\\"") + "\""
                                    : "{{" + line + "}}";
    }

    /** Orchestra compilata da apeCsound mentre il motore suona, per i Toggle
        con una riga EVENT (stile CsoundQt): uno strumento sempre attivo che
        manda la riga quando il bottone passa da 0 a 1. Ricompilata quando
        cambiano le righe: "generation" ferma l'istanza della versione
        precedente (gkApeSlotEvents cambia, la vecchia istanza fa turnoff).
        Vuota = solo il nuovo valore di generation (ferma tutto). */
    inline juce::String csoundSlotEventsOrc (const std::vector<EventButton>& buttons, int generation)
    {
        juce::String code;
        code << "gkApeSlotEvents init " << generation << "\n";
        if (buttons.empty())
            return code;
        code << "instr apeSlotEvents\n"
             << "  if gkApeSlotEvents != " << generation << " then\n"
             << "    turnoff\n"
             << "  endif\n";
        for (const auto& b : buttons)
        {
            // ON: 0 -> 1 (pressione / accensione); OFF: 1 -> 0 (rilascio /
            // spegnimento). Righe vuote: niente.
            if (b.scoreLine.trim().isNotEmpty())
                code << "  if trigger:k(chnget:k(\"" << b.channel << "\"), 0.5, 0) == 1 then\n"
                     << "    scoreline " << csoundScoreLiteral (b.scoreLine) << ", 1\n"
                     << "  endif\n";
            if (b.offLine.trim().isNotEmpty())
                code << "  if trigger:k(chnget:k(\"" << b.channel << "\"), 0.5, 1) == 1 then\n"
                     << "    scoreline " << csoundScoreLiteral (b.offLine) << ", 1\n"
                     << "  endif\n";
        }
        code << "endin\n"
             << "schedule \"apeSlotEvents\", 0, -1\n";
        return code;
    }

    /** Uno strumento sempre attivo che manda la riga di score di ciascun
        bottone "event" importato (CsoundQt) quando il bottone viene premuto. */
    inline juce::String csoundEventButtonsInstrument (const std::vector<EventButton>& buttons)
    {
        if (buttons.empty())
            return {};
        juce::String code;
        code << "\n; ---------------------------------------------------------------------------\n"
             << "; Event buttons imported from CsoundQt by apeCsound. Each button is a\n"
             << "; momentary Button parameter; this instrument sends its score line once,\n"
             << "; when the button goes from 0 to 1 (a press, or the DAW automation).\n"
             << "; ---------------------------------------------------------------------------\n"
             << "instr apeEventButtons\n";
        for (const auto& b : buttons)
        {
            const auto line = b.scoreLine.trim().replace ("\n", " ").replace ("\r", " ");
            const auto literal = line.contains ("}}") ? "\"" + line.replace ("\"", "\\\"") + "\""
                                                      : "{{" + line + "}}";
            code << "  if trigger:k(chnget:k(\"" << b.channel << "\"), 0.5, 0) == 1 then\n"
                 << "    scoreline " << literal << ", 1\n"
                 << "  endif\n";
        }
        code << "endin\n"
             << "alwayson \"apeEventButtons\"\n";
        return code;
    }
} // namespace cabbageImport
