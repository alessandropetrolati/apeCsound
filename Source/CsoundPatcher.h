#pragma once
/*
    CsoundPatcher.h - vista "Patch" (Algorithm): il csd come un patch di
    Max / Pure Data.

    Alessandro Petrolati - www.apesoft.it

      - ogni opcode e' un oggetto su una riga ("reverbsc 0.8 6000"), con gli
        ingressi in alto e le uscite in basso; il segnale scende verso l'uscita;
      - cavi: audio spessi e blu, spettrali tratteggiati, controllo sottili;
      - i canali letti sono number box con il valore dal vivo, le variabili
        globali sono [r nome] / [s nome], i commenti del codice sono commenti
        del patch;
      - l'orchestra e' il patch principale: le righe dello score sono message
        box, gli strumenti e gli UDO sono subpatch [p ...] (doppio tocco: si
        apre), i bus globali e gli eventi sono cavi fra subpatch.

    Il contenuto dello strumento e' quello del Flow (csoundFlow::build): solo
    cio' che arriva a un'uscita, piu' i segnali calcolati e mai usati.
*/
#include <JuceHeader.h>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <vector>
#include "CsoundGraphModel.h"
#include "CsoundGraphFlow.h"
#include "CsoundGraphStyle.h"

namespace csoundPatch
{
    enum class Kind { Object, Number, Message, Sub, Dac };

    struct Obj
    {
        Kind kind = Kind::Object;
        juce::String text, comment, label, channel, outVar, name;
        int node = -1;                 // strumento: indice in Graph::nodes
        int block = -1;                // orchestra: indice in Orchestra::blocks
        int line = 0;
        int ins = 0, outs = 0;
        char rate = 'a';
        bool unused = false, tail = false, commentIsNote = false, send = false;
        float order = 0.0f;
        int layer = 0;
        juce::Rectangle<float> r;      // la scatola
        float slotW = 0.0f, commentW = 0.0f;
        // vista Audio: i controlli che entrano, come etichette sopra gli ingressi
        struct Tag { int inlet = 0; juce::String var; char rate = 'k'; };
        std::vector<Tag> tags;
    };

    // wireless: senza filo, come [s] / [r] in Max - una variabile che va a
    // molti oggetti (o molto lontano) ha un'etichetta all'uscita e una a
    // ogni ingresso, invece di un fascio di cavi lunghi
    struct Cord { int from = 0, fromOut = 0, to = 0, toIn = 0; char rate = 'a'; juce::String var; bool back = false; bool wireless = false; };

    struct Patch
    {
        std::vector<Obj> objs;
        std::vector<Cord> cords;
        bool orchestra = false;
        float width = 0.0f, height = 0.0f;
        float unusedY = 0.0f;          // dove comincia la parte "non collegati" (0 = nessuna)
        float unusedHeadY = 0.0f;      // il suo titolo
    };

    constexpr float boxH = 24.0f, portW = 9.0f, pad = 36.0f, gapX = 30.0f;

    inline juce::Font boxFont()  { return csoundGraphUI::monoFont (12.5f); }
    inline juce::Font noteFont() { return csoundGraphUI::plainFont (12.0f); }

    //==========================================================================
    // Commenti del codice: la riga di commento subito sopra (o il commento in
    // fondo alla stessa riga). Ognuno una volta sola.
    inline juce::String trailingComment (const juce::String& line)
    {
        bool str = false;
        for (int i = 0; i < line.length(); ++i)
        {
            const auto c = line[i];
            if (c == '"') str = ! str;
            if (str) continue;
            if (c == ';') return line.substring (i + 1).trim();
            if (c == '/' && i + 1 < line.length() && line[i + 1] == '/') return line.substring (i + 2).trim();
        }
        return {};
    }
    // "; -- live ----" -> "live" (anche le linee disegnate con i caratteri di riquadro)
    inline juce::String cleanComment (const juce::String& line)
    {
        const auto deco = juce::String ("-=*#~_ ") + juce::String::fromUTF8 ("\xe2\x94\x80\xe2\x95\x90\xe2\x80\x94");
        const auto c = line.trim().trimCharactersAtStart (";/").trimCharactersAtStart (deco).trimCharactersAtEnd (deco).trim();
        return c.startsWithChar ('@') ? juce::String() : c;      // "@group ...": attributi dei canali, non commenti
    }
    inline bool isCommentLine (const juce::String& t) { return t.startsWithChar (';') || t.startsWith ("//"); }

    inline juce::String commentFor (const juce::StringArray* src, int line, std::set<int>& used)
    {
        if (src == nullptr || line <= 0 || line > src->size()) return {};
        // sopra: la riga di commento piu' vicina (si salta una riga vuota)
        for (int l = line - 1, blank = 0; l >= 1 && l >= line - 3; --l)
        {
            const auto t = (*src)[l - 1].trim();
            if (t.isEmpty()) { if (++blank > 1) break; continue; }
            if (! isCommentLine (t)) break;
            if (used.count (l)) break;
            used.insert (l);
            auto c = cleanComment (t);
            if (c.isNotEmpty()) return c;
            break;
        }
        const auto tc = cleanComment (trailingComment ((*src)[line - 1]));
        if (tc.isNotEmpty() && used.insert (line).second) return tc;
        return {};
    }

