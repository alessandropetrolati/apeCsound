#pragma once

#include <JuceHeader.h>
#include <map>
#include <regex>
#include <vector>

/*
    Logica "pura" del gruppo Monitor del pannello Parametri (vedi
    CsoundParameterMappingPanel::refreshMonitorList): come si leggono gli
    attributi di chn_k e come i canali vengono accorpati in card. Isolata qui
    per i test unitari (Tests/CsoundUnitTests.cpp).

    ATTRIBUTI (undicesimo argomento di chn_k, arriva all'host in
    controlChannelHints_t::attributes; parole separate da spazi o virgole,
    case-insensitive):

      chn_k "Level", 2, 2, 0, 0, 1, 0, 0, 0, 0, "db spark"

      db           il valore e' un'ampiezza lineare: mostrato e misurato in
                   dB (range -60..0 dB se gli hint min/max non sono dati)
      spark        storia degli ultimi ~5 s (sparkline) al posto della barra
      int          valore senza decimali
      hidden       il canale non compare nel Monitor
      group:Name   la card multi-barra "Name" (vince sul raggruppamento
                   automatico per nome; "group=Name" equivalente)

    ACCORPAMENTO PER NOME: canali numerici "Base1, Base2..." / "Base_1,
    Base_2..." (prefisso uguale + indice finale, almeno due) diventano una
    sola card con una barra per canale. Le stringhe non si accorpano mai.
*/
namespace csoundMonitor
{
    struct Attributes
    {
        bool db = false, spark = false, isInt = false, hidden = false;
        juce::String group;
    };

    inline Attributes parseAttributes (const juce::String& text)
    {
        Attributes a;
        juce::StringArray tokens;
        tokens.addTokens (text, " ,\t;", "\"");
        tokens.removeEmptyStrings();

        for (auto t : tokens)
        {
            t = t.unquoted().trim();
            const auto lower = t.toLowerCase();

            if      (lower == "db")     a.db = true;
            else if (lower == "spark")  a.spark = true;
            else if (lower == "int")    a.isInt = true;
            else if (lower == "hidden") a.hidden = true;
            else if (lower.startsWith ("group:") || lower.startsWith ("group="))
                a.group = t.substring (6).trim();
        }
        return a;
    }

    /** "Band_7" -> ("Band", 7); "LFO2" -> ("LFO", 2); "Cutoff" -> ("", -1). */
    inline bool splitIndexedName (const juce::String& name, juce::String& base, int& index)
    {
        static const std::regex re (R"re(^(.*?[^\d_ \-])[ _\-]?(\d{1,4})$)re");
        const auto s = name.toStdString();
        std::smatch m;
        if (! std::regex_match (s, m, re))
            return false;
        base  = juce::String (m[1].str());
        index = std::stoi (m[2].str());
        return base.isNotEmpty();
    }

    struct ChannelInfo
    {
        juce::String name;
        bool isString = false;
        Attributes attributes;
    };

    struct Card
    {
        juce::String title;          // nome del canale, o del gruppo
        std::vector<int> channels;   // indici in input, nell'ordine di visualizzazione
        bool multi = false;          // card multi-barra
        int firstIndex = 0;          // posizione del primo membro nell'elenco (ordine delle card)
    };

