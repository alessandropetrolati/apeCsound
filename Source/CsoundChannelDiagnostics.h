#pragma once

#include <JuceHeader.h>
#include <map>
#include <vector>

/*
    Scheda "Channels" (menu principale > Show Channels): TUTTI i canali che
    il motore conosce a runtime, con tipo, direzione, valore e "origine" -
    chi li scrive. La classificazione e' qui, pura, per i test unitari
    (Tests/CsoundUnitTests.cpp); la lettura dal motore sta in
    CsoundAudioProcessor::listAllChannels, la UI in CsoundChannelsPanel.

    Perche': "la patch non suona / lo slider non si muove" quasi sempre e' un
    canale che il codice legge con chnget e che NESSUNO scrive (widget
    Cabbage non importato, slot finiti, nome sbagliato). Qui quei canali
    sono in evidenza, con un bottone "Map" che crea il parametro.
*/
namespace csoundChannels
{
    enum class Type { control, string, audio, pvs, other };

    enum class Origin
    {
        parameter,   // uno slot Float/Int/Bool/Choice del pannello: lo scrive l'host
        host,        // HOST_BPM, IS_PLAYING...: lo scrive apeCsound
        written,     // il codice lo scrive (chnset) e non lo legge: Monitor
        readWritten, // il codice lo legge E lo scrive: autonomo
        orphan       // il codice lo LEGGE (chnget) e nessuno lo scrive: il sospetto
    };

    struct Info
    {
        juce::String name;
        Type type = Type::control;
        bool input = false, output = false;   // flag INPUT/OUTPUT di Csound: chnget -> input, chnset -> output
        Origin origin = Origin::orphan;
        juce::String parameterKind;           // "Float" / "Int" / "Bool" / "Choice" se origin == parameter
        bool hasHints = false;
        double hintMin = 0.0, hintMax = 1.0, hintDefault = 0.0;
    };

    inline Origin classify (bool isParameter, bool isHost, bool input, bool output)
    {
        if (isParameter)       return Origin::parameter;
        if (isHost)            return Origin::host;
        if (input && output)   return Origin::readWritten;
        if (output)            return Origin::written;
        return Origin::orphan;
    }

    inline const char* typeName (Type t)
    {
        switch (t)
        {
            case Type::control: return "k";
            case Type::string:  return "S";
            case Type::audio:   return "a";
            case Type::pvs:     return "f";
            default:            return "?";
        }
    }

    inline const char* originName (Origin o)
    {
        switch (o)
        {
            case Origin::parameter:   return "parameter";
            case Origin::host:        return "host";
            case Origin::written:     return "written by code";
            case Origin::readWritten: return "read & written by code";
            case Origin::orphan:      return "read, nobody writes";
        }
        return "";
    }

    /** Parametro che il codice SCRIVE (chnset) e non legge mai: un
        indicatore GUI importato per sbaglio (LED, meter di Cabbage) che
        occupa uno slot per niente. Candidato a "Unmap". */
    inline bool isNeverReadParameter (const Info& i)
    {
        return i.origin == Origin::parameter && i.output && ! i.input;
    }

    /** Un canale di controllo non-parametro, non-host puo' diventare un
        parametro (Map). Gli audio/pvs no; le stringhe no (nessun tipo di
        parametro stringa). */
    inline bool canMap (const Info& i)
    {
        return i.type == Type::control && i.origin != Origin::parameter && i.origin != Origin::host;
    }

    struct Summary { int total = 0, parameters = 0, host = 0, written = 0, readWritten = 0, orphans = 0; };

    inline Summary summarize (const std::vector<Info>& infos)
    {
        Summary s;
        s.total = (int) infos.size();
        for (const auto& i : infos)
        {
            switch (i.origin)
            {
                case Origin::parameter:   ++s.parameters;  break;
                case Origin::host:        ++s.host;        break;
                case Origin::written:     ++s.written;     break;
                case Origin::readWritten: ++s.readWritten; break;
                case Origin::orphan:      ++s.orphans;     break;
            }
        }
        return s;
    }

    /** Ordine di visualizzazione: prima gli orfani (il motivo della scheda),
        poi parametri, letti&scritti, scritti, host; a parita' per nome. */
    inline void sortForDisplay (std::vector<Info>& infos)
    {
        auto rank = [] (Origin o)
        {
            switch (o)
            {
                case Origin::orphan:      return 0;
                case Origin::parameter:   return 1;
                case Origin::readWritten: return 2;
                case Origin::written:     return 3;
                case Origin::host:        return 4;
            }
            return 5;
        };
        std::stable_sort (infos.begin(), infos.end(), [&] (const Info& a, const Info& b)
        {
            const int ra = rank (a.origin), rb = rank (b.origin);
            return ra != rb ? ra < rb : a.name.compareNatural (b.name, true) < 0;
        });
    }

    /** "Acc1".."Acc16" -> ("Acc", 1): prefisso + indice finale (come il Monitor). */
    inline bool splitIndexedName (const juce::String& name, juce::String& base, int& index)
    {
        const int len = name.length();
        int i = len;
        while (i > 0 && juce::CharacterFunctions::isDigit (name[i - 1])) --i;
        if (i == len || i == 0 || len - i > 4)
            return false;
        int b = i;
        if (b > 0 && (name[b - 1] == '_' || name[b - 1] == ' ' || name[b - 1] == '-')) --b;
        if (b == 0)
            return false;
        base  = name.substring (0, b);
        index = name.substring (i).getIntValue();
        return true;
    }

    /** Famiglie di canali con lo stesso prefisso + indice, stesso tipo e
        stessa origine, almeno minSize membri: una riga sola nella scheda,
        con "Map N". channels: indici in 'infos', ordinati per indice. */
    struct Family
    {
        juce::String base;
        std::vector<int> channels;
        Type type = Type::control;
        Origin origin = Origin::orphan;
    };

    inline std::vector<Family> findFamilies (const std::vector<Info>& infos, int minSize = 3)
    {
        std::vector<Family> out;
        std::vector<juce::String> order;
        std::map<juce::String, std::vector<std::pair<int, int>>> byKey;   // base|type|origin -> (indice, numero)

        for (int i = 0; i < (int) infos.size(); ++i)
        {
            juce::String base; int index = 0;
            if (! splitIndexedName (infos[(size_t) i].name, base, index))
                continue;
            const auto key = base + "|" + juce::String ((int) infos[(size_t) i].type) + "|" + juce::String ((int) infos[(size_t) i].origin);
            if (byKey.find (key) == byKey.end())
                order.push_back (key);
            byKey[key].push_back ({ i, index });
        }

        for (const auto& key : order)
        {
            auto members = byKey[key];
            if ((int) members.size() < minSize)
                continue;
            std::sort (members.begin(), members.end(), [] (auto& a, auto& b) { return a.second < b.second; });
            Family f;
            f.type   = infos[(size_t) members.front().first].type;
            f.origin = infos[(size_t) members.front().first].origin;
            splitIndexedName (infos[(size_t) members.front().first].name, f.base, members.front().second);
            for (const auto& m : members) f.channels.push_back (m.first);
            out.push_back (f);
        }
        return out;
    }
} // namespace csoundChannels