    //==========================================================================
    // Disposizione di un gruppo di oggetti collegati: strati dall'alto in basso
    // (sorgenti sopra, uscite in fondo), ogni oggetto il piu' vicino possibile
    // a chi lo usa; nello strato, ordine che riduce gli incroci (baricentro di
    // chi lo alimenta e di chi lo usa). Una fila piu' larga di maxW va a capo.
    // Scrive le posizioni relative al gruppo; restituisce la sua misura.
    inline juce::Point<float> layoutGroup (std::vector<Obj>& all, const std::vector<int>& ids, const std::vector<Cord>& cords,
                                           float rowH, float maxW)
    {
        const int n = (int) ids.size();
        std::map<int, int> local;
        for (int i = 0; i < n; ++i) local[ids[(size_t) i]] = i;
        auto O = [&all, &ids] (int i) -> Obj& { return all[(size_t) ids[(size_t) i]]; };

        // strati: ordine topologico (Kahn); cio' che resta e' in un ciclo
        std::vector<std::vector<int>> par ((size_t) n), chi ((size_t) n);
        std::vector<int> indeg ((size_t) n, 0);
        for (const auto& c : cords)
        {
            if (c.back || c.wireless || c.from == c.to) continue;
            const auto f = local.find (c.from), t = local.find (c.to);
            if (f == local.end() || t == local.end()) continue;
            chi[(size_t) f->second].push_back (t->second); par[(size_t) t->second].push_back (f->second); ++indeg[(size_t) t->second];
        }
        std::vector<int> topo, layer ((size_t) n, 0), queue;
        for (int i = 0; i < n; ++i) if (indeg[(size_t) i] == 0) queue.push_back (i);
        std::vector<bool> done ((size_t) n, false);
        for (size_t q = 0; q < queue.size(); ++q)
        {
            const int i = queue[q];
            done[(size_t) i] = true;
            topo.push_back (i);
            for (int c : chi[(size_t) i])
            {
                layer[(size_t) c] = juce::jmax (layer[(size_t) c], layer[(size_t) i] + 1);
                if (--indeg[(size_t) c] == 0) queue.push_back (c);
            }
        }
        for (int i = 0; i < n; ++i)
            if (! done[(size_t) i])
            {
                for (int pr : par[(size_t) i]) if (done[(size_t) pr]) layer[(size_t) i] = juce::jmax (layer[(size_t) i], layer[(size_t) pr] + 1);
                done[(size_t) i] = true;
                topo.push_back (i);
            }
        // il piu' in basso possibile: subito sopra il primo che lo usa
        for (int k = (int) topo.size(); --k >= 0;)
        {
            const int i = topo[(size_t) k];
            if (chi[(size_t) i].empty()) continue;
            int m = INT_MAX;
            for (int c : chi[(size_t) i]) m = juce::jmin (m, layer[(size_t) c] - 1);
            if (m != INT_MAX && m > layer[(size_t) i]) layer[(size_t) i] = m;
        }
        int maxLayer = 0;
        for (int i = 0; i < n; ++i) if (! O (i).tail) maxLayer = juce::jmax (maxLayer, layer[(size_t) i]);
        // le uscite (nessuno le usa) tutte in fondo, insieme; poi gli eventi
        for (int i = 0; i < n; ++i)
        {
            if (O (i).tail) layer[(size_t) i] = maxLayer + 1;
            else if (chi[(size_t) i].empty() && ! par[(size_t) i].empty()) layer[(size_t) i] = maxLayer;
        }
        // strati vuoti: via
        std::map<int, int> remap;
        for (int i = 0; i < n; ++i) remap[layer[(size_t) i]] = 0;
        int li = 0;
        for (auto& kv : remap) kv.second = li++;
        const int numLayers = li;
        std::vector<std::vector<int>> rows ((size_t) numLayers);
        for (int i = 0; i < n; ++i)
        {
            layer[(size_t) i] = remap[layer[(size_t) i]];
            rows[(size_t) layer[(size_t) i]].push_back (i);
        }
        for (auto& row : rows)
            std::stable_sort (row.begin(), row.end(), [&O] (int a, int b) { return O (a).order < O (b).order; });

        // posizioni orizzontali (centro della scatola)
        std::vector<float> cx ((size_t) n, 0.0f);
        auto place = [&] (std::vector<int>& row, const std::vector<float>& want)
        {
            std::stable_sort (row.begin(), row.end(), [&want] (int a, int b) { return want[(size_t) a] < want[(size_t) b]; });
            float cursor = -1.0e9f;
            bool first = true;
            for (int i : row)
            {
                auto& o = O (i);
                // vicino a chi lo alimenta, ma senza grandi vuoti nella fila
                float x = want[(size_t) i] - o.r.getWidth() * 0.5f;
                if (! first) x = juce::jlimit (cursor, cursor + 140.0f, x);
                first = false;
                cx[(size_t) i] = x + o.r.getWidth() * 0.5f;
                cursor = x + o.slotW + gapX;
            }
        };
        {
            std::vector<float> want ((size_t) n, 0.0f);
            for (auto& row : rows)
            {
                float x = 0.0f;
                for (int i : row) { want[(size_t) i] = x + O (i).r.getWidth() * 0.5f; x += O (i).slotW + gapX; }
                place (row, want);
            }
        }
        auto sweep = [&] (bool down)
        {
            std::vector<float> want (cx);
            for (int L = down ? 1 : numLayers - 2; down ? L < numLayers : L >= 0; L += down ? 1 : -1)
            {
                for (int i : rows[(size_t) L])
                {
                    const auto& nb = down ? par[(size_t) i] : chi[(size_t) i];
                    if (nb.empty()) { want[(size_t) i] = cx[(size_t) i]; continue; }
                    float sum = 0.0f;
                    for (int j : nb) sum += cx[(size_t) j];
                    want[(size_t) i] = sum / (float) nb.size();
                }
                place (rows[(size_t) L], want);
            }
        };
        if (n > 1 && n < 3000)
        {
            for (int it = 0; it < 3; ++it) { sweep (true); sweep (false); }
            sweep (true);
        }

        // colonne vuote: strette a una distanza normale
        float minX = 1.0e9f;
        {
            std::vector<std::pair<float, float>> spans;
            for (int i = 0; i < n; ++i)
            {
                const float x0 = cx[(size_t) i] - O (i).r.getWidth() * 0.5f;
                spans.push_back ({ x0, x0 + O (i).slotW });
            }
            std::sort (spans.begin(), spans.end());
            std::vector<std::pair<float, float>> cuts;      // (da x, quanto togliere)
            float end = spans.front().second;
            for (size_t k = 1; k < spans.size(); ++k)
            {
                if (spans[k].first > end + gapX * 2.0f) cuts.push_back ({ spans[k].first, spans[k].first - end - gapX * 2.0f });
                end = juce::jmax (end, spans[k].second);
            }
            for (int i = 0; i < n; ++i)
            {
                const float x0 = cx[(size_t) i] - O (i).r.getWidth() * 0.5f;
                float shift = 0.0f;
                for (const auto& c : cuts) if (x0 >= c.first - 0.01f) shift += c.second;
                cx[(size_t) i] -= shift;
                minX = juce::jmin (minX, cx[(size_t) i] - O (i).r.getWidth() * 0.5f);
            }
        }

        // a capo: una fila troppo larga continua sotto, nello stesso strato
        float y = 0.0f, width = 0.0f;
        for (auto& row : rows)
        {
            std::stable_sort (row.begin(), row.end(), [&cx] (int a, int b) { return cx[(size_t) a] < cx[(size_t) b]; });
            int line = 0;
            float lineStart = 0.0f, cursor = 0.0f;
            for (int i : row)
            {
                auto& o = O (i);
                float x = cx[(size_t) i] - o.r.getWidth() * 0.5f - minX - lineStart;
                if (x + o.slotW > maxW && cursor > 0.0f)
                {
                    ++line;
                    lineStart += x;
                    x = 0.0f;
                    cursor = 0.0f;
                }
                x = juce::jmax (x, cursor);
                o.r.setPosition (x, y + (float) line * rowH);
                cursor = x + o.slotW + gapX;
                width = juce::jmax (width, x + o.slotW);
            }
            y += (float) (line + 1) * rowH;
        }
        return { width, y - rowH + boxH };
    }

    inline void arrange (Patch& p, float rowH)
    {
        auto& objs = p.objs;
        const int n = (int) objs.size();
        p.unusedY = 0.0f;
        if (n == 0) { p.width = p.height = 0.0f; return; }

        // misure
        const auto bf = boxFont(), nf = noteFont();
        for (auto& o : objs)
        {
            float w = csoundGraphUI::textWidth (bf, o.text) + 18.0f;
            if (o.kind == Kind::Number) w = juce::jmax (64.0f, w);
            w = juce::jmax (w, 40.0f, (float) juce::jmax (o.ins, o.outs) * (portW + 6.0f) + 4.0f);
            if (! o.tags.empty()) w = juce::jmax (w, (float) o.ins * 46.0f);     // etichette sopra ingressi vicini
            o.r.setSize (w, boxH);
            o.commentW = o.comment.isNotEmpty() ? juce::jmin (220.0f, csoundGraphUI::textWidth (nf, o.comment) + 4.0f) : 0.0f;
            float slot = w + (o.commentW > 0.0f ? o.commentW + 10.0f : 0.0f);
            if (o.label.isNotEmpty()) slot = juce::jmax (slot, csoundGraphUI::textWidth (csoundGraphUI::plainFont (11.0f), o.label) + 4.0f);
            o.slotW = slot;
        }

        // un cavo fra cio' che suona e cio' che non arriva all'uscita: senza filo
        for (auto& c : p.cords)
            if (! p.orchestra && c.var.isNotEmpty() && objs[(size_t) c.from].unused != objs[(size_t) c.to].unused) c.wireless = true;

        // gruppi collegati (union-find sui cavi con il filo)
        std::vector<int> root ((size_t) n);
        for (int i = 0; i < n; ++i) root[(size_t) i] = i;
        std::function<int (int)> find = [&] (int i) { while (root[(size_t) i] != i) { root[(size_t) i] = root[(size_t) root[(size_t) i]]; i = root[(size_t) i]; } return i; };
        for (const auto& c : p.cords)
            if (! c.wireless && c.from >= 0 && c.to >= 0 && c.from < n && c.to < n) root[(size_t) find (c.from)] = find (c.to);
        std::map<int, std::vector<int>> groups;
        for (int i = 0; i < n; ++i) groups[find (i)].push_back (i);
        std::vector<std::vector<int>> list;
        for (auto& kv : groups) list.push_back (std::move (kv.second));
        auto firstOrder = [&objs] (const std::vector<int>& g) { float m = 1.0e30f; for (int i : g) m = juce::jmin (m, objs[(size_t) i].order); return m; };
        std::stable_sort (list.begin(), list.end(), [&] (const std::vector<int>& a, const std::vector<int>& b)
        {
            const bool ta = objs[(size_t) a.front()].tail, tb = objs[(size_t) b.front()].tail;
            if (ta != tb) return tb;                        // eventi e UDO dopo
            return firstOrder (a) < firstOrder (b);
        });

        // i gruppi su scaffali: da sinistra a destra, a capo oltre maxW;
        // prima cio' che suona, sotto (con un titolo) cio' che non arriva all'uscita
        const float maxW = 1500.0f;
        float x = 0.0f, y = 0.0f, shelfH = 0.0f, width = 0.0f;
        for (int pass = 0; pass < 2; ++pass)
        {
            bool any = false;
            for (const auto& g : list)
            {
                if (objs[(size_t) g.front()].unused != (pass == 1)) continue;
                if (pass == 1 && ! any)
                {
                    bool tagged = false;
                    for (const auto& o : objs) tagged = tagged || (o.unused && ! o.tags.empty());
                    if (x > 0.0f || y > 0.0f) { y += shelfH + 74.0f + (tagged ? 50.0f : 0.0f); x = 0.0f; shelfH = 0.0f; }
                    p.unusedHeadY = pad + 14.0f + y - 34.0f - (tagged ? 50.0f : 0.0f);
                    p.unusedY = pad + 14.0f + y;
                }
                any = true;
                const auto size = layoutGroup (objs, g, p.cords, rowH, maxW);
                if (x > 0.0f && x + size.x > maxW) { x = 0.0f; y += shelfH + 46.0f; shelfH = 0.0f; }
                for (int i : g) objs[(size_t) i].r.translate (pad + x, pad + 14.0f + y);
                x += size.x + 56.0f;
                shelfH = juce::jmax (shelfH, size.y);
                width = juce::jmax (width, x - 56.0f);
            }
        }
        for (auto& c : p.cords)
        {
            const auto& a = objs[(size_t) c.from];
            const auto& b = objs[(size_t) c.to];
            c.back = c.back || b.r.getY() <= a.r.getY() + 1.0f;
            if (! p.orchestra && c.var.isNotEmpty() && b.r.getY() - a.r.getY() > rowH * 5.5f) c.wireless = true;
        }
        for (int i = 0; i < n; ++i) objs[(size_t) i].layer = (int) (objs[(size_t) i].r.getY() / rowH);
        p.width = width + pad * 2.0f;
        p.height = pad * 2.0f + 14.0f + y + shelfH + 20.0f;
    }

