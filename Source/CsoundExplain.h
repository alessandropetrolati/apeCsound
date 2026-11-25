#pragma once
/*
    CsoundExplain.h - spiegare il codice in parole (prototipo).

    Alessandro Petrolati - www.apesoft.it

    Per capire un algoritmo non basta vederne la struttura: serve sapere
    che cosa fa ogni passo, in termini musicali. Qui si producono:

      - una nota breve per ogni riga con un opcode ("envelope: rises from
        0.001 to 1 in 5 ms, then falls to 0.001 over the rest of the note"),
        da mostrare in grigio accanto alla riga nell'editor;
      - un riassunto di poche righe per ogni strumento ("Each note plays an
        oscillator at the pitch of MIDI note p4...").

    Le frasi vengono da modelli per gli opcode piu' usati, riempiti con i
    valori veri degli argomenti; per gli altri si usa la prima frase della
    descrizione del manuale. Il modello del codice e' quello del grafo
    (CsoundGraphModel.h), il percorso del segnale quello della vista Flow
    (CsoundGraphFlow.h). Testo in inglese, come tutta l'interfaccia.
*/
#include <JuceHeader.h>
#include <map>
#include <set>
#include <vector>
#include "CsoundGraphModel.h"
#include "CsoundGraphFlow.h"
#include "CsoundOpcodeHelp.h"
#include "CsoundGraphStyle.h"

namespace csoundExplain
{
    struct Summary { int line = 0; juce::StringArray text; };   // da mostrare sopra la riga "instr ..."

    struct Explanation
    {
        std::map<int, juce::String> notes;     // riga del file (da 1) -> nota
        std::vector<Summary> summaries;
        std::map<int, char> rates;             // riga -> 'i' (una volta, all'inizio della nota), 'k', 'a'
        std::map<int, std::vector<float>> shapes;   // riga -> forma della tabella creata o usata (64 punti)
        double sr = 44100.0, ksmps = 10.0;     // dall'header (predefiniti di Csound se mancano)
    };

    //==========================================================================
    // Il tempo: i (una volta, quando nasce la nota), k (a ogni ciclo di
    // controllo, sr/ksmps volte al secondo), a (a ogni campione, calcolati
    // ksmps alla volta).
    namespace timing
    {
        inline juce::String number (double v)
        {
            if (std::abs (v - std::round (v)) < 1.0e-6) return juce::String ((juce::int64) std::llround (v));
            return "about " + juce::String ((juce::int64) std::llround (v));
        }
        inline juce::String sectionTitle (char rate, double sr, double ksmps)
        {
            if (rate == 'i') return "Once, when the note starts  (i-time)";
            if (rate == 'k') return "At every control period: " + number (sr / ksmps) + " times a second  (k-rate, ksmps " + number (ksmps) + ")";
            return "At every sample: " + number (sr) + " a second, computed " + number (ksmps) + " at a time  (a-rate)";
        }
        inline juce::String rateWord (char rate)
        {
            return rate == 'i' ? "note start" : rate == 'k' ? "control" : rate == 'a' ? "audio" : "";
        }
        /** Il tempo di un nodo: quello del modello, ma un opcode senza uscite
            e senza variabili (schedule 2, 0, 1) prende il tempo dalla sintassi. */
        inline char rateOf (const csoundGraph::Node& n)
        {
            using K = csoundGraph::NodeKind;
            if (n.kind != K::Opcode && n.kind != K::Expr && n.kind != K::FrameHead && n.kind != K::Jump) return 0;
            if (n.kind == K::Jump && (n.name == "igoto" || n.name == "cigoto")) return 'i';
            bool anyVar = false;
            for (const auto& in : n.ins) anyVar = anyVar || ! in.vars.empty();
            if (n.outs.empty() && ! anyVar && n.kind == K::Opcode)
            {
                if (const auto* h = CsoundOpcodeHelpData::find (juce::String (n.name)))
                {
                    const auto first = juce::String::fromUTF8 (h->syntax).upToFirstOccurrenceOf ("\n", false, false).trim();
                    const auto args = first.fromFirstOccurrenceOf (juce::String (n.name), false, false).trim();
                    if (args.startsWithChar ('i') || args.startsWith ("insnum") || args.startsWith ("\"")) return 'i';
                }
                if (n.name == "schedule" || n.name == "event_i" || n.name == "prints" || n.name == "print") return 'i';
            }
            // sprintfk, strcpyk...: la k finale dice che gira a ogni ciclo di controllo
            if (n.kind == K::Opcode && n.rate != 'a' && n.name.size() > 2 && n.name.back() == 'k'
                && n.name.compare (0, 3, "ink") != 0 && n.name != "metrok")
                return 'k';
            return n.rate == 'a' || n.rate == 'k' || n.rate == 'i' ? n.rate : 'k';
        }
        inline double setting (const csoundGraph::Orchestra& orc, const char* key, double fallback)
        {
            auto it = orc.settings.find (key);
            if (it == orc.settings.end()) return fallback;
            const auto t = juce::String (it->second).trim();
            const double v = t.getDoubleValue();
            return t.containsOnly ("0123456789.") && v > 0.0 ? v : fallback;
        }
    }

    //==========================================================================
    // Tabelle (ftgen, righe "f" dello score): la forma, calcolata qui per le
    // GEN piu' comuni, e una frase. Per le altre (file audio...) niente forma.
    namespace tables
    {
        struct Def { int gen = 0; juce::StringArray args; int line = 0; };

        inline bool numeric (const juce::String& t)
        {
            const auto x = t.trim();
            return x.isNotEmpty() && x.containsOnly ("0123456789.-+eE") && x.containsAnyOf ("0123456789");
        }
        inline std::vector<double> numbers (const juce::StringArray& a)
        {
            std::vector<double> v;
            for (const auto& t : a) { if (! numeric (t)) break; v.push_back (t.trim().getDoubleValue()); }
            return v;
        }

        /** 64 punti normalizzati a -1..1 (o 0..1); vuoto se la GEN non si calcola. */
        inline std::vector<float> shape (int gen, const juce::StringArray& args, int n = 64)
        {
            const auto a = numbers (args);
            std::vector<double> y ((size_t) n, 0.0);
            const double twoPi = juce::MathConstants<double>::twoPi;
            const int g = std::abs (gen);
            auto segments = [&] (bool expo, int stride)
            {
                if (a.size() < 3) return false;
                double total = 0.0;
                for (size_t i = 1; i < a.size(); i += (size_t) stride) total += juce::jmax (0.0, a[i]);
                if (total <= 0.0) return false;
                for (int k = 0; k < n; ++k)
                {
                    double pos = total * k / (n - 1), acc = 0.0;
                    double v = a.back();
                    for (size_t i = 1; i + (size_t) stride - 1 < a.size(); i += (size_t) stride)
                    {
                        const double d = juce::jmax (0.0, a[i]), v0 = a[i - 1], v1 = a[i + (size_t) stride - 1];
                        if (pos <= acc + d || i + (size_t) stride >= a.size())
                        {
                            const double f = d > 0.0 ? juce::jlimit (0.0, 1.0, (pos - acc) / d) : 1.0;
                            v = expo && v0 * v1 > 0.0 ? v0 * std::pow (v1 / v0, f) : v0 + (v1 - v0) * f;
                            break;
                        }
                        acc += d;
                    }
                    y[(size_t) k] = v;
                }
                return true;
            };
            bool ok = true;
            switch (g)
            {
                case 10:
                    for (int k = 0; k < n; ++k)
                        for (size_t h = 0; h < a.size(); ++h) y[(size_t) k] += a[h] * std::sin (twoPi * (double) (h + 1) * k / n);
                    break;
                case 9: case 19:
                {
                    const size_t step = g == 9 ? 3 : 4;
                    for (int k = 0; k < n; ++k)
                        for (size_t i = 0; i + 2 < a.size(); i += step)
                            y[(size_t) k] += a[i + 1] * std::sin (twoPi * a[i] * k / n + a[i + 2] * juce::MathConstants<double>::pi / 180.0)
                                             + (g == 19 && i + 3 < a.size() ? a[i + 3] : 0.0);
                    break;
                }
                case 11:
                {
                    const int nh = a.empty() ? 1 : juce::jmax (1, (int) a[0]);
                    const int lh = a.size() > 1 ? juce::jmax (1, (int) a[1]) : 1;
                    const double r = a.size() > 2 ? a[2] : 1.0;
                    for (int k = 0; k < n; ++k)
                        for (int h = 0; h < juce::jmin (nh, 64); ++h) y[(size_t) k] += std::pow (r, h) * std::cos (twoPi * (lh + h) * k / n);
                    break;
                }
                case 7:  ok = segments (false, 2); break;
                case 5:  ok = segments (true, 2);  break;
                case 16: ok = segments (false, 3); break;
                case 2: case 17:
                    if (a.empty()) { ok = false; break; }
                    for (int k = 0; k < n; ++k) y[(size_t) k] = a[(size_t) juce::jmin ((int) a.size() - 1, k * (int) a.size() / n)];
                    break;
                case 20:
                {
                    const int type = a.empty() ? 1 : (int) a[0];
                    for (int k = 0; k < n; ++k)
                    {
                        const double x = (double) k / (n - 1);
                        y[(size_t) k] = type == 1 ? 0.54 - 0.46 * std::cos (twoPi * x)
                                      : type == 3 ? 1.0 - std::abs (2.0 * x - 1.0)
                                      : type == 4 ? 0.42 - 0.5 * std::cos (twoPi * x) + 0.08 * std::cos (2.0 * twoPi * x)
                                      : type == 6 ? std::exp (-0.5 * std::pow ((x - 0.5) / 0.18, 2.0))
                                                  : 0.5 - 0.5 * std::cos (twoPi * x);
                    }
                    break;
                }
                default: ok = false;
            }
            if (! ok) return {};
            double peak = 0.0;
            for (auto v : y) peak = juce::jmax (peak, std::abs (v));
            std::vector<float> out;
            for (auto v : y) out.push_back ((float) (gen > 0 && peak > 0.0 ? v / peak : juce::jlimit (-1.0, 1.0, v)));
            return out;
        }