    /** Card nell'ordine di prima apparizione; i canali "hidden" non compaiono. */
    inline std::vector<Card> buildCards (const std::vector<ChannelInfo>& channels)
    {
        std::vector<Card> cards;
        std::map<juce::String, size_t> groupCard;                 // titolo gruppo -> indice card
        std::map<juce::String, std::vector<std::pair<int, int>>> byBase; // base -> (indice, numero)
        std::vector<juce::String> baseOrder;

        // 1) gruppi espliciti e candidati per nome
        for (int i = 0; i < (int) channels.size(); ++i)
        {
            const auto& c = channels[(size_t) i];
            if (c.attributes.hidden)
                continue;

            if (! c.isString && c.attributes.group.isNotEmpty())
            {
                auto it = groupCard.find (c.attributes.group);
                if (it == groupCard.end())
                {
                    cards.push_back ({ c.attributes.group, {}, true, i });
                    it = groupCard.emplace (c.attributes.group, cards.size() - 1).first;
                }
                cards[it->second].channels.push_back (i);
                continue;
            }

            juce::String base; int index = -1;
            if (! c.isString && splitIndexedName (c.name, base, index))
            {
                if (byBase.find (base) == byBase.end())
                    baseOrder.push_back (base);
                byBase[base].push_back ({ i, index });
                continue;
            }

            cards.push_back ({ c.name, { i }, false, i });
        }

        // 2) i candidati per nome: gruppo se almeno due, altrimenti card singole.
        //    Inseriti nella posizione del primo membro per rispettare l'ordine.
        struct Pending { int firstIndex; Card card; };
        std::vector<Pending> pending;
        for (const auto& base : baseOrder)
        {
            auto members = byBase[base];
            int firstIndex = members.front().first;              // posizione del primo membro nel testo
            for (const auto& m : members) firstIndex = juce::jmin (firstIndex, m.first);
            std::sort (members.begin(), members.end(), [] (auto& a, auto& b) { return a.second < b.second; });
            if (members.size() >= 2)
            {
                Card card { base, {}, true, firstIndex };
                for (const auto& m : members) card.channels.push_back (m.first);
                pending.push_back ({ firstIndex, card });
            }
            else
            {
                pending.push_back ({ firstIndex, { channels[(size_t) firstIndex].name, { firstIndex }, false, firstIndex } });
            }
        }

        for (const auto& p : pending)
        {
            auto pos = std::find_if (cards.begin(), cards.end(), [&] (const Card& c) { return c.firstIndex > p.firstIndex; });
            cards.insert (pos, p.card);
        }

        return cards;
    }

    /** Nomi dichiarati come identChannel("...") nella sezione <Cabbage> del
        testo: canali stringa GUI-only (chnset "colour(...)", "visible(0)"...)
        che in Cabbage pilotano l'aspetto di un widget e qui non hanno
        nessun senso nel Monitor. */
    inline juce::StringArray cabbageIdentChannels (const juce::String& csdText)
    {
        juce::StringArray names;
        const auto section = csdText.fromFirstOccurrenceOf ("<Cabbage>", false, false)
                                    .upToFirstOccurrenceOf ("</Cabbage>", false, false);
        if (section.isEmpty())
            return names;

        static const std::regex re (R"re(\bidentChannel\s*\(\s*"([^"]+)"\s*\))re");
        const auto str = section.toStdString();
        for (auto it = std::sregex_iterator (str.begin(), str.end(), re); it != std::sregex_iterator(); ++it)
            names.addIfNotAlreadyThere (juce::String ((*it)[1].str()));
        return names;
    }

    /** Il VALORE di un canale stringa e' un messaggio GUI di Cabbage?
        "visible(1)", "bounds(10,20,1,50)", "colour(20, 60, 70)",
        "tableNumber(1)", "text(abc)", anche in sequenza separati da virgola.
        Mandati con chnset a identChannel costruiti a runtime (sprintfk), non
        riconoscibili dal nome: si guarda il contenuto. */
    inline bool isCabbageGuiMessage (const juce::String& value)
    {
        const auto v = value.trim();
        if (v.isEmpty() || ! v.endsWithChar (')'))
            return false;
        static const std::regex re (R"re(^\s*[A-Za-z_][A-Za-z0-9_:]*\s*\([^()]*\)(\s*,?\s*[A-Za-z_][A-Za-z0-9_:]*\s*\([^()]*\))*\s*$)re");
        return std::regex_match (v.toStdString(), re);
    }

    /** Ampiezza lineare -> dB, limitata in basso. */
    inline double linearToDb (double v, double floorDb = -60.0)
    {
        return v > 0.0 ? juce::jmax (floorDb, 20.0 * std::log10 (v)) : floorDb;
    }
} // namespace csoundMonitor