    //==========================================================================
    // Vista Audio: solo il percorso del suono (segnali a e f, le uscite, i bus
    // audio). Il calcolo di controllo non si disegna: cio' che entra da li' e'
    // un'etichetta col nome sopra l'ingresso (toccala: il suo valore, e con il
    // doppio tocco sull'oggetto da dove viene). false se lo strumento non ha
    // audio (allora si disegna tutto).
    inline bool keepAudioOnly (Patch& p)
    {
        const int n = (int) p.objs.size();
        std::vector<bool> keep ((size_t) n, false);
        int kept = 0;
        std::vector<bool> signalOut ((size_t) n, false);     // manda un segnale a o f (tab2pvs: k dentro, f fuori)
        for (const auto& c : p.cords) if (c.rate == 'a' || c.rate == 'f') signalOut[(size_t) c.from] = true;
        for (int i = 0; i < n; ++i)
        {
            const auto& o = p.objs[(size_t) i];
            keep[(size_t) i] = (o.rate == 'a' || o.rate == 'f' || signalOut[(size_t) i]) && o.kind != Kind::Number && o.kind != Kind::Message;
            kept += keep[(size_t) i] ? 1 : 0;
        }
        if (kept == 0) return false;
        std::vector<int> remap ((size_t) n, -1);
        std::vector<Obj> objs;
        for (int i = 0; i < n; ++i) if (keep[(size_t) i]) { remap[(size_t) i] = (int) objs.size(); objs.push_back (p.objs[(size_t) i]); }
        std::vector<Cord> cords;
        for (auto c : p.cords)
        {
            const int a = remap[(size_t) c.from], b = remap[(size_t) c.to];
            if (b < 0) continue;
            if (a >= 0) { c.from = a; c.to = b; cords.push_back (c); continue; }
            if (c.var.isEmpty()) continue;
            auto& tags = objs[(size_t) b].tags;
            bool dup = false;
            for (const auto& t : tags) dup = dup || (t.inlet == c.toIn && t.var == c.var);
            if (! dup) tags.push_back ({ c.toIn, c.var, c.rate });
        }
        p.objs = std::move (objs);
        p.cords = std::move (cords);
        return true;
    }

    //==========================================================================
    // Meno oggetti, stesso algoritmo:
    //  - le variabili usate come indice di un array (kIndex in kArr[kIndex])
    //    sono contatori: niente cavi, niente etichette, e gli oggetti che le
    //    calcolano soltanto (kIndex = 0, kIndex + 1) non si disegnano;
    //  - un array e' UN oggetto [kGMag[]] (come [table] in Max): chi scrive un
    //    suo elemento ci entra, chi lo legge ne esce; la sua creazione
    //    (kGMag[] init iBIN) e le scritture di una costante (kM1[kIndex] = 0)
    //    non sono oggetti a parte.
    inline void simplifyArraysAndIndexes (Patch& p, const csoundGraph::Graph& g)
    {
        using namespace csoundGraphUI;
        using K = csoundGraph::NodeKind;
        const int nn = (int) g.nodes.size();

        // indici: identificatori dentro [ ... ]
        std::set<juce::String> indexVars;
        auto scan = [&indexVars] (const std::string& t)
        {
            int depth = 0; size_t start = 0;
            for (size_t i = 0; i < t.size(); ++i)
            {
                if (t[i] == '[') { if (depth++ == 0) start = i + 1; }
                else if (t[i] == ']' && depth > 0 && --depth == 0)
                {
                    std::vector<std::string> v;
                    csoundGraph::detail::exprVars (t.substr (start, i - start), v);
                    for (const auto& x : v) indexVars.insert (js (x));
                }
            }
        };
        for (const auto& nd : g.nodes)
        {
            scan (nd.text);
            for (const auto& in : nd.ins) scan (in.text);
        }
        std::set<juce::String> arrays;
        for (const auto& nd : g.nodes)
            for (const auto& o : nd.outs) if (o.isArray) arrays.insert (js (o.var));
        for (const auto& a : arrays) indexVars.erase (a);
        if (indexVars.empty() && arrays.empty()) return;

        const int n = (int) p.objs.size();
        std::vector<bool> drop ((size_t) n, false);
        auto nodeOf = [&] (int i) -> const csoundGraph::Node* { const int k = p.objs[(size_t) i].node; return k >= 0 && k < nn && ! p.objs[(size_t) i].send ? &g.nodes[(size_t) k] : nullptr; };
        std::map<juce::String, int> arrObj;          // nome -> indice del nuovo oggetto
        std::vector<Obj> extra;
        auto arrayObject = [&] (const juce::String& name, const csoundGraph::Node& nd) -> int
        {
            if (auto it = arrObj.find (name); it != arrObj.end()) return it->second;
            Obj a;
            a.text = name + "[]";
            a.ins = 1; a.outs = 1; a.rate = 'k';
            a.line = nd.line; a.node = -1;
            a.order = (float) nd.line;
            arrObj[name] = n + (int) extra.size();
            extra.push_back (a);
            return arrObj[name];
        };
        for (int i = 0; i < n; ++i)
        {
            const auto* nd = nodeOf (i);
            if (nd == nullptr || nd->outs.empty()) continue;
            bool allIndex = true, allArray = true, create = true;
            for (const auto& o : nd->outs)
            {
                allIndex = allIndex && indexVars.count (js (o.var)) > 0;
                allArray = allArray && o.isArray;
                create = create && o.isArray && ! o.partial;
                if (o.isArray) arrayObject (js (o.var), *nd);
            }
            if (allIndex) { drop[(size_t) i] = true; continue; }
            if (allArray && create && (nd->name == "init" || nd->kind == K::Expr))
            {
                auto& a = extra[(size_t) (arrObj[js (nd->outs[0].var)] - n)];
                if (a.comment.isEmpty()) { a.comment = p.objs[(size_t) i].comment; a.commentIsNote = p.objs[(size_t) i].commentIsNote; }
                a.line = nd->line; a.node = (int) (nd - g.nodes.data());
                drop[(size_t) i] = true;
                continue;
            }
            if (allArray && nd->kind == K::Expr)       // kM1[kIndex] = 0: una costante scritta nell'array
            {
                bool constant = true;
                for (const auto& in : nd->ins)
                    for (const auto& v : in.vars) constant = constant && indexVars.count (js (v)) > 0;
                if (constant) drop[(size_t) i] = true;
            }
        }

        // i cavi di nuovo: indici via, array attraverso il loro oggetto
        std::vector<Cord> cords;
        std::set<std::tuple<int, int, int, int>> seen;
        auto add = [&] (int from, int fromOut, int to, int toIn, char rate, const juce::String& var)
        {
            if (from == to || ! seen.insert ({ from, fromOut, to, toIn }).second) return;
            cords.push_back ({ from, fromOut, to, toIn, rate, var, false, false });
        };
        for (const auto& c : p.cords)
        {
            if (indexVars.count (c.var)) continue;
            if (auto it = arrObj.find (c.var); it != arrObj.end())
            {
                if (! drop[(size_t) c.from]) add (c.from, c.fromOut, it->second, 0, 'k', c.var);
                if (! drop[(size_t) c.to])   add (it->second, 0, c.to, c.toIn, 'k', c.var);
                continue;
            }
            if (! drop[(size_t) c.from] && ! drop[(size_t) c.to]) cords.push_back (c);
        }
        // chi scrive un array ci entra sempre (anche se nessuno lo legge qui)
        for (int i = 0; i < n; ++i)
        {
            const auto* nd = nodeOf (i);
            if (nd == nullptr || drop[(size_t) i]) continue;
            for (int k = 0; k < (int) nd->outs.size(); ++k)
                if (nd->outs[(size_t) k].isArray)
                    if (auto it = arrObj.find (js (nd->outs[(size_t) k].var)); it != arrObj.end())
                    {
                        p.objs[(size_t) i].outs = juce::jmax (p.objs[(size_t) i].outs, k + 1);
                        add (i, k, it->second, 0, 'k', it->first);
                    }
        }

        // nuovi indici
        std::vector<int> remap ((size_t) (n + (int) extra.size()), -1);
        std::vector<Obj> objs;
        for (int i = 0; i < n; ++i) if (! drop[(size_t) i]) { remap[(size_t) i] = (int) objs.size(); objs.push_back (p.objs[(size_t) i]); }
        for (size_t e = 0; e < extra.size(); ++e) { remap[(size_t) n + e] = (int) objs.size(); objs.push_back (extra[e]); }
        for (auto& c : cords) { c.from = remap[(size_t) c.from]; c.to = remap[(size_t) c.to]; }
        cords.erase (std::remove_if (cords.begin(), cords.end(), [] (const Cord& c) { return c.from < 0 || c.to < 0; }), cords.end());
        // un array e' "non collegato" solo se tutti quelli che lo toccano lo sono
        for (const auto& [name, idx] : arrObj)
        {
            const int a = remap[(size_t) idx];
            bool anyUsed = false, any = false;
            for (const auto& c : cords)
                if (c.from == a || c.to == a) { any = true; anyUsed = anyUsed || ! objs[(size_t) (c.from == a ? c.to : c.from)].unused; }
            objs[(size_t) a].unused = any && ! anyUsed;
        }
        p.objs = std::move (objs);
        p.cords = std::move (cords);
    }

