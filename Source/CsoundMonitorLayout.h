#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <map>
#include <regex>
#include <vector>

/*
    Logica "pura" del gruppo Monitor del pannello Parametri (vedi
    CsoundParameterMappingPanel::refreshMonitorList): che aspetto prende un
    canale e come i canali vengono accorpati in card. Isolata qui per i test
    unitari (Tests/CsoundUnitTests.cpp).

    Filosofia: nessun linguaggio da imparare. L'aspetto lo decidono il TIPO
    del canale (k, S, audio), il range standard di chn_k, i VALORI osservati
    (0/1 -> spia, range attraverso lo zero -> barra bipolare) e il NOME
    (prefisso + indice -> una card con una barra per canale). Un solo
    attributo, nell'undicesimo argomento di chn_k (con itype diverso da 0,
    altrimenti Csound lo scarta):

      chn_k "Pitch", 2, 2, 0, 0, 0, 0, 0, 0, 0, "spark"

      spark   storia degli ultimi ~5 s (sparkline) al posto della barra

    Gli attributi della 1.0.5 (db, int, hidden, group:) sono ignorati.
*/
namespace csoundMonitor
{
    struct Attributes
    {
        bool spark = false;
    };

    inline Attributes parseAttributes (const juce::String& text)
    {
        Attributes a;
        juce::StringArray tokens;
        tokens.addTokens (text, " ,\t;", "\"");
        for (const auto& t : tokens)
            if (t.unquoted().trim().equalsIgnoreCase ("spark"))
                a.spark = true;
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
        bool isAudio = false;   // i canali audio si accorpano solo fra loro
    };

    struct Card
    {
        juce::String title;          // nome del canale, o del gruppo
        std::vector<int> channels;   // indici in input, nell'ordine di visualizzazione
        bool multi = false;          // card multi-barra
        int firstIndex = 0;          // posizione del primo membro nell'elenco (ordine delle card)
    };