        inline juce::String describe (int gen, const juce::StringArray& args)
        {
            const auto a = numbers (args);
            const int g = std::abs (gen);
            auto list = [&a] (size_t from, size_t step, size_t max)
            {
                juce::StringArray t;
                for (size_t i = from; i < a.size() && t.size() < (int) max; i += step) t.add (juce::String (a[i], 3).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."));
                return t.joinIntoString (", ") + (a.size() > from + step * max ? ", ..." : "");
            };
            switch (g)
            {
                case 10:
                {
                    int count = 0;
                    for (auto v : a) count += v != 0.0 ? 1 : 0;
                    if (count == 1 && ! a.empty() && a[0] != 0.0) return "a sine wave (GEN10)";
                    return "a waveform of " + juce::String (count) + " harmonics, strengths " + list (0, 1, 6) + " (GEN10)";
                }
                case 9: case 19: return "a waveform of chosen partials (GEN" + juce::String (g) + ")";
                case 11: return "a pulse of " + (a.empty() ? juce::String ("N") : juce::String ((int) a[0])) + " harmonics (GEN11)";
                case 7:  return "straight segments through " + list (0, 2, 6) + " (GEN07)";
                case 5:  return "exponential segments through " + list (0, 2, 6) + " (GEN05)";
                case 16: return "curved segments through " + list (0, 3, 6) + " (GEN16)";
                case 2:  return "a list of " + juce::String ((int) args.size()) + " values: " + list (0, 1, 6) + " (GEN02)";
                case 17: return "a step table (GEN17)";
                case 20: return "a window shape (GEN20)";
                case 1:  return "the sound file " + args[0].trim() + " (GEN01)";
                case 23: return "values read from the file " + args[0].trim() + " (GEN23)";
                case 41: case 42: return "a table of random values (GEN" + juce::String (g) + ")";
                default: return "GEN" + juce::String (g);
            }
        }
    }

    //==========================================================================
    // Valori in parole
    namespace detail
    {
        using csoundGraphUI::js;

        inline bool isNumber (const juce::String& t)
        {
            return t.isNotEmpty() && t.containsOnly ("0123456789.-+eE") && t.containsAnyOf ("0123456789");
        }

        inline juce::String number (const juce::String& t)
        {
            auto s = juce::String (t.getDoubleValue(), 4);
            if (s.containsChar ('.')) s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
            return s;
        }

        // Contesto: da dove arrivano le variabili del blocco (globali, canali)
        struct Ctx
        {
            const csoundGraph::Orchestra* orc = nullptr;
            int block = -1;
            std::map<juce::String, juce::String> origin;   // "gkVol" -> "from instr 1", "kVol" -> "the VOLUME slider"
            std::map<int, juce::String> pvalues;            // 4 -> "60, 62 in the score"
            std::map<juce::String, tables::Def> tables;     // "giTimbro" / "1" -> definizione
        };

        inline juce::String value (const juce::String& raw, const Ctx& ctx)
        {
            const auto t = raw.trim();
            if (t.isEmpty()) return "-";
            if (isNumber (t)) return number (t);
            if (t == "p3") return "the note duration";
            if (t.length() > 1 && t[0] == 'p' && t.substring (1).containsOnly ("0123456789"))
                if (auto it = ctx.pvalues.find (t.substring (1).getIntValue()); it != ctx.pvalues.end())
                    return t + " (" + it->second + ")";
            if (t.startsWith ("cpsmidinn(") && t.endsWithChar (')'))
                return "MIDI note " + value (t.fromFirstOccurrenceOf ("(", false, false).upToLastOccurrenceOf (")", false, false), ctx);
            if (t.startsWith ("ampdb(") && t.endsWithChar (')'))
                return t.fromFirstOccurrenceOf ("(", false, false).upToLastOccurrenceOf (")", false, false) + " dB";
            if (t.startsWith ("ampdbfs(") && t.endsWithChar (')'))
                return t.fromFirstOccurrenceOf ("(", false, false).upToLastOccurrenceOf (")", false, false) + " dBFS";
            if (t.startsWithChar ('"')) return t;
            // una sola variabile: con la sua origine, se la conosciamo
            if (t.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"))
            {
                auto it = ctx.origin.find (t);
                return it != ctx.origin.end() ? t + " (" + it->second + ")" : t;
            }
            // espressione: le variabili note dentro restano col loro nome
            juce::StringArray from;
            auto hasWord = [&t] (const juce::String& w)         // "gksolo1" non e' dentro "gksolo10"
            {
                for (int at = t.indexOf (w); at >= 0; at = t.indexOf (at + 1, w))
                {
                    const auto before = at > 0 ? t[at - 1] : (juce::juce_wchar) ' ';
                    const auto after = t[at + w.length()];
                    if (! juce::CharacterFunctions::isLetterOrDigit (before) && before != '_' && ! juce::CharacterFunctions::isLetterOrDigit (after) && after != '_')
                        return true;
                }
                return false;
            };
            for (const auto& [var, where] : ctx.origin)
                if (hasWord (var)) from.addIfNotAlreadyThere (var + " " + where);
            for (const auto& [pn, where] : ctx.pvalues)
            {
                const auto name = "p" + juce::String (pn);
                for (int at = t.indexOf (name); at >= 0; at = t.indexOf (at + 1, name))
                {
                    const auto before = at > 0 ? t[at - 1] : (juce::juce_wchar) ' ';
                    const auto after = t[at + name.length()];
                    if (! juce::CharacterFunctions::isLetterOrDigit (before) && before != '_' && ! juce::CharacterFunctions::isDigit (after))
                    { from.addIfNotAlreadyThere (name + ": " + where); break; }
                }
            }
            if (from.size() > 4) { const int more = from.size() - 3; from.removeRange (3, from.size()); from.add (juce::String (more) + " more"); }
            return csoundGraphUI::shorten (t, 32) + (from.isEmpty() ? juce::String() : " (" + from.joinIntoString ("; ") + ")");
        }

        inline juce::String duration (const juce::String& raw)
        {
            const auto t = raw.removeCharacters (" ");
            if (isNumber (t))
            {
                const double s = t.getDoubleValue();
                if (s < 0.0) return "until turned off (negative duration = held)";
                if (s > 0.0 && s < 1.0) return number (juce::String (s * 1000.0, 1)) + " ms";
                return number (t) + " s";
            }
            if (t == "p3") return "the whole note";
            if (t.startsWith ("p3-") || t.startsWith ("p3*")) return "the rest of the note";
            if (t.contains ("p3")) return "a time based on the note duration (" + raw.trim() + ")";
            return raw.trim();
        }

        inline juce::String tableName (const juce::String& raw, const Ctx& ctx)
        {
            const auto t = raw.trim();
            if (t.isEmpty() || t == "-1") return "a sine wave";
            if (auto it = ctx.tables.find (t); it != ctx.tables.end())
            {
                auto d = tables::describe (it->second.gen, it->second.args);
                return "table " + t + " (" + d.upToLastOccurrenceOf (" (GEN", false, false) + ")";
            }
            return "table " + value (t, ctx);
        }

        // linseg / expseg / cosseg: "rises from 0 to 1 in 5 ms, then falls to 0 over the rest of the note"
        inline juce::String segments (const juce::StringArray& a)
        {
            if (a.size() < 3) return {};
            juce::String out;
            juce::String prev = a[0].trim();
            for (int i = 1; i + 1 < a.size(); i += 2)
            {
                const auto dur = duration (a[i]);
                const auto next = a[i + 1].trim();
                juce::String verb = "goes";
                if (isNumber (prev) && isNumber (next))
                    verb = next.getDoubleValue() > prev.getDoubleValue() ? "rises" : next.getDoubleValue() < prev.getDoubleValue() ? "falls" : "stays";
                if (out.isEmpty()) out << "starts at " << (isNumber (prev) ? number (prev) : prev) << ", ";
                else out << ", then ";
                out << verb << " to " << (isNumber (next) ? number (next) : next)
                    << (dur.endsWith ("note") ? " over " : " in ") << dur;
                prev = next;
                if (out.length() > 110) { out << "..."; break; }
            }
            return out;
        }

        inline juce::String firstSentence (const juce::String& d)
        {
            auto s = d.upToFirstOccurrenceOf (". ", false, false).trimCharactersAtEnd (".").trim();
            if (s.isNotEmpty()) s = s.substring (0, 1).toLowerCase() + s.substring (1);
            return s;
        }
    }