    //==========================================================================
    /** Il patch di uno strumento (o di un UDO), dal suo Flow. */
    inline Patch fromFlow (const csoundFlow::Flow& f, const csoundGraph::Graph& g, const juce::StringArray* src, bool audioOnly = true)
    {
        using namespace csoundGraphUI;
        using K = csoundGraph::NodeKind;
        Patch p;
        std::set<int> usedComments;
        std::vector<int> objOf (f.boxes.size(), -1);
        const int nn = (int) g.nodes.size();

        for (size_t bi = 0; bi < f.boxes.size(); ++bi)
        {
            const auto& b = f.boxes[bi];
            if (b.node < 0 || b.node >= nn) continue;
            const auto& nd = g.nodes[(size_t) b.node];
            Obj o;
            o.node = b.node; o.line = b.line; o.rate = b.rate; o.unused = b.unused; o.outVar = b.outVar;
            o.order = (float) b.line + (float) bi * 1.0e-4f;
            if (b.select)                 // if / else che sceglie: come [gate] / [selector~]
            {
                o.text = b.title;
                o.ins = (int) b.params.size() + 1;
                o.outs = 1;
            }
            else if (nd.kind == K::GlobalIn)
            {
                o.text = "r " + js (nd.name);
                o.outs = 1;
            }
            else if (b.source && b.channel.isNotEmpty())
            {
                o.kind = Kind::Number;
                o.label = b.channel;
                o.channel = b.channel;
                o.outs = juce::jmax (1, (int) nd.outs.size());
            }
            else if (nd.kind == K::Expr)
            {
                const auto op = js (nd.name);
                const auto rhs = js (nd.text).trim();
                o.text = shorten (op == "=" || op.isEmpty() ? rhs : op + " " + rhs, 46);
                o.ins = (int) nd.ins.size();
                o.outs = juce::jmax (1, (int) nd.outs.size());
            }
            else
            {
                auto t = js (nd.name);
                if (b.channel.isNotEmpty()) t << " \"" << b.channel << "\"";
                for (const auto& prm : b.params)
                    if (! (b.channel.isNotEmpty() && prm.text.unquoted() == b.channel)) t << " " << prm.text;
                o.text = shorten (t, 50);
                o.ins = (int) nd.ins.size();
                o.outs = (int) nd.outs.size();
            }
            o.comment = b.select ? b.role : commentFor (src, nd.line, usedComments);
            bool writesGlobal = false;          // gia' detto dal suo [s nome]
            for (const auto& out : nd.outs) writesGlobal = writesGlobal || out.global;
            if (o.comment.isEmpty() && b.sink && b.outNote.isNotEmpty() && ! nd.audioOut && ! writesGlobal)
            {
                o.comment = u8 ("\xe2\x86\x92 ") + b.outNote;
                o.commentIsNote = true;
            }
            objOf[bi] = (int) p.objs.size();
            p.objs.push_back (o);
        }

        for (const auto& e : f.edges)
        {
            if (e.from < 0 || e.to < 0 || e.from >= (int) objOf.size() || e.to >= (int) objOf.size()) continue;
            const int a = objOf[(size_t) e.from], b = objOf[(size_t) e.to];
            if (a < 0 || b < 0) continue;
            Cord c;
            c.from = a; c.to = b; c.toIn = e.toIn; c.rate = e.rate; c.var = e.var; c.back = e.feedback;
            const auto& src2 = g.nodes[(size_t) p.objs[(size_t) a].node];
            for (int k = 0; k < (int) src2.outs.size(); ++k)
                if (js (src2.outs[(size_t) k].var) == e.var) { c.fromOut = k; break; }
            auto& to = p.objs[(size_t) b];
            to.ins = juce::jmax (to.ins, c.toIn + 1);
            auto& fr = p.objs[(size_t) a];
            fr.outs = juce::jmax (fr.outs, c.fromOut + 1);
            p.cords.push_back (c);
        }

        // variabili globali scritte: [s nome] sotto chi le scrive
        const int before = (int) p.objs.size();
        for (int i = 0; i < before; ++i)
        {
            const auto& nd = g.nodes[(size_t) p.objs[(size_t) i].node];
            if (p.objs[(size_t) i].kind == Kind::Number || nd.kind == K::GlobalIn) continue;
            for (int k = 0; k < (int) nd.outs.size(); ++k)
            {
                const auto& out = nd.outs[(size_t) k];
                if (! out.global) continue;
                Obj s;
                s.text = "s " + js (out.var);
                s.ins = 1; s.rate = out.rate; s.line = nd.line; s.node = p.objs[(size_t) i].node; s.send = true;
                s.order = p.objs[(size_t) i].order + 0.01f;
                p.objs[(size_t) i].outs = juce::jmax (p.objs[(size_t) i].outs, k + 1);
                p.cords.push_back ({ i, k, (int) p.objs.size(), 0, out.rate == '?' ? 'k' : out.rate, js (out.var), false });
                p.objs.push_back (s);
            }
        }

        // eventi: una message box per ogni nota che lo strumento fa partire
        for (const auto& ev : f.events)
        {
            Obj o;
            o.kind = Kind::Message;
            juce::String t = ev.opcode;
            for (const auto& r : ev.rows) t << " " << r.text;
            o.text = shorten (t, 50);
            o.node = ev.node;
            o.line = ev.node >= 0 && ev.node < nn ? g.nodes[(size_t) ev.node].line : 0;
            o.order = (float) o.line;
            o.rate = 'i';
            o.tail = true;
            o.comment = u8 ("\xe2\x86\x92 starts instr ") + ev.target;
            o.commentIsNote = true;
            p.objs.push_back (o);
        }
        // solo il percorso del suono quando lo strumento e' grande: uno
        // piccolo si legge meglio intero (il phasor che muove la frequenza)
        if (! (audioOnly && p.objs.size() > 40 && keepAudioOnly (p)))
            simplifyArraysAndIndexes (p, g);

        // una variabile che va a molti oggetti: senza filo
        std::map<std::pair<int, int>, int> fan;
        for (const auto& c : p.cords) ++fan[{ c.from, c.fromOut }];
        for (auto& c : p.cords) if (fan[{ c.from, c.fromOut }] >= 4 && c.var.isNotEmpty()) c.wireless = true;
        bool tagged = false;
        for (const auto& o : p.objs) tagged = tagged || ! o.tags.empty();
        arrange (p, tagged ? 96.0f : 74.0f);
        return p;
    }