    /** Card nell'ordine di prima apparizione. */
    inline std::vector<Card> buildCards (const std::vector<ChannelInfo>& channels)
    {
        std::vector<Card> cards;
        std::map<juce::String, std::vector<std::pair<int, int>>> byBase; // base -> (indice, numero)
        std::vector<juce::String> baseOrder;

        // 1) candidati per nome (prefisso + indice); gli altri: card singole
        for (int i = 0; i < (int) channels.size(); ++i)
        {
            const auto& c = channels[(size_t) i];
            juce::String base; int index = -1;
            if (! c.isString && splitIndexedName (c.name, base, index))
            {
                const auto key = base + (c.isAudio ? "\x01" "audio" : "");
                if (byBase.find (key) == byBase.end())
                    baseOrder.push_back (key);
                byBase[key].push_back ({ i, index });
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
                Card card { base.upToFirstOccurrenceOf ("\x01", false, false), {}, true, firstIndex };
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

    /** Barra di un valore nel range lo..hi, come frazioni 0..1 della
        larghezza: [from, to]. Range BIPOLARE (lo < 0 < hi, es. LFO -1..1):
        la barra parte dallo zero, a destra per i positivi, a sinistra per i
        negativi; altrimenti parte da sinistra. zero = posizione dello zero
        (-1 se il range non lo contiene). */
    struct BarSpan { float from = 0.0f, to = 0.0f, zero = -1.0f; };

    inline BarSpan barSpan (double value, double lo, double hi)
    {
        BarSpan b;
        if (! (hi > lo))
            return b;
        const auto norm = [lo, hi] (double v) { return (float) juce::jlimit (0.0, 1.0, (v - lo) / (hi - lo)); };
        const float f = norm (value);
        if (lo < 0.0 && hi > 0.0)
        {
            b.zero = norm (0.0);
            b.from = juce::jmin (b.zero, f);
            b.to   = juce::jmax (b.zero, f);
        }
        else
        {
            b.from = 0.0f;
            b.to   = f;
        }
        return b;
    }

    /** Riconoscimento automatico di una SPIA (LED): un canale che vale solo
        0 o 1 (Overload, Gate, Play, i passi di un sequencer). Diventa LED
        dopo minObservations letture binarie con almeno un 1; il primo
        valore diverso da 0/1 lo esclude per sempre (niente sfarfallio fra
        barra e spia). */
    struct BinaryTracker
    {
        int observations = 0;
        bool oneSeen = false, nonBinarySeen = false;

        void observe (double v)
        {
            ++observations;
            if (v == 1.0)        oneSeen = true;
            else if (v != 0.0)   nonBinarySeen = true;
        }
        bool looksBinary (int minObservations = 20) const
        {
            return ! nonBinarySeen && oneSeen && observations >= minObservations;
        }
    };

    /** Card multipla (Light1..Light8): la decisione e' della card, non del
        singolo membro. In una fila di luci ciascuna vale 1 solo ogni tanto;
        decidendo membro per membro la card resterebbe per un po' un misto
        di barre e spie. Tracker comune: 1 visto in un membro qualsiasi,
        un valore non 0/1 in un membro qualsiasi esclude tutti, letture =
        quelle del membro che ne ha di meno. */
    inline BinaryTracker combine (const BinaryTracker& a, const BinaryTracker& b)
    {
        BinaryTracker t;
        t.observations  = juce::jmin (a.observations, b.observations);
        t.oneSeen       = a.oneSeen || b.oneSeen;
        t.nonBinarySeen = a.nonBinarySeen || b.nonBinarySeen;
        return t;
    }

    /** LED si' o no: solo valori 0/1 osservati (BinaryTracker), e range
        assente o esattamente 0..1. */
    inline bool showsAsLed (bool hasHints, double hintMin, double hintMax, const BinaryTracker& t)
    {
        if (hasHints && ! (hintMin == 0.0 && hintMax == 1.0))
            return false;
        return t.looksBinary();
    }

    /** Riconoscimento automatico di un canale a PASSI (la posizione di un
        sequencer, una scelta fra N): solo valori interi. Come BinaryTracker:
        dopo minObservations letture intere; il primo valore non intero lo
        esclude per sempre. */
    struct IntegerTracker
    {
        int observations = 0;
        bool nonIntegerSeen = false;

        void observe (double v)
        {
            ++observations;
            if (v != std::floor (v)) nonIntegerSeen = true;
        }
        bool looksInteger (int minObservations = 10) const
        {
            return ! nonIntegerSeen && observations >= minObservations;
        }
    };

    /** Card a passi: range dichiarato con estremi interi, da 3 a 33 valori
        (0..1 e' una spia, oltre 33 celle non si leggono), e solo valori
        interi osservati. */
    constexpr int kMaxStepCells = 33;
    inline bool showsAsSteps (bool hasHints, double hintMin, double hintMax, const IntegerTracker& t)
    {
        if (! hasHints || hintMin != std::floor (hintMin) || hintMax != std::floor (hintMax))
            return false;
        const double cells = hintMax - hintMin + 1.0;
        return cells >= 3.0 && cells <= (double) kMaxStepCells && t.looksInteger();
    }

    /** I tipi di card, ciascuno con il suo aspetto (vedi MonitorRow::paint). */
    enum class Kind { text, lamp, audio, spark, steps, bipolar, range, adaptive };

    /** Il tipo di una card singola. lo/hi: range in uso (dichiarato o visto). */
    inline Kind kindOf (bool isString, bool isAudio, bool spark, bool led, bool hasHints, double lo, double hi,
                        bool steps = false)
    {
        if (isString) return Kind::text;
        if (isAudio)  return Kind::audio;
        if (led)      return Kind::lamp;
        if (spark)    return Kind::spark;
        if (steps)    return Kind::steps;
        if (hi > lo && lo < 0.0 && hi > 0.0) return Kind::bipolar;
        return hasHints ? Kind::range : Kind::adaptive;
    }

    /** Il testo del valore di una card numerica: fino a 3 decimali, senza
        zeri finali (gli interi restano interi). audioDb: valore lineare
        mostrato in dB con un decimale. */
    inline juce::String formatMonitorValue (double raw, bool audioDb = false);

    /** RMS di un blocco di campioni (canale audio: l'ultimo ksmps). */
    template <typename Sample>
    inline double blockRms (const Sample* data, int n)
    {
        if (data == nullptr || n <= 0)
            return 0.0;
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
            sum += (double) data[i] * (double) data[i];
        return std::sqrt (sum / (double) n);
    }

    /** Balistica da meter: sale subito, scende di 'decay' per lettura (a
        20 Hz, 0.82 = circa -30 dB/s). Un blocco di ksmps campioni letto ogni
        50 ms e' rumoroso: cosi' il meter resta leggibile. */
    inline double meterBallistics (double newValue, double previous, double decay = 0.82)
    {
        return newValue >= previous ? newValue : juce::jmax (newValue, previous * decay);
    }

    /** Ampiezza lineare -> dB, limitata in basso. */
    inline double linearToDb (double v, double floorDb = -60.0)
    {
        return v > 0.0 ? juce::jmax (floorDb, 20.0 * std::log10 (v)) : floorDb;
    }

    inline juce::String formatMonitorValue (double raw, bool audioDb)
    {
        const double v = audioDb ? linearToDb (raw) : raw;
        auto t = juce::String (v, audioDb ? 1 : 3);
        if (t.containsChar ('.')) t = t.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        if (t == "-0") t = "0";
        return audioDb ? t + " dB" : t;
    }
} // namespace csoundMonitor