    //==========================================================================
    /** Una nota per la riga di un nodo (vuota se non c'e' niente da dire). */
    inline juce::String noteFor (const csoundGraph::Graph& g, const csoundGraph::Node& n, const detail::Ctx& ctx)
    {
        using namespace detail;
        using K = csoundGraph::NodeKind;
        juce::StringArray a;
        for (const auto& in : n.ins) a.add (js (in.text));
        auto arg = [&a, &ctx] (int i) { return i < a.size() ? value (a[i], ctx) : juce::String ("-"); };
        const auto op = js (n.name);
        const auto out = n.outs.empty() ? juce::String() : js (n.outs[0].var);

        if (n.kind == K::FrameHead)
        {
            if (op == "while") return "repeats the lines below while " + js (n.text);
            if (op == "until") return "repeats the lines below until " + js (n.text);
            return "the lines below run only if " + js (n.text);
        }
        if (n.kind == K::Jump)
        {
            if (op.startsWith ("if "))          // if cond kgoto LABEL
            {
                const auto t = js (n.text);
                const auto cond = t.upToLastOccurrenceOf (" -> ", false, false), label = t.fromLastOccurrenceOf (" -> ", false, false);
                return "jumps to " + label + " when " + cond + (op.endsWith ("igoto") ? " (only at note start)" : op.endsWith ("kgoto") ? " (only while playing, not at note start)" : juce::String());
            }
            return op == "igoto" || op == "cigoto" ? "jumps only at note start: the lines skipped still run later (use goto)"
                                                   : "jumps to " + js (n.text);
        }
        if (n.kind != K::Opcode && n.kind != K::Expr) return {};

        if (n.kind == K::Expr)
        {
            const auto rhs = js (n.text).trim();
            // "gaRev += aSig * 0.3": si somma (o si toglie) a una variabile, tipicamente un bus
            if (n.accumulates && (op == "+=" || op == "-="))
                return (op == "+=" ? "adds " : "subtracts ") + rhs + (op == "+=" ? " to " : " from ") + out
                       + (out.startsWith ("ga") ? " (a bus shared with other instruments)" : juce::String());
            // "aS = aS * aEnv": il segnale moltiplicato per un inviluppo
            if (rhs.containsChar ('*') && n.rate == 'a')
            {
                juce::StringArray factors;              // solo i * fuori dalle parentesi
                {
                    int depth = 0; juce::String curF;
                    for (auto ch : rhs)
                    {
                        if (ch == '(' || ch == '[') ++depth;
                        if (ch == ')' || ch == ']') --depth;
                        if (ch == '*' && depth == 0) { factors.add (curF.trim()); curF.clear(); continue; }
                        curF << juce::String::charToString (ch);
                    }
                    factors.add (curF.trim());
                }
                juce::String env;
                for (auto& f : factors)
                    for (const auto& nd : g.nodes)
                        for (const auto& o : nd.outs)
                            if (js (o.var) == f && (nd.name == "expseg" || nd.name == "linseg" || nd.name == "madsr" || nd.name == "adsr"
                                                    || nd.name == "mxadsr" || nd.name == "linen" || nd.name == "transeg" || nd.name == "cosseg" || nd.name == "expon" || nd.name == "line"))
                                env = f;
                if (env.isNotEmpty()) return "the envelope " + env + " shapes the volume of the sound";
                if (factors.size() > 4) return "multiplies " + juce::String (factors.size()) + " factors (changes the level)";
                return "multiplies " + factors.joinIntoString (" by ") + " (changes the level)";
            }
            if (rhs.containsChar ('+') && n.rate == 'a' && ! n.accumulates) return "mixes " + rhs.replace ("+", " and ");
            if (n.accumulates && n.rate == 'a') return "adds this signal to the bus " + out;
            if (rhs.startsWith ("p") && detail::isNumber (rhs.substring (1))) return out + " = note parameter " + rhs;
            if (rhs.contains ("/ 127") || rhs.contains ("/127")) return "turns the velocity (0-127) into an amplitude";
            if (rhs.containsChar ('?')) return "chooses a value: " + rhs;
            if (! n.accumulates && out.isNotEmpty() && isNumber (rhs)) return out + " is the constant " + number (rhs);
            if (! n.accumulates && out.isNotEmpty() && rhs.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"))
                return "copies " + value (rhs, ctx) + " into " + out;
            if (n.accumulates && (op == "*=" || op == "/=")) return (op == "*=" ? "multiplies " : "divides ") + out + " by " + value (rhs, ctx);
            if (out.isNotEmpty() && rhs.isNotEmpty())
            {
                // un calcolo: si dice da dove vengono i valori che usa
                const auto v = value (rhs, ctx);
                if (v.contains ("(") && v != rhs)
                    return "computes " + out + " from " + csoundGraphUI::shorten (v.fromFirstOccurrenceOf ("(", false, false).upToLastOccurrenceOf (")", false, false), 140);
            }
            return {};
        }

        // --- oscillatori ---
        if (op == "oscili" || op == "oscil" || op == "poscil" || op == "poscil3" || op == "oscil3" || op == "oscilikt")
            return "oscillator: " + juce::String (a[1].trim().startsWith ("cpsmidinn") ? "pitch " : "frequency ") + arg (1) + ", level " + arg (0) + ", waveform " + tableName (a[2], ctx);
        if (op == "vco2")  return "analog-style oscillator (saw by default): frequency " + arg (1) + ", level " + arg (0);
        if (op == "vco")   return "analog-style oscillator: frequency " + arg (1) + ", level " + arg (0);
        if (op == "buzz" || op == "gbuzz") return "pulse of harmonics (buzz): frequency " + arg (1) + ", level " + arg (0);
        if (op == "pluck") return "plucked string: frequency " + arg (1) + ", level " + arg (0);
        if (op == "noise" || op == "rand") return "white noise, level " + arg (0);
        if (op == "pinker" || op == "pinkish") return "pink noise";
        if (op == "diskin2" || op == "diskin" || op == "soundin") return "plays the sound file " + arg (0);
        if (op == "loscil" || op == "loscil3" || op == "flooper2") return "plays a sample from " + tableName (a[2], ctx);
        if (op == "fof" || op == "fof2") return "formant grains (FOF): fundamental " + arg (1) + ", formant " + arg (2);
        if (op == "grain" || op == "grain3" || op == "granule" || op == "partikkel" || op == "syncgrain") return "granular synthesis: many short grains of sound";

        // --- inviluppi e curve ---
        if (op == "expseg" || op == "expsega" || op == "expsegr" || op == "linseg" || op == "linsegr" || op == "cosseg")
            return "envelope (" + juce::String (op.startsWith ("expseg") ? "exponential" : op == "cosseg" ? "smooth" : "straight") + "): " + segments (a);
        if (op == "line" || op == "expon") return "line: from " + arg (0) + " to " + arg (2) + " in " + duration (a[1]);
        if (op == "linen") return "envelope: level " + arg (0) + ", fade in " + duration (a[1]) + ", fade out " + duration (a[3]);
        if (op == "madsr" || op == "adsr" || op == "mxadsr" || op == "mdsr")
            return "ADSR envelope: attack " + duration (a[0]) + ", decay " + duration (a[1]) + ", sustain " + arg (2) + ", release " + duration (a[3]);
        if (op == "transeg") return "envelope with shaped curves: starts at " + arg (0);

        // --- modulatori ---
        if (op == "lfo") return "LFO: slow periodic movement, depth " + arg (0) + ", speed " + arg (1) + " Hz";
        if (op == "rspline") return "random curve: moves smoothly between " + arg (0) + " and " + arg (1) + ", " + arg (2) + "-" + arg (3) + " changes per second";
        if (op == "jspline") return "jitter: random curve of size " + arg (0) + ", " + arg (1) + "-" + arg (2) + " changes per second";
        if (op == "random" || op == "randomi" || op == "randomh") return "random value: between " + arg (0) + " and " + arg (1);
        if (op == "randi" || op == "randh") return "random values: within +/-" + arg (0) + ", " + arg (1) + " per second";
        if (op == "port" || op == "portk" || op == "lag" || op == "tonek") return "smooths " + arg (0) + " (no jumps)";

        // --- filtri ---
        if (op == "moogladder" || op == "moogladder2" || op == "moogvcf" || op == "lpf18" || op == "zdf_ladder")
            return "low-pass filter (analog style): cuts the highs of " + arg (0) + " above " + arg (1) + ", resonance " + arg (2);
        if (op == "butterlp" || op == "butlp" || op == "tone" || op == "tonex")
            return "low-pass filter: keeps " + arg (0) + " below " + arg (1);
        if (op == "butterhp" || op == "buthp" || op == "atone" || op == "atonex")
            return "high-pass filter: keeps " + arg (0) + " above " + arg (1);
        if (op == "butterbp" || op == "butbp" || op == "reson" || op == "resonz")
            return "band-pass filter: keeps " + arg (0) + " around " + arg (1) + ", width " + arg (2);
        if (op == "bqrez" || op == "svfilter" || op == "statevar") return "resonant filter on " + arg (0) + ", cutoff " + arg (1);

        // --- effetti ---
        if (op == "reverbsc") return "stereo reverb: room size " + arg (2) + ", brightness " + arg (3);
        if (op == "freeverb") return "stereo reverb: room size " + arg (2) + ", damping " + arg (3);
        if (op == "delay" || op == "vdelay" || op == "vdelay3" || op == "vdelayx") return "delay: " + arg (0) + " is heard again after " + duration (a[1]);
        if (op == "delayr") return "opens a delay line of " + duration (a[0]);
        if (op == "delayw") return "writes " + arg (0) + " into the delay line";
        if (op == "deltap" || op == "deltapi" || op == "deltap3") return "reads the delay line " + duration (a[0]) + " back";
        if (op == "flanger") return "flanger on " + arg (0);
        if (op == "distort" || op == "distort1" || op == "clip") return "distortion / clipping of " + arg (0);
        if (op == "balance") return "matches the level of " + arg (0) + " to " + arg (1);
        if (op == "pan2") return "places " + arg (0) + " between left and right at " + arg (1);

        // --- uscite, canali, eventi ---
        if (op == "outs") return a.size() >= 2 && a[0].trim() == a[1].trim() ? "output: " + a[0].trim() + " to both speakers (left and right)"
                                                                               : "output: " + a[0].trim() + " to the left, " + a[1].trim() + " to the right";
        if (op == "out" || op == "outc" || op == "outall") return "output: " + a.joinIntoString (", ");
        if (op == "outch") return "output: audio to channel " + arg (0);
        if (op == "inch" || op == "ins" || op == "in") return "reads the audio input";
        if (op == "chnget" || op == "chngetk" || op == "chngeti" || op == "chnget_i")
        {
            const auto ch = js (n.channel);
            if (ch.isEmpty()) return "reads the channel whose name is in " + arg (0);
            bool host = false;
            if (ctx.orc != nullptr) for (const auto& hc : ctx.orc->hostChannels) host = host || js (hc) == ch;
            return host ? "reads the \"" + ch + "\" control (a slider of the Parameters panel or the DAW)"
                        : "reads the channel \"" + ch + "\"";
        }
        if (op == "chnset" || op == "chnsetk" || op == "chnseti" || op == "chnset_i")
            return n.channel.empty() ? "writes " + arg (0) + " to the channel whose name is in " + arg (1)
                                     : "writes " + arg (0) + " to the channel \"" + js (n.channel) + "\" (shown in the Monitor)";
        if (op == "schedule" || op == "schedulek" || op == "event_i" || op == "event")
            {
                const auto d = duration (a[op.startsWith ("event") ? 3 : 2]);
                return "starts a note of instr " + js (n.eventTarget) + (d.startsWith ("until") ? ", held " + d : ", lasting " + d);
            }
        if (op == "turnoff") return "stops this note";
        if (op == "xin")  return "the inputs of this opcode";
        if (op == "xout") return "returns " + a.joinIntoString (", ") + " to the caller";

        // --- conversioni ---
        if (op == "ampdb") return "converts " + arg (0) + " dB into an amplitude";
        if (op == "cpsmidinn") return "converts MIDI note " + arg (0) + " into Hz";
        if (op == "ftgen" || op == "ftgenonce")
        {
            juce::StringArray rest;
            for (int i = 4; i < a.size(); ++i) rest.add (a[i]);
            return "creates table " + (out.isNotEmpty() ? out : arg (0)) + ": " + tables::describe (a[3].trim().getIntValue(), rest);
        }
        if (op == "sprintf" || op == "sprintfk") return "builds a text";
        if (op == "prints" || op == "printks" || op == "print" || op == "printk" || op == "printf") return "prints to the console";

        // --- valori, tabelle, array ---
        auto is = [&op] (std::initializer_list<const char*> names) { for (auto* x : names) if (op == x) return true; return false; };
        const auto outName = out.isNotEmpty() ? out : juce::String ("the result");
        if (op == "init") return outName + " starts at " + arg (0) + " (set once, when the note starts)";
        if (is ({ "table", "tablei", "table3", "tab", "tab_i", "tablekt", "tableikt" }))
            return "reads " + tableName (a[1], ctx) + " at position " + arg (0) + (a[2].trim() == "1" ? " (0-1 = the whole table)" : juce::String())
                   + (op.endsWith ("i") || op == "table3" ? ", interpolating" : juce::String());
        if (is ({ "tablew", "tabw", "tabw_i", "tableiw" })) return "writes " + arg (0) + " into " + tableName (a[2], ctx) + " at position " + arg (1);
        if (op == "fillarray") return "fills the array " + outName + " with " + a.joinIntoString (", ");
        if (op == "lenarray") return "the number of elements of " + arg (0);
        if (op == "ftlen") return "the length (number of points) of " + tableName (a[0], ctx);
        if (op == "ftgentmp") return "creates a temporary table (GEN " + arg (3) + "), freed when the note ends";
        if (is ({ "vaget" })) return "reads sample " + arg (0) + " of the audio block " + arg (1) + " (one of the ksmps samples)";
        if (is ({ "vaset" })) return "writes " + arg (0) + " into sample " + arg (1) + " of the audio block " + arg (2);
        if (op == "clear") return "sets " + a.joinIntoString (", ") + " to 0 (an audio bus emptied after use, ready for the next cycle)";

        // --- conversioni e range ---
        if (op == "downsamp") return "takes one value per control period from the audio " + arg (0) + " (audio to control)";
        if (op == "upsamp")   return "turns the control signal " + arg (0) + " into audio (steps)";
        if (op == "interp")   return "turns the control signal " + arg (0) + " into smooth audio (interpolated)";
        if (op == "limit")    return "keeps " + arg (0) + " between " + arg (1) + " and " + arg (2);
        if (op == "wrap")     return "wraps " + arg (0) + " around between " + arg (1) + " and " + arg (2);
        if (op == "mirror")   return "reflects " + arg (0) + " back inside " + arg (1) + " - " + arg (2);
        if (op == "scale")    return "scales " + arg (0) + " (0-1) into the range " + arg (2) + " - " + arg (1);
        if (op == "ntrpol")   return "crossfades between " + arg (0) + " and " + arg (1) + " by " + arg (2) + " (0 = first, 1 = second)";
        if (op == "cpspch")   return "converts the pitch " + arg (0) + " (octave.semitone, 8.00 = middle C) into Hz";
        if (op == "cpsoct")   return "converts the octave value " + arg (0) + " (8 = middle C) into Hz";
        if (op == "mtof")     return "converts MIDI note " + arg (0) + " into Hz";
        if (op == "ftom")     return "converts " + arg (0) + " Hz into a MIDI note number";
        if (op == "dbamp")    return "converts the amplitude " + arg (0) + " into dB";
        if (op == "rms")      return "measures the level (RMS) of " + arg (0);
        if (op == "follow" || op == "follow2") return "follows the loudness (envelope) of " + arg (0);
        if (op == "max_k")    return "the peak of " + arg (0) + " during each period (a level meter)";
        if (op == "dcblock" || op == "dcblock2") return "removes the DC offset (a constant shift) from " + arg (0);
        if (op == "sum")      return "adds " + a.joinIntoString (" + ");

        // --- tempo, trigger, MIDI ---
        if (op == "phasor")   return "a ramp from 0 to 1, " + arg (0) + " times per second (a phase, e.g. to read a table)";
        if (op == "metro")    return "a tick " + arg (0) + " times per second (1 on the tick, otherwise 0)";
        if (op == "trigger")  return "1 when " + arg (0) + " crosses " + arg (1) + ", otherwise 0";
        if (op == "changed" || op == "changed2")
            return "1 when " + (a.size() > 4 ? "any of " + juce::String (a.size()) + " values" : a.joinIntoString (", ")) + " changes, otherwise 0";
        if (op == "timeinstk" || op == "timeinsts") return "the time since the note started (" + juce::String (op == "timeinsts" ? "seconds" : "control periods") + ")";
        if (op == "times" || op == "timek") return "the time since Csound started";
        if (op == "release")  return "1 while the note is releasing (after note-off), otherwise 0";
        if (op == "xtratim")  return "keeps the note alive " + duration (a[0]) + " longer (time for the release)";
        if (op == "active")   return "how many notes of instr " + arg (0) + " are playing";
        if (op == "cpsmidi")  return "the frequency (Hz) of the MIDI key that started the note";
        if (op == "notnum")   return "the MIDI note number of the key that started the note";
        if (op == "veloc")    return "the MIDI velocity of the note" + (a.size() >= 2 ? ", scaled to " + arg (0) + "-" + arg (1) : juce::String (" (0-127)"));
        if (op == "ampmidi")  return "the MIDI velocity, scaled to 0-" + arg (0);
        if (op == "massign")  return "MIDI channel " + (a[0].trim() == "0" ? juce::String ("(all)") : arg (0)) + " plays instr " + arg (1);
        if (op == "seed")     return "sets the random generator" + juce::String (a[0].trim() == "0" ? " (0: different at every run)" : " (same numbers at every run)");
        if (op == "setksmps") return "this block uses a smaller control period: ksmps = " + arg (0);
        if (op == "rireturn") return "end of the lines repeated by reinit";
        if (op == "schedkwhen" || op == "schedkwhennamed")
            return "when " + arg (0) + " is not 0, starts a note of instr " + js (n.eventTarget) + ", lasting " + duration (a[5]);
        if (op == "turnoff2" || op == "turnoff2_i") return "stops the notes of instr " + arg (0);
        if (op == "exitnow")  return "stops Csound";
        if (op == "alwayson") return "keeps instr " + arg (0) + " always on (no score needed)";
        if (op == "connect")  return "connects output \"" + a[1].trim().unquoted() + "\" of instr " + arg (0) + " to input \"" + a[3].trim().unquoted() + "\" of instr " + arg (2);
        if (op == "outleta" || op == "outletk") return "sends " + arg (1) + " out of the outlet " + arg (0) + " (see connect)";
        if (op == "inleta" || op == "inletk") return "receives the inlet " + arg (0) + " (see connect)";
        if (op == "chn_k" || op == "chn_a" || op == "chn_S") return "declares the channel " + arg (0) + " (mode " + arg (1) + ": 1 in, 2 out, 3 both)";

        // --- spettro ---
        if (op == "pvsanal") return "spectral analysis of " + arg (0) + " (FFT size " + arg (1) + ", hop " + arg (2) + ")";
        if (op == "pvsynth") return "turns the spectrum " + arg (0) + " back into audio";
        if (op == "pvscale") return "shifts the pitch of the spectrum " + arg (0) + " by the ratio " + arg (1);
        if (op == "pvsmorph") return "morphs between the spectra " + arg (0) + " and " + arg (1);
        if (op == "pvsblur") return "blurs the spectrum " + arg (0) + " over time";
        if (op == "pvsfreeze") return "freezes the spectrum " + arg (0) + " (amplitude " + arg (1) + ", frequency " + arg (2) + ")";

        // --- altri suoni ed effetti ---
        if (op == "oscils") return "sine oscillator: frequency " + arg (1) + ", level " + arg (0);
        if (op == "reverb" || op == "reverb2" || op == "nreverb") return "reverb on " + arg (0) + ", time " + duration (a[1]);
        if (op == "comb" || op == "vcomb") return "comb filter (echoes) on " + arg (0) + ", time " + duration (a[1]);
        if (op == "lowpass2") return "resonant low-pass filter: keeps " + arg (0) + " below " + arg (1);
        if (op == "clfilt") return "filter on " + arg (0) + ", cutoff " + arg (1);
        if (op == "compress" || op == "compress2" || op == "dam") return "compressor on " + arg (0);
        if (op == "hilbert") return "splits " + arg (0) + " into two signals 90 degrees apart (for frequency shifting)";
        if (op == "pan") return "places " + arg (0) + " in space";
        if (op == "rand" || op == "random") return "random value";

        // --- funzioni matematiche (anche scritte come opcode) ---
        if (op == "int" || op == "round" || op == "floor" || op == "ceil") return "rounds " + arg (0) + (op == "int" ? " (integer part)" : op == "round" ? " to the nearest integer" : op == "floor" ? " down" : " up");
        if (op == "frac") return "the fractional part of " + arg (0);
        if (op == "abs")  return "the absolute value of " + arg (0);
        if (op == "sqrt") return "the square root of " + arg (0);
        if (op == "pow")  return arg (0) + " to the power of " + arg (1);
        if (op == "exp")  return "e to the power of " + arg (0);
        if (op == "max" || op == "min") return "the " + juce::String (op == "max" ? "largest" : "smallest") + " of " + a.joinIntoString (", ");
        if (op == "sin" || op == "cos" || op == "tan") return op + " of " + arg (0) + " (radians)";
        if (op == "linlin") return "maps " + arg (0) + " from the range " + arg (3) + "-" + arg (4) + " to " + arg (1) + "-" + arg (2);
        if (op == "octpch") return "converts the pitch " + arg (0) + " into an octave value";
        if (op == "pchbend") return "the MIDI pitch bend, scaled to " + arg (0) + " - " + arg (1);
        if (op == "peak") return "the highest absolute value of " + arg (0) + " so far";
        if (op == "mpulse") return "a single-sample click every " + duration (a[1]) + ", amplitude " + arg (0);
        if (op == "chnmix") return "adds " + arg (0) + " into the channel " + arg (1) + " (a mix bus)";
        if (op == "chnclear") return "empties the channel " + a.joinIntoString (", ");
        if (op == "zakinit") return "creates the zak buses: " + arg (0) + " audio, " + arg (1) + " control";
        if (is ({ "zar", "zkr", "zir" })) return "reads zak bus " + arg (0);
        if (is ({ "zaw", "zkw", "ziw", "zawm", "zkwm" })) return "writes " + arg (0) + " into zak bus " + arg (1) + (op.endsWith ("m") ? " (mixing)" : juce::String());
        if (op == "alpass" || op == "valpass") return "all-pass filter (diffusion, part of a reverb) on " + arg (0);
        if (op == "lowres" || op == "lowresx") return "resonant low-pass filter on " + arg (0) + ", cutoff " + arg (1);
        if (op == "pareq") return "equalizer band on " + arg (0) + " at " + arg (1) + " Hz";
        if (op == "outq") return "output: four channels (" + a.joinIntoString (", ") + ")";
        if (op == "subinstr") return "plays instr " + arg (0) + " inside this one and takes its sound";
        if (op == "transegr") return "envelope with shaped curves and a release segment";

        // --- testo e stampa ---
        if (is ({ "strcat", "strcatk" })) return "joins the texts " + a.joinIntoString (" and ");
        if (is ({ "strsub", "strsubk" })) return "takes a part of the text " + arg (0);
        if (is ({ "strcpy", "strcpyk" })) return "copies the text " + arg (0);
        if (is ({ "strget" })) return "reads a text from the string table " + arg (0);
        if (is ({ "println", "puts", "printf_i", "printk", "printk2", "fprints", "printarray" })) return "prints to the console";
        if (is ({ "fout", "fouti", "foutk" })) return "writes to the file " + arg (0);

        // altrimenti: la descrizione del manuale, prima frase
        if (const auto* h = CsoundOpcodeHelpData::find (op))
            return firstSentence (juce::String::fromUTF8 (h->description));
        return {};
    }

    //==========================================================================
    inline juce::StringArray summaryFor (const csoundGraph::Orchestra& orc, int b, const detail::Ctx& ctx)
    {
        using namespace detail;
        juce::StringArray lines;
        const auto& g = orc.blocks[(size_t) b];
        const auto flow = csoundFlow::build (orc, b);

        // la catena audio, in parole
        std::vector<int> spine;
        for (int i = 0; i < (int) flow.boxes.size(); ++i) if (flow.boxes[(size_t) i].spine) spine.push_back (i);
        std::sort (spine.begin(), spine.end(), [&flow] (int x, int y) { return flow.boxes[(size_t) x].depth < flow.boxes[(size_t) y].depth; });

        auto phrase = [&] (int box)
        {
            const auto& nd = g.nodes[(size_t) flow.boxes[(size_t) box].node];
            auto t = noteFor (g, nd, ctx);
            return t.upToFirstOccurrenceOf (":", false, false).trim();     // "oscillator", "low-pass filter (analog style)", ...
        };

        // "envelope (exponential)" -> "an exponential envelope"; "oscillator" -> "an oscillator"
        auto noun = [] (juce::String p)
        {
            if (p.contains ("(") && p.endsWithChar (')'))
            {
                const auto kind = p.fromFirstOccurrenceOf ("(", false, false).upToLastOccurrenceOf (")", false, false);
                const auto base = p.upToFirstOccurrenceOf ("(", false, false).trim();
                if (! kind.containsChar (' ') || kind.endsWith ("style")) p = kind + " " + base;
            }
            const bool vowel = juce::String ("aeiouAEIOU").containsChar (p[0]);
            return (vowel ? "an " : "a ") + p;
        };
        // dettaglio utile dalla nota completa: "pitch MIDI note p4" -> "(pitch from MIDI note p4)"
        auto detailOf = [&] (int box)
        {
            const auto& nd = g.nodes[(size_t) flow.boxes[(size_t) box].node];
            const auto t = noteFor (g, nd, ctx).fromFirstOccurrenceOf (":", false, false).trim();
            auto first = t.upToFirstOccurrenceOf (",", false, false).trim();
            if (first.containsChar ('(') && ! first.containsChar (')'))     // "(440, 660 in the score)" tagliata dalla virgola
                first = first.upToFirstOccurrenceOf (" (", false, false);
            if (first.startsWith ("pitch ")) return " (pitch from " + first.substring (6) + ")";
            if (first.startsWith ("frequency ") && first.length() < 40) return " (" + first + ")";
            return juce::String();
        };

        if (flow.audio && ! spine.empty())
        {
            juce::StringArray steps;           // generatore e trasformazioni, in ordine
            juce::String outputPhrase;
            for (int i : spine)
            {
                const auto& bx = flow.boxes[(size_t) i];
                if (bx.group == "expression") continue;
                if (bx.select)                  // if / else che sceglie il segnale
                {
                    if (bx.params.empty()) continue;      // solo una condizione, nessuna scelta
                    const auto step = "a choice (" + bx.title + ": " + bx.params.front().text + ", otherwise "
                                    + (bx.params.size() > 1 ? bx.params.back().text : juce::String ("unchanged")) + ")";
                    if (step.length() < 90 && ! bx.params.empty()) steps.addIfNotAlreadyThere (step);
                    continue;
                }
                const auto p = phrase (i);
                if (p.isEmpty() || p.length() > 48) continue;
                if (p == "output")
                {
                    const auto& nd = g.nodes[(size_t) bx.node];
                    outputPhrase = nd.name == "outs" ? (nd.ins.size() >= 2 && nd.ins[0].text == nd.ins[1].text ? "sent to both speakers" : "sent to the left and right speakers")
                                                     : "sent to the outputs";
                    continue;
                }
                const bool shaperStep = p.startsWith ("envelope") || p.startsWith ("ADSR") || p.startsWith ("line") || p.startsWith ("LFO") || p.startsWith ("random") || p.startsWith ("jitter");
                if (shaperStep) continue;      // va fra chi "modella" (sotto)
                const auto step = noun (p) + (steps.isEmpty() ? detailOf (i) : juce::String());
                if (! steps.contains (step)) steps.add (step);
            }
            if (outputPhrase.isEmpty())
                for (int i : spine)
                    if (flow.boxes[(size_t) i].sink && flow.boxes[(size_t) i].outNote.isNotEmpty())
                        outputPhrase = "sent to " + flow.boxes[(size_t) i].outNote;

            juce::StringArray shapers, controls, audioIn;
            for (int i = 0; i < (int) flow.boxes.size(); ++i)
            {
                const auto& bx = flow.boxes[(size_t) i];
                if (bx.spine || bx.group == "expression") continue;
                if (bx.source && bx.rate == 'a')
                {
                    audioIn.addIfNotAlreadyThere (bx.title.replace ("receive ", ""));
                    continue;
                }
                if (bx.source)
                {
                    controls.addIfNotAlreadyThere (bx.title.startsWith ("chnget") ? "the " + bx.title.fromFirstOccurrenceOf ("\"", true, false) + " control"
                                                                                 : bx.title.replace ("receive ", "") + " (" + bx.role + ")");
                    continue;
                }
                const auto p = phrase (i);
                const bool shaper = p.startsWith ("envelope") || p.startsWith ("ADSR") || p.startsWith ("line") || p.startsWith ("LFO") || p.startsWith ("random") || p.startsWith ("jitter");
                if (shaper) shapers.addIfNotAlreadyThere (noun (p));
            }
            for (int i : spine)       // inviluppi sulla catena stessa (aS = aS * aEnv... o un inviluppo audio)
            {
                const auto p = phrase (i);
                if (p.startsWith ("envelope") || p.startsWith ("ADSR")) shapers.addIfNotAlreadyThere (noun (p));
            }

            auto list = [] (const juce::StringArray& a)
            {
                if (a.size() <= 1) return a.joinIntoString ("");
                juce::StringArray head (a);
                const auto last = head[head.size() - 1];
                head.remove (head.size() - 1);
                return head.joinIntoString (", ") + " and " + last;
            };

            if (shapers.size() > 3) { shapers.removeRange (3, shapers.size()); shapers.add ("more"); }
            juce::String s = g.kind == csoundGraph::Graph::Udo ? "This opcode uses " : audioIn.isEmpty() ? "Each note plays " : "Each note takes the audio of ";
            if (! audioIn.isEmpty())
            {
                s << list (audioIn);
                if (! steps.isEmpty()) s << " through ";
            }
            if (steps.isEmpty()) s << (audioIn.isEmpty() ? "a sound" : "");
            else
            {
                s << steps[0];
                if (steps.size() > 1)
                {
                    juce::StringArray rest (steps);
                    rest.remove (0);
                    s << ", then " << list (rest);
                }
            }
            if (! shapers.isEmpty()) s << ", shaped by " << list (shapers);
            if (outputPhrase.isNotEmpty()) s << ", " << outputPhrase;
            lines.add (s + ".");
            if (controls.size() > 6)
            {
                const int more = controls.size() - 5;
                controls.removeRange (5, controls.size());
                controls.add (juce::String (more) + " more");
            }
            if (! controls.isEmpty()) lines.add ("It is controlled by " + list (controls) + ".");
        }
        for (const auto& ev : flow.events)
            lines.add ("Starts notes of instr " + ev.target + (ev.frames.isEmpty() ? juce::String() : ", " + ev.frames[ev.frames.size() - 1].replace ("while", "repeated while")) + ".");
        if (! g.pfields.empty() && g.kind == csoundGraph::Graph::Instr)
        {
            juce::String pf;
            for (int p : g.pfields) if (p > 3) pf << "p" << p << " ";
            if (pf.isNotEmpty()) lines.add ("Each note brings its own values: " + pf.trim().replace (" ", ", ") + ".");
        }
        return lines;
    }

    //==========================================================================
    inline Explanation explain (const csoundGraph::Orchestra& orc)
    {
        using csoundGraphUI::js;
        Explanation ex;
        ex.sr = timing::setting (orc, "sr", 44100.0);
        const double kr = timing::setting (orc, "kr", 0.0);
        ex.ksmps = timing::setting (orc, "ksmps", kr > 0.0 ? ex.sr / kr : 10.0);

        // tabelle: ftgen nel codice, righe "f" nello score
        std::map<juce::String, tables::Def> defs;
        for (const auto& g : orc.blocks)
            for (const auto& nd : g.nodes)
                if ((nd.name == "ftgen" || nd.name == "ftgenonce") && nd.ins.size() >= 4)
                {
                    tables::Def d;
                    d.gen = js (nd.ins[3].text).trim().getIntValue();
                    for (size_t i = 4; i < nd.ins.size(); ++i) d.args.add (js (nd.ins[i].text));
                    d.line = nd.line;
                    if (! nd.outs.empty()) defs[js (nd.outs[0].var)] = d;
                    const auto num = js (nd.ins[0].text).trim();
                    if (tables::numeric (num) && num.getIntValue() > 0) defs[num] = d;
                }
        for (const auto& sl : orc.scoreLines)
        {
            if (sl.p.size() < 4) continue;
            if (sl.kind == 'f')
            {
                tables::Def d;
                d.gen = js (sl.p[3]).getIntValue();
                for (size_t i = 4; i < sl.p.size(); ++i) d.args.add (js (sl.p[i]));
                d.line = sl.line;
                defs[js (sl.p[0])] = d;
                ex.notes[sl.line] = "table " + js (sl.p[0]) + ": " + tables::describe (d.gen, d.args);
            }
            else
            {
                juce::String t = "note of instr " + js (sl.instr) + ": at " + js (sl.p[1]) + " s, ";
                const double dur = js (sl.p[2]).getDoubleValue();
                t << (dur < 0.0 ? juce::String ("held until turned off") : "lasting " + js (sl.p[2]) + " s");
                juce::StringArray more;
                for (size_t i = 3; i < sl.p.size() && more.size() < 6; ++i) more.add ("p" + juce::String ((int) i + 1) + " = " + js (sl.p[i]));
                if (! more.isEmpty()) t << ", " << more.joinIntoString (", ");
                ex.notes[sl.line] = t;
            }
        }
        for (const auto& [key, d] : defs)
        {
            auto sh = tables::shape (d.gen, d.args);
            if (! sh.empty() && d.line > 0) ex.shapes[d.line] = sh;
        }

        for (int b = 0; b < (int) orc.blocks.size(); ++b)
        {
            const auto& g = orc.blocks[(size_t) b];
            detail::Ctx ctx;
            ctx.orc = &orc;
            ctx.block = b;
            // origine delle variabili: globali da altri blocchi, canali letti
            for (const auto& l : orc.links)
                if (l.to == b && l.from >= 0 && l.kind != "evento")
                    ctx.origin[js (l.name)] = "from " + js (orc.blocks[(size_t) l.from].title());
            for (const auto& nd : g.nodes)
                if (! nd.channel.empty() && ! nd.channelWrite && ! nd.outs.empty())
                    ctx.origin[js (nd.outs[0].var)] = "the \"" + js (nd.channel) + "\" control";
            ctx.tables = defs;

            // i p-field della nota: dallo score e da chi la fa partire
            if (g.kind == csoundGraph::Graph::Instr)
            {
                std::map<int, juce::StringArray> fromScore;
                for (const auto& sl : orc.scoreLines)
                    if (sl.kind == 'i' && std::find (g.names.begin(), g.names.end(), sl.instr) != g.names.end())
                        for (size_t i = 3; i < sl.p.size(); ++i) fromScore[(int) i + 1].addIfNotAlreadyThere (js (sl.p[i]));
                std::map<int, juce::StringArray> fromEvents;
                for (size_t ob = 0; ob < orc.blocks.size(); ++ob)
                    for (const auto& nd : orc.blocks[ob].nodes)
                    {
                        if (nd.eventTarget.empty() || std::find (g.names.begin(), g.names.end(), nd.eventTarget) == g.names.end()) continue;
                        const int off = nd.name == "schedule" || nd.name == "schedulek" ? 0
                                      : nd.name == "event" || nd.name == "event_i" ? 1
                                      : nd.name == "schedkwhen" ? 3 : -1;
                        if (off < 0) continue;
                        for (size_t i = (size_t) off + 3; i < nd.ins.size(); ++i)
                            fromEvents[(int) i - off + 1].addIfNotAlreadyThere (js (nd.ins[i].text).trim() + " from " + js (orc.blocks[ob].title()));
                    }
                for (int pn : g.pfields)
                {
                    if (pn < 4) continue;
                    juce::StringArray parts;
                    if (auto it = fromScore.find (pn); it != fromScore.end())
                    {
                        auto v = it->second;
                        const bool more = v.size() > 3;
                        if (more) v.removeRange (3, v.size());
                        parts.add (v.joinIntoString (", ") + (more ? ", ..." : "") + " in the score");
                    }
                    if (auto it = fromEvents.find (pn); it != fromEvents.end())
                        parts.add (it->second.joinIntoString ("; "));
                    if (! parts.isEmpty()) ctx.pvalues[pn] = parts.joinIntoString ("; ");
                }
            }

            std::map<char, int> perRate;
            for (const auto& nd : g.nodes)
            {
                const char r = timing::rateOf (nd);
                if (r == 0 || nd.line <= 0) continue;
                auto rank = [] (char c) { return c == 'a' ? 3 : c == 'k' ? 2 : c == 'i' ? 1 : 0; };
                if (! ex.rates.count (nd.line)) ++perRate[r];
                for (int l = nd.line; l <= juce::jmax (nd.line, nd.lineEnd); ++l)
                    if (rank (r) > rank (ex.rates[l])) ex.rates[l] = r;
            }
            for (const auto& nd : g.nodes)
            {
                if (nd.line <= 0 || ex.shapes.count (nd.line)) continue;
                const bool osc = nd.name.rfind ("osc", 0) == 0 || nd.name.rfind ("poscil", 0) == 0 || nd.name.rfind ("table", 0) == 0;
                for (size_t i = 0; i < nd.ins.size(); ++i)
                {
                    const auto t = js (nd.ins[i].text).trim();
                    auto it = defs.find (t);
                    if (it == defs.end() || (tables::numeric (t) && ! (osc && i == 2))) continue;
                    auto sh = tables::shape (it->second.gen, it->second.args);
                    if (! sh.empty()) { ex.shapes[nd.line] = sh; break; }
                }
            }
            for (const auto& nd : g.nodes)
            {
                if (nd.line <= 0 || ex.notes.count (nd.line)) continue;
                auto t = noteFor (g, nd, ctx);
                // un argomento vuoto: quasi sempre una virgola di troppo ("oscili, 0.2, 440")
                if (nd.kind == csoundGraph::NodeKind::Opcode)
                    for (const auto& in : nd.ins)
                        if (js (in.text).trim().isEmpty())
                        {
                            t = "check: an empty argument - a comma right after " + js (nd.name) + "?" + (t.isNotEmpty() ? "  (" + t + ")" : juce::String());
                            break;
                        }
                if (t.isNotEmpty()) ex.notes[nd.line] = t;
            }
            // else / elseif: quando valgono (la riga "else" non ha un nodo suo)
            for (const auto& f : g.frames)
            {
                if (f.kind != "else" && f.kind != "elseif") continue;
                if (f.chain < 0 || f.chain >= (int) g.frames.size()) continue;
                const auto& root = g.frames[(size_t) f.chain];
                int line = f.lineBegin;
                if (f.kind == "else")
                {
                    // la riga dell'else: subito prima della prima riga del ramo
                    if (line <= 0) continue;
                    ex.notes[line] = "otherwise (when " + js (root.cond) + " is false) the lines below run";
                }
                else if (line > 0)
                    ex.notes[line] = "otherwise, the lines below run if " + js (f.cond);
            }
            if (g.kind != csoundGraph::Graph::Header && g.lineBegin > 0)
            {
                Summary s;
                s.line = g.lineBegin;
                s.text = summaryFor (orc, b, ctx);
                // quando gira: quante righe a ogni tempo, con i numeri veri
                juce::StringArray when;
                auto lines = [] (int n) { return juce::String (n) + (n == 1 ? " line" : " lines"); };
                if (perRate['i'] > 0) when.add (lines (perRate['i']) + " once at note start (i)");
                if (perRate['k'] > 0) when.add (lines (perRate['k']) + " " + timing::number (ex.sr / ex.ksmps) + " times a second (k)");
                if (perRate['a'] > 0) when.add (lines (perRate['a']) + " on every sample, " + timing::number (ex.ksmps) + " at a time (a)");
                if (! when.isEmpty() && g.kind == csoundGraph::Graph::Instr)
                    s.text.add ("Time: " + when.joinIntoString (", ") + ".");
                if (! s.text.isEmpty()) ex.summaries.push_back (s);
            }
        }
        return ex;
    }
}

//==============================================================================
// Il codice con le note in parole: accanto a ogni riga (finestra larga) o
// sotto la riga (stretta, iPhone), con il riassunto sopra ogni strumento.
// Usato dalla vista Algorithm (pillola "Explain"); un tocco su una riga
// porta il cursore a quella riga.
class CsoundExplainCanvas final : public juce::Component
{
public:
    CsoundExplainCanvas() = default;