    //==========================================================================
    /** L'orchestra come patch principale. */
    inline Patch fromOrchestra (const csoundGraph::Orchestra& orc, const juce::StringArray* src)
    {
        using namespace csoundGraphUI;
        using G = csoundGraph::Graph;
        Patch p;
        p.orchestra = true;
        std::set<int> usedComments;
        std::map<int, int> objOfBlock;

        auto blockComment = [&] (const G& g)
        {
            // il commento sopra "instr", altrimenti il primo dentro lo strumento
            auto c = commentFor (src, g.lineBegin, usedComments);
            if (c.isEmpty() && src != nullptr)
                for (int l = g.lineBegin + 1; l <= juce::jmin (g.lineEnd, g.lineBegin + 4, src->size()); ++l)
                {
                    const auto t = (*src)[l - 1].trim();
                    if (isCommentLine (t)) { c = cleanComment (t); if (c.isNotEmpty()) break; }
                    else if (t.isNotEmpty()) break;
                }
            return shorten (c, 48);
        };

        bool anyAudio = false;
        for (size_t b = 0; b < orc.blocks.size(); ++b)
        {
            const auto& g = orc.blocks[b];
            if (g.kind == G::Header)
            {
                bool linked = false;
                for (const auto& l : orc.links) linked = linked || l.from == (int) b || l.to == (int) b;
                if (! linked) continue;
            }
            Obj o;
            o.kind = Kind::Sub;
            o.block = (int) b;
            o.line = g.lineBegin;
            o.order = (float) b;
            o.rate = 'a';
            if (g.kind == G::Header) o.text = "p global";
            else if (g.kind == G::Udo) { o.text = "p " + js (g.title()); o.tail = true; }
            else o.text = "p " + js (g.title());
            if (g.kind == G::Instr && ! g.names.empty()) o.name = js (g.names[0]);
            o.comment = g.kind == G::Header ? juce::String ("code outside the instruments") : blockComment (g);
            bool audio = false;
            for (const auto& nd : g.nodes) audio = audio || nd.audioOut;
            if (g.kind == G::Instr && audio) { o.outs = 1; anyAudio = true; }
            objOfBlock[(int) b] = (int) p.objs.size();
            p.objs.push_back (o);
        }

        // le righe "i" dello score: una message box per strumento
        std::map<juce::String, std::pair<int, int>> firstLine;      // strumento -> (indice in scoreLines, numero di righe)
        for (int i = 0; i < (int) orc.scoreLines.size(); ++i)
        {
            const auto& sl = orc.scoreLines[(size_t) i];
            if (sl.kind != 'i') continue;
            const auto key = js (sl.instr);
            auto it = firstLine.find (key);
            if (it == firstLine.end()) firstLine[key] = { i, 1 };
            else ++it->second.second;
        }
        auto blockOfInstr = [&orc] (const juce::String& nm)
        {
            for (size_t b = 0; b < orc.blocks.size(); ++b)
                if (orc.blocks[b].kind == G::Instr)
                    for (const auto& x : orc.blocks[b].names) if (js (x) == nm) return (int) b;
            return -1;
        };
        for (const auto& [instr, fl] : firstLine)
        {
            const int b = blockOfInstr (instr);
            if (b < 0 || objOfBlock.count (b) == 0) continue;
            const auto& sl = orc.scoreLines[(size_t) fl.first];
            Obj m;
            m.kind = Kind::Message;
            juce::String t ("i");
            for (const auto& v : sl.p) t << " " << js (v);
            m.text = shorten (t, 40);
            m.line = sl.line;
            m.outs = 1;
            m.rate = 'i';
            m.order = (float) b - 0.5f;
            if (fl.second > 1) { m.comment = juce::String (fl.second) + " notes in the score"; m.commentIsNote = true; }
            const int to = objOfBlock[b];
            p.objs[(size_t) to].ins = juce::jmax (p.objs[(size_t) to].ins, 1);
            p.cords.push_back ({ (int) p.objs.size(), 0, to, 0, 'i', "score", false });
            p.objs.push_back (m);
        }

        // collegamenti fra blocchi: note (evento) all'ingresso 0, segnali all'1
        // un cavo per coppia di blocchi e tipo, con tutti i nomi che porta ("gaInL, gaInR")
        std::map<std::tuple<int, int, char>, size_t> cordOf;
        for (const auto& l : orc.links)
        {
            if (objOfBlock.count (l.from) == 0 || objOfBlock.count (l.to) == 0 || l.from == l.to) continue;
            const bool ev = l.kind == "evento";
            const auto name = js (l.name) + (ev ? juce::String (" (note)") : juce::String());
            const int a = objOfBlock[l.from], b = objOfBlock[l.to];
            const char r = ev ? 'i' : l.kind == "canale" ? 'k' : (l.rate == 'a' || l.rate == 'k' || l.rate == 'f' ? l.rate : 'k');
            const auto key = std::make_tuple (a, b, r);
            if (auto it = cordOf.find (key); it != cordOf.end())
            {
                auto& v = p.cords[it->second].var;
                if (! v.contains (name) && v.length() < 60) v << ", " << name;
                continue;
            }
            const int in = ev ? 0 : 1;
            p.objs[(size_t) b].ins = juce::jmax (p.objs[(size_t) b].ins, in + 1);
            p.objs[(size_t) a].outs = juce::jmax (p.objs[(size_t) a].outs, 1);
            cordOf[key] = p.cords.size();
            p.cords.push_back ({ a, 0, b, in, r, name, false });
        }

        if (anyAudio)
        {
            Obj d;
            d.kind = Kind::Dac;
            const auto it = orc.settings.find ("nchnls");
            d.text = "dac~" + (it != orc.settings.end() ? "  " + js (it->second) + " ch" : juce::String());
            d.ins = 1;
            d.order = 1.0e6f;
            const int di = (int) p.objs.size();
            p.objs.push_back (d);
            for (const auto& [b, oi] : objOfBlock)
            {
                const auto& g = orc.blocks[(size_t) b];
                bool audio = false;
                for (const auto& nd : g.nodes) audio = audio || nd.audioOut;
                if (g.kind == G::Instr && audio) p.cords.push_back ({ oi, 0, di, 0, 'a', "audio out", false });
            }
        }
        arrange (p, 92.0f);
        return p;
    }
}

//==============================================================================
class CsoundPatcherCanvas final : public juce::Component
{
public:
    CsoundPatcherCanvas() = default;
    std::function<void (int node)> onNodeTapped, onNodeDoubleTapped;     // strumento: indice in Graph::nodes
    std::function<void (int block)> onBlockOpen;                         // orchestra: doppio tocco su [p ...]
    std::function<void()> onEmptyTapped;
    std::function<void (const juce::String& var, juce::Point<float> where)> onCableTapped;
    std::function<void (const juce::String& var)> onCableHovered;

    void setPatch (csoundPatch::Patch newPatch)
    {
        const bool same = patch.orchestra == newPatch.orchestra && patch.objs.size() == newPatch.objs.size();
        patch = std::move (newPatch);
        if (! same || selected >= (int) patch.objs.size()) selected = -1;
        rebuildHits();
        applySize();
        repaint();
    }
    bool isOrchestra() const noexcept { return patch.orchestra; }

    bool selectNode (int node)
    {
        for (int i = 0; i < (int) patch.objs.size(); ++i)
            if (patch.objs[(size_t) i].node == node && node >= 0) { selected = i; repaint(); return true; }
        return false;
    }
    void clearSelection() { selected = -1; repaint(); }
    juce::Rectangle<float> getNodeBounds (int node) const
    {
        for (const auto& o : patch.objs) if (o.node == node && node >= 0) return o.r * zoom;
        return {};
    }

    void setLiveCables (const csoundGraphUI::LiveCables& lc) { const auto h = liveCables.hover; liveCables = lc; liveCables.hover = h; repaint(); }
    void setLiveValues (const std::map<juce::String, juce::String>& v) { if (v != live) { live = v; repaint(); } }
    void setActiveCounts (const std::map<int, int>& counts) { if (counts != active) { active = counts; repaint(); } }
    std::vector<std::pair<int, juce::String>> instrumentNames() const
    {
        std::vector<std::pair<int, juce::String>> out;
        for (const auto& o : patch.objs) if (o.kind == csoundPatch::Kind::Sub && o.name.isNotEmpty()) out.push_back ({ o.block, o.name });
        return out;
    }