    std::function<void (int line)> onLineTapped;

    /** Senza note: solo il codice (pannello "Code" accanto al grafo). */
    void setNotesVisible (bool shouldShow)         { showNotes = shouldShow; }
    /** Righe raggruppate per tempo (i, k, a) invece che nell'ordine del file. */
    void setGroupedByTime (bool shouldGroup)      { grouped = shouldGroup; }
    /** Riga evidenziata (0 = nessuna), sincronizzata con la scatola scelta nel grafo. */
    void setHighlightedLine (int line)             { if (highlighted != line) { highlighted = line; repaint(); } }
    juce::Rectangle<int> getLineBounds (int line) const
    {
        for (const auto& r : rows) if (! r.summary && r.section == 0 && r.line == line) return r.area.toNearestInt();
        return {};
    }

    /** lines: tutto il file; first..last: righe da mostrare (da 1). */
    void setContent (const juce::StringArray& fileLines, const csoundExplain::Explanation& ex, int first, int last, int availableWidth)
    {
        lines = fileLines;
        notes = ex.notes;
        rates = ex.rates;
        shapes = ex.shapes;
        sr = ex.sr; ksmps = ex.ksmps;
        summaries.clear();
        for (const auto& s : ex.summaries) summaries[s.line] = s.text;
        firstLine = juce::jmax (1, first);
        lastLine = juce::jmin (lines.size(), last);
        width = juce::jmax (320, availableWidth);
        layout();
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { csoundGraphUI::forwardWheel (*this, e, w); }

    void paint (juce::Graphics& g) override
    {
        using namespace csoundGraphUI;
        g.fillAll (kBg);
        const auto mono = monoFont (13.0f);
        const auto clip = g.getClipBounds().toFloat();
        for (const auto& r : rows)
        {
            if (r.area.getBottom() < clip.getY() - 4.0f || r.area.getY() > clip.getBottom() + 4.0f) continue;   // solo le righe visibili
            if (r.section != 0)
            {
                const auto c = rateColour (r.section);
                g.setColour (c.withAlpha (0.10f));
                g.fillRoundedRectangle (r.area.reduced (0.0f, 3.0f), 5.0f);
                drawBadge (g, r.section, r.area.getX() + 8.0f, r.area.getCentreY());
                g.setFont (boldFont (12.5f));
                g.setColour (c);
                g.drawText (csoundExplain::timing::sectionTitle ((char) r.section, sr, ksmps),
                            r.area.withTrimmedLeft (34.0f), juce::Justification::centredLeft, true);
                continue;
            }
            if (r.summary)
            {
                const auto& t = summaries[r.line];
                g.setColour (kAccent.withAlpha (0.12f));
                g.fillRoundedRectangle (r.area, 6.0f);
                g.setColour (kAccent);
                g.fillRoundedRectangle (r.area.withWidth (4.0f), 2.0f);
                for (int i = 0; i < t.size(); ++i)
                {
                    g.setFont (i == 0 ? boldFont (13.0f) : plainFont (12.5f));
                    g.setColour (i == 0 ? kText : kTextMuted);
                    g.drawText (t[i], r.area.reduced (14.0f, 5.0f).withHeight (18.0f).translated (0.0f, 18.0f * (float) i), juce::Justification::centredLeft, true);
                }
                continue;
            }
            const auto code = lines[r.line - 1].trimEnd();
            const bool comment = code.trim().startsWithChar (';');
            if (r.line == highlighted)
            {
                g.setColour (kAccent.withAlpha (0.22f));
                g.fillRect (r.area.withX (gutter - 4.0f).withRight ((float) width));
                g.setColour (kAccent);
                g.fillRect (r.area.withX (gutter - 4.0f).withWidth (3.0f));
            }
            g.setFont (mono);
            g.setColour (kTextMuted.withAlpha (0.7f));
            g.drawText (juce::String (r.line), juce::Rectangle<float> (0.0f, r.area.getY(), gutter - 28.0f, lineH), juce::Justification::centredRight, false);
            if (auto rt = rates.find (r.line); showNotes && rt != rates.end() && rt->second != 0)
                drawBadge (g, rt->second, gutter - 22.0f, r.area.getY() + lineH * 0.5f);
            g.setFont (mono);
            g.setColour (comment ? juce::Colour (0xff6a9955) : juce::Colour (0xffd4d4d4));
            g.drawText (code, juce::Rectangle<float> (gutter, r.area.getY(), codeW + 10.0f, lineH), juce::Justification::centredLeft, false);
            const bool hasShape = showNotes && shapes.count (r.line) > 0;
            if (hasShape)
            {
                // la forma della tabella, in fondo alla riga
                const auto& v = shapes[r.line];
                const juce::Rectangle<float> box ((float) width - shapeW - 10.0f, r.area.getY() + 2.0f, shapeW, lineH - 4.0f);
                g.setColour (kBoxFill);
                g.fillRoundedRectangle (box, 3.0f);
                bool bipolar = false;
                for (auto x : v) bipolar = bipolar || x < -0.01f;
                juce::Path p;
                for (size_t k = 0; k < v.size(); ++k)
                {
                    const float px = box.getX() + 3.0f + (box.getWidth() - 6.0f) * (float) k / (float) juce::jmax ((size_t) 1, v.size() - 1);
                    const float norm = bipolar ? (v[k] + 1.0f) * 0.5f : v[k];
                    const float py = box.getBottom() - 2.0f - (box.getHeight() - 4.0f) * juce::jlimit (0.0f, 1.0f, norm);
                    if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
                }
                g.setColour (kAccent);
                g.strokePath (p, juce::PathStrokeType (1.3f));
                g.setFont (mono);
            }
            if (auto it = notes.find (r.line); showNotes && it != notes.end())
            {
                g.setFont (plainFont (12.5f));
                g.setColour (kAccent.withAlpha (0.9f));
                const auto note = u8 (wide ? "\xe2\x86\x90  " : "\xe2\x86\xb3  ") + it->second;
                if (wide)
                    g.drawText (note, juce::Rectangle<float> (gutter + codeW + 30.0f, r.area.getY(), (float) width - codeW - gutter - 40.0f - (hasShape ? shapeW + 8.0f : 0.0f), lineH), juce::Justification::centredLeft, true);
                else
                    g.drawFittedText (note, juce::Rectangle<int> ((int) gutter + 16, (int) (r.area.getY() + lineH), width - (int) gutter - 24, (int) (r.area.getHeight() - lineH)),
                                      juce::Justification::topLeft, 3, 1.0f);
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown()) return;
        for (const auto& r : rows)
            if (! r.summary && r.section == 0 && r.area.contains (e.position))
            {
                if (onLineTapped) onLineTapped (r.line);
                return;
            }
    }

private:
    struct Row { int line = 0; bool summary = false; juce::Rectangle<float> area; int section = 0; };
    juce::StringArray lines;
    std::map<int, juce::String> notes;
    std::map<int, juce::StringArray> summaries;
    std::vector<Row> rows;
    int firstLine = 1, lastLine = 0, width = 800;
    float codeW = 0.0f;
    bool wide = true, showNotes = true, grouped = false;
    int highlighted = 0;
    std::map<int, char> rates;
    std::map<int, std::vector<float>> shapes;
    double sr = 44100.0, ksmps = 10.0;
    static constexpr float shapeW = 72.0f;
    static constexpr float lineH = 20.0f, gutter = 64.0f;

public:
    // i blu, k ambra, a teal (come i cavi audio)
    static juce::Colour rateColour (int rate)
    {
        return rate == 'i' ? juce::Colour (0xff8fb0d8) : rate == 'k' ? juce::Colour (0xffe0b050) : csoundGraphUI::kAccent;
    }
    static void drawBadge (juce::Graphics& g, int rate, float x, float centreY)
    {
        const juce::Rectangle<float> b (x, centreY - 7.0f, 15.0f, 14.0f);
        g.setColour (rateColour (rate).withAlpha (0.22f));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (rateColour (rate));
        g.setFont (csoundGraphUI::boldFont (10.5f));
        g.drawText (juce::String::charToString ((juce::juce_wchar) rate), b, juce::Justification::centred, false);
    }
private:

    void layout()
    {
        using namespace csoundGraphUI;
        rows.clear();
        const auto mono = monoFont (13.0f);
        codeW = 0.0f;
        // carattere a larghezza fissa: la riga piu' lunga basta (misurare ogni riga costa su file di migliaia di righe)
        int longest = 0;
        for (int l = firstLine; l <= lastLine; ++l) longest = juce::jmax (longest, lines[l - 1].trimEnd().length());
        codeW = textWidth (mono, juce::String::repeatedString ("M", longest));
        // larga: note a destra del codice (codice al massimo 560 px); stretta: note sotto la riga
        wide = width >= 980 || ! showNotes;
        codeW = juce::jmin (codeW, showNotes && wide ? 560.0f : (float) width - gutter - 12.0f);

        // l'ordine delle righe: quello del file, oppure per tempo (la prima
        // riga, poi le righe i, k, a sotto il loro titolo, poi l'ultima)
        std::vector<int> order;     // numero di riga, oppure -'i' / -'k' / -'a' per un titolo
        if (grouped && showNotes && lastLine > firstLine)
        {
            order.push_back (firstLine);
            for (char rate : { 'i', 'k', 'a' })
            {
                bool any = false;
                for (int l = firstLine + 1; l < lastLine; ++l)
                {
                    auto it = rates.find (l);
                    if (it == rates.end() || it->second != rate) continue;
                    if (! any) { order.push_back (-(int) rate); any = true; }
                    order.push_back (l);
                }
            }
            order.push_back (lastLine);
        }
        else
            for (int l = firstLine; l <= lastLine; ++l) order.push_back (l);

        float y = 12.0f;
        for (int l : order)
        {
            if (l < 0)
            {
                y += 6.0f;
                Row sec; sec.section = -l; sec.area = { gutter - 26.0f, y, (float) width - gutter + 14.0f, 26.0f };
                rows.push_back (sec);
                y += 30.0f;
                continue;
            }
            if (auto it = summaries.find (l); showNotes && it != summaries.end())
            {
                const float h = 10.0f + 18.0f * (float) it->second.size();
                rows.push_back ({ l, true, { gutter + 6.0f, y + 2.0f, (float) width - gutter - 22.0f, h } });
                y += h + 10.0f;
            }
            float h = lineH;
            if (showNotes && ! wide && notes.count (l))
            {
                const int textW = width - (int) gutter - 24;
                const float w = textWidth (plainFont (12.5f), notes[l]) + 20.0f;
                h += 17.0f * juce::jlimit (1.0f, 3.0f, std::ceil (w / (float) juce::jmax (100, textW))) + 4.0f;
            }
            rows.push_back ({ l, false, { 0.0f, y, (float) width, h } });
            y += h;
        }
        setSize (width, (int) y + 20);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundExplainCanvas)
};