    //--------------------------------------------------------------------------
    // Zoom: pinch, Cmd/Ctrl + rotellina, doppio tocco sul vuoto
    void viewportResized() { applySize(); }
    void setZoom (float newZoom, juce::Point<float> anchor)
    {
        newZoom = juce::jlimit (0.2f, 2.5f, newZoom);
        if (std::abs (newZoom - zoom) < 0.001f) return;
        auto* vp = findParentComponentOfClass<juce::Viewport>();
        const auto inContent = anchor / zoom;
        const auto inView = vp != nullptr ? anchor - vp->getViewPosition().toFloat() : anchor;
        zoom = newZoom;
        applySize();
        if (vp != nullptr) vp->setViewPosition ((inContent * zoom - inView).roundToInt());
        repaint();
    }
    void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override { setZoom (zoom * scaleFactor, e.position); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
            setZoom (zoom * std::exp (0.9f * (! juce::approximatelyEqual (w.deltaY, 0.0f) ? w.deltaY : w.deltaX)), e.position);
        else
            csoundGraphUI::forwardWheel (*this, e, w);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.source.isTouch()) return;
        touches[e.source.getIndex()] = e.getScreenPosition().toFloat();
        if (touches.size() == 2) startPinch();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! e.source.isTouch() || touches.count (e.source.getIndex()) == 0) return;
        touches[e.source.getIndex()] = e.getScreenPosition().toFloat();
        if (touches.size() < 2 || pinchStartDistance <= 0.0f) return;
        auto it = touches.begin();
        const auto a = it->second, b = (++it)->second;
        const float d = a.getDistanceFrom (b);
        if (d > 1.0f) setZoom (pinchStartZoom * d / pinchStartDistance, getLocalPoint (nullptr, (a + b) * 0.5f));
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto pos = e.position / zoom;
        const auto v = csoundGraphUI::cableAt (cableHits, pos);
        if (v != liveCables.hover) { liveCables.hover = v; repaint(); if (onCableHovered) onCableHovered (patch.orchestra ? juce::String() : v); }
        setMouseCursor (objectAt (pos) >= 0 || v.isNotEmpty() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        if (liveCables.hover.isNotEmpty()) { liveCables.hover.clear(); repaint(); if (onCableHovered) onCableHovered ({}); }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.source.isTouch())
        {
            touches.erase (e.source.getIndex());
            if (pinching) { if (touches.empty()) endPinch(); return; }
        }
        if (e.mouseWasDraggedSinceMouseDown()) return;
        const auto pos = e.position / zoom;
        const bool twice = e.getNumberOfClicks() >= 2;
        if (const int i = objectAt (pos); i >= 0)
        {
            const auto& o = patch.objs[(size_t) i];
            selected = twice || selected != i ? i : -1;       // il secondo tocco del doppio non deseleziona
            repaint();
            if (patch.orchestra)
            {
                if (twice && o.kind == csoundPatch::Kind::Sub && onBlockOpen) onBlockOpen (o.block);
                return;
            }
            if (o.node < 0) return;
            if (twice) { if (onNodeDoubleTapped) onNodeDoubleTapped (o.node); }
            else if (selected >= 0 && onNodeTapped) onNodeTapped (o.node);
            return;
        }
        if (twice) { toggleFitZoom (e.position); return; }
        if (! patch.orchestra)
            if (const auto v = csoundGraphUI::cableAt (cableHits, pos); v.isNotEmpty())
            {
                if (onCableTapped) onCableTapped (v, e.position);
                return;
            }
        selected = -1;
        repaint();
        if (onEmptyTapped) onEmptyTapped();
    }

    //--------------------------------------------------------------------------
    void paint (juce::Graphics& g) override
    {
        using namespace csoundGraphUI;
        using csoundPatch::Kind;
        g.addTransform (juce::AffineTransform::scale (zoom));
        const auto clip = g.getClipBounds().toFloat().expanded (40.0f);

        if (patch.objs.empty())
        {
            g.setColour (kTextMuted);
            g.setFont (plainFont (14.0f));
            g.drawText ("Nothing to draw here: no signal reaches an output", juce::Rectangle<float> (0, 0, (float) getWidth() / zoom, 80.0f), juce::Justification::centred);
            return;
        }

        // vicini dell'oggetto scelto: il resto in penombra
        std::set<int> near;
        if (selected >= 0)
        {
            near.insert (selected);
            for (const auto& c : patch.cords)
            {
                if (c.from == selected) near.insert (c.to);
                if (c.to == selected) near.insert (c.from);
            }
        }

        if (patch.unusedY > 0.0f)
        {
            const float y = patch.unusedHeadY;
            g.setColour (kBorder);
            g.drawHorizontalLine ((int) y, csoundPatch::pad * 0.5f, patch.width - csoundPatch::pad * 0.5f);
            g.setColour (kTextMuted);
            g.setFont (csoundPatch::noteFont());
            g.drawText ("Computed but never reaching an output", juce::Rectangle<float> (csoundPatch::pad, y + 2.0f, 400.0f, 14.0f), juce::Justification::centredLeft, false);
        }

        // --- cavi -------------------------------------------------------------
        std::vector<LiveLabel> labels;
        for (size_t ci = 0; ci < patch.cords.size(); ++ci)
        {
            const auto& c = patch.cords[ci];
            const auto& path = cordPaths[ci];
            if (! path.getBounds().expanded (4.0f).intersects (clip)) continue;
            const bool lit = selected < 0 || c.from == selected || c.to == selected;
            const bool hot = c.var.isNotEmpty() && (c.var == liveCables.hover || c.var == liveCables.selected);
            const auto col = cordColour (c.rate).withAlpha (lit ? (hot ? 1.0f : 0.9f) : 0.16f);
            const float w = (c.rate == 'a' ? 2.6f : c.rate == 'f' ? 2.0f : 1.2f) + (hot ? 1.2f : 0.0f);
            g.setColour (col);
            if (c.wireless)
            {
                g.strokePath (path, juce::PathStrokeType (w));
                const auto top = path.getBounds();
                drawTag (g, { top.getCentreX(), top.getY() }, true, c.var, col, hot);
            }
            else if (c.rate == 'f' || c.back)
            {
                juce::Path d;
                const float dash[] = { 6.0f, 3.5f };
                juce::PathStrokeType (w).createDashedStroke (d, path, dash, 2);
                g.fillPath (d);
            }
            else
                g.strokePath (path, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            if (c.var.isEmpty()) continue;
            const bool hasValue = liveCables.text.count (c.var) > 0;
            if (hot || (liveCables.all && hasValue && lit))
                labels.push_back ({ path, c.var, hasValue ? liveCables.text.at (c.var) : juce::String(), cordColour (c.rate), hot });
        }
        for (const auto& w : wirelessOut)
        {
            const bool lit = selected < 0 || w.key.first == selected;
            const bool hot = w.var == liveCables.hover || w.var == liveCables.selected;
            const auto col = cordColour (w.rate).withAlpha (lit ? 0.9f : 0.16f);
            g.setColour (col);
            g.strokePath (w.path, juce::PathStrokeType (w.rate == 'a' ? 2.6f : 1.2f));
            const auto b = w.path.getBounds();
            drawTag (g, { b.getCentreX(), b.getBottom() }, false, w.var, col, hot);
        }
        for (const auto& t : inletTags)
        {
            const bool lit = selected < 0 || t.obj == selected;
            const bool hot = t.var == liveCables.hover || t.var == liveCables.selected;
            const auto col = cordColour (t.rate).withAlpha (lit ? 0.9f : 0.16f);
            g.setColour (col);
            g.strokePath (t.path, juce::PathStrokeType (1.2f));
            const auto b = t.path.getBounds();
            drawTag (g, { b.getCentreX(), b.getY() }, true, t.var, col, hot);
            const bool hasValue = liveCables.text.count (t.var) > 0;
            if (hot || (liveCables.all && hasValue && lit))
                labels.push_back ({ t.path, t.var, hasValue ? liveCables.text.at (t.var) : juce::String(), cordColour (t.rate), hot });
        }
        // uscite non collegate: un moncherino toccabile come un cavo
        for (const auto& o : patch.objs)
        {
            if (! o.unused || o.outVar.isEmpty()) continue;
            const auto p0 = outletPos (o, 0);
            juce::Path s;
            s.startNewSubPath (p0);
            s.lineTo (p0.translated (0.0f, 22.0f));
            const bool hot = o.outVar == liveCables.hover || o.outVar == liveCables.selected;
            g.setColour (cordColour (o.rate).withAlpha (hot ? 1.0f : 0.5f));
            g.strokePath (s, juce::PathStrokeType (hot ? 3.0f : 2.0f));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (p0.translated (0.0f, 22.0f)));
            const bool hasValue = liveCables.text.count (o.outVar) > 0;
            if (hot || (liveCables.all && hasValue))
                labels.push_back ({ s, o.outVar, hasValue ? liveCables.text.at (o.outVar) : juce::String(), cordColour (o.rate), hot });
        }

        // --- oggetti ------------------------------------------------------------
        const auto bf = csoundPatch::boxFont();
        std::vector<juce::Rectangle<float>> obstacles;
        for (int i = 0; i < (int) patch.objs.size(); ++i)
        {
            const auto& o = patch.objs[(size_t) i];
            if (! o.r.expanded (o.slotW, 20.0f).intersects (clip)) continue;
            obstacles.push_back (o.r);
            const float alpha = (selected >= 0 && near.count (i) == 0) ? 0.4f : (o.unused ? 0.55f : 1.0f);
            const auto r = o.r;
            juce::Colour fill (0xff16212a), stroke (0xff5d6b76);
            if (o.kind == Kind::Sub)     { fill = juce::Colour (0xff1a2733); stroke = juce::Colour (0xff8696a2); }
            if (o.kind == Kind::Message) { fill = juce::Colour (0xff24323d); stroke = juce::Colour (0xff6b7b86); }
            if (o.kind == Kind::Number)  { fill = juce::Colour (0xff0f171d); stroke = juce::Colour (0xff9aa5b1); }
            if (o.kind == Kind::Dac)     { stroke = rateColour ('a').withAlpha (0.8f); }
            g.setColour (fill.withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (r, o.kind == Kind::Message ? 7.0f : 3.0f);
            g.setColour ((i == selected ? kAccent : stroke).withMultipliedAlpha (alpha));
            g.drawRoundedRectangle (r.reduced (0.5f), o.kind == Kind::Message ? 7.0f : 3.0f, i == selected ? 2.0f : 1.0f);
            if (o.kind == Kind::Sub)            // subpatch: una seconda linea in basso, come una cartella
            {
                g.drawLine (r.getX() + 4.0f, r.getBottom() + 2.5f, r.getRight() - 4.0f, r.getBottom() + 2.5f, 1.0f);
            }

            g.setFont (bf);
            if (o.kind == Kind::Number)
            {
                juce::Path tri;
                tri.addTriangle (r.getX() + 5.0f, r.getY() + 6.0f, r.getX() + 12.0f, r.getCentreY(), r.getX() + 5.0f, r.getBottom() - 6.0f);
                g.setColour (juce::Colour (0xff9aa5b1).withMultipliedAlpha (alpha));
                g.fillPath (tri);
                const auto it = live.find (o.channel);
                g.setColour (kText.withMultipliedAlpha (alpha));
                g.drawText (it != live.end() ? shortValue (it->second) : juce::String ("--"), r.withTrimmedLeft (16.0f).withTrimmedRight (4.0f), juce::Justification::centredLeft, true);
                g.setColour (kTextMuted.withMultipliedAlpha (alpha));
                g.setFont (plainFont (11.0f));
                g.drawText (o.label, juce::Rectangle<float> (r.getX(), r.getY() - 16.0f, o.slotW, 14.0f), juce::Justification::bottomLeft, true);
            }
            else
            {
                g.setColour (kText.withMultipliedAlpha (alpha));
                g.drawText (o.text, r.withTrimmedLeft (8.0f).withTrimmedRight (4.0f), juce::Justification::centredLeft, true);
            }

            // ingressi e uscite
            for (int k = 0; k < o.ins; ++k)
            {
                const auto p = inletPos (o, k);
                g.setColour (portColour (o, k, true).withMultipliedAlpha (alpha));
                g.fillRect (p.x - csoundPatch::portW * 0.5f, r.getY(), csoundPatch::portW, 3.0f);
            }
            for (int k = 0; k < o.outs; ++k)
            {
                const auto p = outletPos (o, k);
                g.setColour (portColour (o, k, false).withMultipliedAlpha (alpha));
                g.fillRect (p.x - csoundPatch::portW * 0.5f, r.getBottom() - 3.0f, csoundPatch::portW, 3.0f);
            }

            // note attive (orchestra)
            if (o.kind == Kind::Sub)
                if (const auto it = active.find (o.block); it != active.end() && it->second > 0)
                {
                    const auto t = juce::String (it->second) + (it->second == 1 ? " note" : " notes");
                    const auto f = boldFont (10.5f);
                    const float w = textWidth (f, t) + 12.0f;
                    const juce::Rectangle<float> pill (r.getRight() - w * 0.5f, r.getY() - 9.0f, w, 16.0f);
                    g.setColour (juce::Colour (0xff52b788));
                    g.fillRoundedRectangle (pill, 8.0f);
                    g.setColour (juce::Colour (0xff0b1116));
                    g.setFont (f);
                    g.drawText (t, pill, juce::Justification::centred, false);
                }

            // commento accanto, come in un patch
            if (o.commentW > 0.0f)
            {
                g.setColour ((o.commentIsNote ? kTextMuted : juce::Colour (0xff7fae6a)).withMultipliedAlpha (alpha));
                g.setFont (csoundPatch::noteFont());
                g.drawText (o.comment, juce::Rectangle<float> (r.getRight() + 10.0f, r.getY(), o.commentW, r.getHeight()), juce::Justification::centredLeft, true);
            }
        }

        drawLiveLabels (g, labels, obstacles, true);
    }

private:
    csoundPatch::Patch patch;
    std::vector<juce::Path> cordPaths;
    struct WirelessOut { std::pair<int, int> key; juce::Path path; juce::String var; char rate; };
    std::vector<WirelessOut> wirelessOut;
    struct InletTag { int obj; juce::Path path; juce::String var; char rate; };
    std::vector<InletTag> inletTags;

    // etichetta di una variabile senza filo: sopra l'ingresso o sotto l'uscita
    static juce::Rectangle<float> drawTag (juce::Graphics& g, juce::Point<float> at, bool above, const juce::String& var, juce::Colour col, bool hot)
    {
        const auto f = csoundGraphUI::boldFont (10.5f);
        const float w = csoundGraphUI::textWidth (f, var) + 12.0f, h = 15.0f;
        const juce::Rectangle<float> r (at.x - w * 0.5f, above ? at.y - h : at.y, w, h);
        g.setColour (csoundGraphUI::kBg);
        g.fillRoundedRectangle (r, 7.5f);
        g.setColour (hot ? csoundGraphUI::kAccent : col);
        g.drawRoundedRectangle (r.reduced (0.5f), 7.5f, hot ? 1.6f : 1.0f);
        g.setColour (csoundGraphUI::kText.withAlpha (col.getFloatAlpha()));
        g.setFont (f);
        g.drawText (var, r, juce::Justification::centred, false);
        return r;
    }
    std::vector<std::pair<juce::Path, juce::String>> cableHits;
    csoundGraphUI::LiveCables liveCables;
    std::map<juce::String, juce::String> live;
    std::map<int, int> active;
    int selected = -1;
    float zoom = 1.0f;
    std::map<int, juce::Point<float>> touches;
    float pinchStartDistance = 0.0f, pinchStartZoom = 1.0f;
    bool pinching = false;
    juce::Viewport::ScrollOnDragMode savedDragMode = juce::Viewport::ScrollOnDragMode::all;

    static juce::Colour cordColour (char r)
    {
        switch (r)
        {
            case 'a': return csoundGraphUI::rateColour ('a');
            case 'f': return csoundGraphUI::rateColour ('f');
            case 'k': return juce::Colour (0xffaab4bd);
            default:  return juce::Colour (0xff6f7c86);
        }
    }
    juce::Colour portColour (const csoundPatch::Obj& o, int k, bool inlet) const
    {
        for (const auto& c : patch.cords)
        {
            if (inlet && &patch.objs[(size_t) c.to] == &o && c.toIn == k) return cordColour (c.rate);
            if (! inlet && &patch.objs[(size_t) c.from] == &o && c.fromOut == k) return cordColour (c.rate);
        }
        return juce::Colour (0xff8a9aa5);
    }
    static juce::String shortValue (const juce::String& v)
    {
        if (! v.containsOnly ("-0123456789.eE")) return v;
        auto t = juce::String (v.getDoubleValue(), 3);
        if (t.containsChar ('.')) t = t.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return t == "-0" ? juce::String ("0") : t;
    }

    static juce::Point<float> portPos (const csoundPatch::Obj& o, int k, int n, float y)
    {
        const float w = o.r.getWidth() - csoundPatch::portW;
        const float x = n <= 1 ? 0.0f : w * (float) juce::jlimit (0, n - 1, k) / (float) (n - 1);
        return { o.r.getX() + x + csoundPatch::portW * 0.5f, y };
    }
    static juce::Point<float> inletPos (const csoundPatch::Obj& o, int k)  { return portPos (o, k, o.ins, o.r.getY()); }
    static juce::Point<float> outletPos (const csoundPatch::Obj& o, int k) { return portPos (o, k, juce::jmax (1, o.outs), o.r.getBottom()); }

    int objectAt (juce::Point<float> pos) const
    {
        for (int i = (int) patch.objs.size(); --i >= 0;)
            if (patch.objs[(size_t) i].r.expanded (2.0f).contains (pos)) return i;
        return -1;
    }

    void rebuildHits()
    {
        cordPaths.clear();
        cableHits.clear();
        wirelessOut.clear();
        inletTags.clear();
        for (int i = 0; i < (int) patch.objs.size(); ++i)
        {
            const auto& o = patch.objs[(size_t) i];
            std::map<int, int> perInlet;
            for (const auto& t : o.tags)
            {
                const int k = perInlet[t.inlet]++;
                const auto p2 = inletPos (o, t.inlet);
                juce::Path s;
                s.startNewSubPath (p2);
                s.lineTo (p2.translated (0.0f, -(12.0f + 15.0f * (float) ((t.inlet % 2) + 2 * k))));
                inletTags.push_back ({ i, s, t.var, t.rate });
                cableHits.push_back ({ s, t.var });
            }
        }
        for (const auto& c : patch.cords)
        {
            const auto& a = patch.objs[(size_t) c.from];
            const auto& b = patch.objs[(size_t) c.to];
            const auto p1 = outletPos (a, c.fromOut), p2 = inletPos (b, c.toIn);
            juce::Path path;
            if (c.wireless)
            {
                // un moncherino sopra l'ingresso (l'etichetta col nome si disegna in cima)
                path.startNewSubPath (p2);
                path.lineTo (p2.translated (0.0f, -(12.0f + 15.0f * (float) (c.toIn % 2))));
                cordPaths.push_back (path);
                cableHits.push_back ({ path, c.var });
                const std::pair<int, int> key { c.from, c.fromOut };
                if (std::find_if (wirelessOut.begin(), wirelessOut.end(), [&key] (const WirelessOut& w) { return w.key == key; }) == wirelessOut.end())
                {
                    juce::Path s;
                    s.startNewSubPath (p1);
                    s.lineTo (p1.translated (0.0f, 10.0f));
                    wirelessOut.push_back ({ key, s, c.var, c.rate });
                    cableHits.push_back ({ s, c.var });
                }
                continue;
            }
            path.startNewSubPath (p1);
            if (p2.y > p1.y + 8.0f)
            {
                const float dy = juce::jmax (18.0f, (p2.y - p1.y) * 0.5f);
                path.cubicTo (p1.translated (0.0f, dy), p2.translated (0.0f, -dy), p2);
            }
            else
            {
                // all'indietro (ciclo precedente, retroazione): giro largo a destra
                const float side = juce::jmax (a.r.getRight(), b.r.getRight()) + 26.0f;
                path.cubicTo (p1.translated (0.0f, 30.0f), { side, p1.y + 30.0f }, { side, (p1.y + p2.y) * 0.5f });
                path.cubicTo ({ side, p2.y - 30.0f }, p2.translated (0.0f, -30.0f), p2);
            }
            cordPaths.push_back (path);
            if (c.var.isNotEmpty()) cableHits.push_back ({ path, c.var });
        }
        for (const auto& o : patch.objs)
            if (o.unused && o.outVar.isNotEmpty())
            {
                const auto p0 = outletPos (o, 0);
                juce::Path s;
                s.startNewSubPath (p0);
                s.lineTo (p0.translated (0.0f, 22.0f));
                cableHits.push_back ({ s, o.outVar });
            }
    }

    void applySize()
    {
        int w = (int) std::ceil (patch.width * zoom), h = (int) std::ceil (patch.height * zoom);
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        {
            w = juce::jmax (w, vp->getMaximumVisibleWidth());
            h = juce::jmax (h, vp->getMaximumVisibleHeight());
        }
        setSize (juce::jmax (1, w), juce::jmax (1, h));
    }

    void toggleFitZoom (juce::Point<float> at)
    {
        auto* vp = findParentComponentOfClass<juce::Viewport>();
        if (vp == nullptr || patch.width <= 0.0f || patch.height <= 0.0f) return;
        const float fit = juce::jmin (1.0f, (float) vp->getMaximumVisibleWidth() / patch.width, (float) vp->getMaximumVisibleHeight() / patch.height);
        const float fitZoom = juce::jmax (0.2f, fit);
        float target;
        if (fit < 0.99f) target = std::abs (zoom - fitZoom) < 0.01f ? 1.0f : fitZoom;
        else             target = std::abs (zoom - 1.0f) < 0.01f ? 2.0f : 1.0f;
        setZoom (target, at);
        if (target <= fitZoom + 0.001f) vp->setViewPosition (0, 0);
    }
    void startPinch()
    {
        auto it = touches.begin();
        const auto a = it->second, b = (++it)->second;
        pinchStartDistance = a.getDistanceFrom (b);
        pinchStartZoom = zoom;
        if (! pinching)
            if (auto* vp = findParentComponentOfClass<juce::Viewport>())
            {
                savedDragMode = vp->getScrollOnDragMode();
                vp->setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
            }
        pinching = true;
    }
    void endPinch()
    {
        pinching = false;
        pinchStartDistance = 0.0f;
        if (auto* vp = findParentComponentOfClass<juce::Viewport>()) vp->setScrollOnDragMode (savedDragMode);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundPatcherCanvas)
};
