#pragma once
/*
    CsoundGraphFlow.h - vista "Flusso del segnale" di uno strumento.

    Alessandro Petrolati - www.apesoft.it

    La vista Dettaglio (CsoundGraphView.h) disegna OGNI riga: fedele al
    testo, ma il suono si perde fra stampe, canali di stato e calcoli i-rate.
    Questa vista disegna l'ALGORITMO, come lo schema a blocchi di un synth:

      - si parte dalle uscite (outs, bus audio globali, chnset audio, xout)
        e si risale solo a cio' che contribuisce al suono (program slicing):
        il resto non si disegna (il conteggio resta visibile);
      - scatole solo per i segnali (k, a, f) e per le sorgenti di controllo
        (variabili globali, canali); costanti ed espressioni i-rate diventano
        righe dentro la scatola che le usa ("amp  iAmp * gkVol");
      - la catena audio principale e' una linea dritta da sinistra a destra
        (la "spina"); i segnali di controllo entrano dall'alto, i rami audio
        secondari stanno sotto; le uscite sono a destra;
      - una striscia colorata dice il ruolo dell'opcode (categoria del
        manuale: generatore, modificatore, spettrale, I/O...);
      - toccando una scatola si illumina da dove viene il suo segnale e dove
        va (il resto si attenua).

    Solo libreria standard + JUCE; il modello e' quello di CsoundGraphModel.h.
*/
#include <JuceHeader.h>
#include <algorithm>
#include <climits>
#include <functional>
#include <map>
#include <set>
#include <tuple>
#include <vector>
#include "CsoundGraphModel.h"
#include "CsoundOpcodeHelp.h"
#include "CsoundGraphStyle.h"

namespace csoundFlow
{
    struct Param { juce::String label, text; };

    struct Box
    {
        int node = -1;                 // indice in Graph::nodes
        int line = 0, lineEnd = 0;     // righe del file (per i colori dei blocchi nell'editor)
        char rate = 'a';
        bool source = false, sink = false, spine = false;
        bool unused = false;           // calcolato ma non arriva a nessuna uscita (es. "aph phasor 50" mai letto)
        bool select = false;           // if / else che sceglie un valore: una scatola sola (vedi mergeBranches)
        juce::String outVar;           // non collegata: la variabile scritta (la sua freccia si tocca come un cavo)
        juce::String title, role, group;
        juce::String outNote;          // dove va l'uscita: "audio out", "gkVol -> instr Nota", "starts instr Nota"
        juce::String channel;          // chnget / chnset: per il valore vivo
        std::vector<Param> params;
        int depth = 0, row = 0;
        juce::Rectangle<float> r;
    };

    struct Edge { int from = -1, to = -1, toIn = 0; char rate = 'a'; juce::String label; bool feedback = false; juce::String var; bool control = false; };   // control: "solo se" (da un if a cio' che racchiude)

    // Un evento (schedule, event, schedkwhen, scoreline): la nota che parte,
    // con i suoi p-field e dove li usa lo strumento che la riceve.
    struct EventRow { juce::String field, text, usedBy; };
    struct EventCard
    {
        int node = -1;
        juce::String opcode, target;
        juce::StringArray frames;      // dalla cornice piu' esterna: "while iC < iN"
        std::vector<EventRow> rows;
        juce::Rectangle<float> r;
    };

    // Un blocco funzionale (vista d'insieme): le scatole con lo stesso ruolo
    // musicale. Un tocco lo apre: le sue scatole si accendono nel Flow.
    struct Stage
    {
        int kind = 0;                  // StageKind
        juce::String title, sentence, comment;
        std::vector<int> boxes;        // indici in Flow::boxes
        int column = 0;
        juce::Rectangle<float> r;      // posizione nella vista d'insieme
    };
    struct StageEdge { int from = 0, to = 0; char rate = 'a'; juce::String label; int count = 0; };

    struct Flow
    {
        std::vector<Stage> stages;
        std::vector<StageEdge> stageEdges;
        std::vector<int> stageOfBox;   // per ogni scatola: indice in stages (-1 = nessuno)
        std::vector<Box> boxes;
        std::vector<Edge> edges;
        std::vector<EventCard> events;
        juce::StringArray summary;     // righe del riassunto in parole
        int hidden = 0;                // scatole della vista Dettaglio non disegnate qui
        bool audio = false;            // c'e' un'uscita audio
    };

    // Ruolo dalla categoria del manuale ("Signal Generators:Basic Oscillators").
    inline void roleOf (const juce::String& name, juce::String& group, juce::String& role)
    {
        if (const auto* h = CsoundOpcodeHelpData::find (name))
        {
            const juce::String cat (h->category);
            group = cat.upToFirstOccurrenceOf (":", false, false);
            role  = cat.fromFirstOccurrenceOf (":", false, false);
            if (role.isEmpty()) role = group;
            return;
        }
        group = "Other"; role = {};
    }

    inline juce::Colour groupColour (const juce::String& group)
    {
        if (group == "Signal Generators")        return juce::Colour (0xff52b788);   // verde
        if (group == "Signal Modifiers")         return juce::Colour (0xfff2a541);   // ambra
        if (group == "Spectral Processing")      return juce::Colour (0xff2a9d8f);   // verde acqua
        if (group == "Signal I/O")               return juce::Colour (0xff8aa4c8);   // grigio azzurro
        if (group == "Instrument Control")       return juce::Colour (0xffb388eb);   // viola
        if (group == "Table Control")            return juce::Colour (0xffd4a373);   // sabbia
        if (group == "Mathematical Operations" || group == "expression") return juce::Colour (0xff9aa5b1);
        if (group == "source")                   return juce::Colour (0xffe4572e);   // corallo, come i cavi k
        return juce::Colour (0xff6b7884);
    }

    //==========================================================================
    void computeStages (Flow& flow, const csoundGraph::Graph& g, const juce::StringArray* sourceLines);

    /** Colonne (profondita'), spina e righe delle scatole di un flusso. */
    inline void arrange (Flow& flow)
    {
        // --- colonne: profondita' nel flusso, uscite tutte a destra ------------
        const int nb = (int) flow.boxes.size();
        std::vector<std::vector<int>> in ((size_t) nb), out ((size_t) nb);
        for (const auto& e : flow.edges)
            if (! e.feedback) { in[(size_t) e.to].push_back (e.from); out[(size_t) e.from].push_back (e.to); }
        int maxDepth = 0;
        for (int i = 0; i < nb; ++i)   // in ordine di riga: i cavi non all'indietro vanno sempre avanti
        {
            int d = 0;
            for (int p : in[(size_t) i]) d = juce::jmax (d, flow.boxes[(size_t) p].depth + 1);
            flow.boxes[(size_t) i].depth = d;
            maxDepth = juce::jmax (maxDepth, d);
        }
        for (auto& b : flow.boxes) if (b.sink && ! b.unused && out[(size_t) (&b - flow.boxes.data())].empty()) b.depth = maxDepth;

        // --- la spina: dall'uscita audio piu' lontana, all'indietro sui cavi audio
        int cur = -1;
        for (int i = 0; i < nb; ++i)
            if (flow.boxes[(size_t) i].sink && (flow.boxes[(size_t) i].rate == 'a' || flow.boxes[(size_t) i].rate == 'f'
                                                 || (! flow.audio && flow.boxes[(size_t) i].rate == 'k')))
                if (cur < 0 || flow.boxes[(size_t) i].depth > flow.boxes[(size_t) cur].depth) cur = i;
        while (cur >= 0)
        {
            flow.boxes[(size_t) cur].spine = true;
            int best = -1;
            for (const auto& e : flow.edges)
                if (e.to == cur && ! e.feedback && ! flow.boxes[(size_t) e.from].spine
                    && (e.rate == 'a' || e.rate == 'f' || ! flow.audio))
                    if (best < 0 || flow.boxes[(size_t) e.from].depth > flow.boxes[(size_t) best].depth) best = e.from;
            cur = best;
        }

        // Fuori dalla spina ogni scatola va il piu' a destra possibile, subito
        // prima di chi la usa: i rami arrivano corti e non attraversano la
        // spina (un inviluppo sta accanto alla moltiplicazione che lo usa).
        for (int i = nb; --i >= 0;)
        {
            auto& b = flow.boxes[(size_t) i];
            if (b.spine || b.unused || out[(size_t) i].empty() || (b.sink && out[(size_t) i].empty())) continue;
            int d = INT_MAX;
            for (int s2 : out[(size_t) i]) d = juce::jmin (d, flow.boxes[(size_t) s2].depth - 1);
            if (d != INT_MAX && d >= b.depth) b.depth = d;
        }

        // --- righe: spina 0, controllo sopra (-1, -2...), rami audio sotto ----
        std::set<std::pair<int, int>> used;
        for (auto& b : flow.boxes) if (b.spine) { b.row = 0; used.insert ({ b.depth, 0 }); }
        std::vector<int> order;
        for (int i = 0; i < nb; ++i) if (! flow.boxes[(size_t) i].spine && ! flow.boxes[(size_t) i].unused) order.push_back (i);
        std::stable_sort (order.begin(), order.end(), [&flow] (int a, int b) { return flow.boxes[(size_t) a].depth > flow.boxes[(size_t) b].depth; });
        for (int i : order)
        {
            auto& b = flow.boxes[(size_t) i];
            const int side = (b.rate == 'a' || b.rate == 'f') && ! b.source ? 1 : -1;
            int row = side;
            if (b.sink && ! used.count ({ b.depth, 0 })) row = 0;   // uscite senza spina: sulla riga centrale
            while (used.count ({ b.depth, row })) row += side;
            b.row = row;
            used.insert ({ b.depth, row });
        }
        // non collegati: righe proprie sotto tutto il resto
        int bottom = 0;
        for (const auto& b : flow.boxes) if (! b.unused) bottom = juce::jmax (bottom, b.row);
        for (auto& b : flow.boxes)
            if (b.unused)
            {
                int row = bottom + 1;
                while (used.count ({ b.depth, row })) ++row;
                b.row = row;
                used.insert ({ b.depth, row });
            }

    }

    // Le condizioni come scatole: ogni catena if / elseif / else che conta
    // (racchiude qualcosa del flusso) e' UNA scatola "if cond":
    //  - entra la condizione (kTic == 1: dal metro);
    //  - una variabile scritta in due o piu' rami ("if kamp > 0.5 then asig =
    //    asig1 else asig = asig2") esce da qui: la scatola sceglie (come
    //    [gate] / [selector~] in Max); le copie semplici spariscono;
    //  - da qui parte un filo "solo se" (tratteggiato) verso cio' che sta
    //    dentro i rami (kNota = ..., kPasso += 1, un if annidato).
    inline void mergeBranches (Flow& flow, const csoundGraph::Graph& g)
    {
        using namespace csoundGraphUI;
        using K = csoundGraph::NodeKind;
        const int nn = (int) g.nodes.size(), nf = (int) g.frames.size();
        auto boxOfNodeFn = [&flow, nn]
        {
            std::vector<int> m ((size_t) nn, -1);
            for (int i = 0; i < (int) flow.boxes.size(); ++i)
                if (flow.boxes[(size_t) i].node >= 0 && flow.boxes[(size_t) i].node < nn) m[(size_t) flow.boxes[(size_t) i].node] = i;
            return m;
        };
        auto boxOfNode = boxOfNodeFn();
        std::vector<bool> removed (flow.boxes.size(), false);
        // la catena (id dell'if) di ogni cornice, -1 per while / for
        auto chainOf = [&g, nf] (int f) { return f >= 0 && f < nf && (g.frames[(size_t) f].kind == "if" || g.frames[(size_t) f].kind == "elseif" || g.frames[(size_t) f].kind == "else") ? g.frames[(size_t) f].chain : -1; };
        // dentro la catena r, a qualunque profondita'?
        auto inside = [&g, nf] (int frame, int r)
        {
            for (int f = frame; f >= 0 && f < nf; f = g.frames[(size_t) f].parent)
                if (g.frames[(size_t) f].chain == r) return true;
            return false;
        };
        std::map<int, int> boxOfChain;      // id dell'if -> scatola
        bool changed = false;

        for (const auto& root : g.frames)
        {
            if (root.kind != "if" || root.chain != root.id) continue;
            std::vector<const csoundGraph::Frame*> branches { &root };
            for (const auto& f : g.frames)
                if (f.chain == root.id && f.id != root.id && (f.kind == "elseif" || f.kind == "else")) branches.push_back (&f);
            std::stable_sort (branches.begin(), branches.end(), [] (auto* x, auto* y) { return x->lineBegin < y->lineBegin; });

            // conta? qualcosa del flusso (scatola o evento) sta dentro
            bool relevant = false;
            for (const auto& bx : flow.boxes)
                if (bx.node >= 0 && bx.node < nn && inside (g.nodes[(size_t) bx.node].frame, root.id)) relevant = true;
            for (const auto& ev : flow.events)
                if (ev.node >= 0 && ev.node < nn && inside (g.nodes[(size_t) ev.node].frame, root.id)) relevant = true;
            if (! relevant) continue;

            Box s;
            s.select = true;
            s.node = root.head;
            s.line = root.lineBegin;
            s.lineEnd = branches.back()->lineEnd;
            s.rate = 'k';
            s.title = "if " + shorten (js (root.cond), 28);
            s.role = "condition";
            s.group = "Instrument Control";
            const int si = (int) flow.boxes.size();
            std::vector<Edge> add;
            for (const auto* fr : branches)                      // la condizione (anche degli elseif)
                if (fr->head >= 0)
                    for (const auto& c : g.cables)
                        if (c.to == fr->head && c.from >= 0 && c.from < nn && boxOfNode[(size_t) c.from] >= 0 && ! removed[(size_t) boxOfNode[(size_t) c.from]])
                            add.push_back ({ boxOfNode[(size_t) c.from], si, 0, c.rate == 'a' ? 'a' : 'k', "condition", false, js (c.var), false });

            // variabili scritte in due o piu' rami: la scelta esce da qui
            std::map<std::string, std::map<int, int>> writes;
            for (int bi = 0; bi < (int) branches.size(); ++bi)
                for (int i = 0; i < (int) flow.boxes.size(); ++i)
                {
                    const auto& b = flow.boxes[(size_t) i];
                    if (removed[(size_t) i] || b.node < 0 || b.node >= nn || b.select) continue;
                    const auto& nd = g.nodes[(size_t) b.node];
                    if (nd.frame != branches[(size_t) bi]->id || nd.outs.size() != 1 || nd.outs[0].isArray || nd.outs[0].partial || nd.outs[0].global) continue;
                    writes[nd.outs[0].var][bi] = i;
                }
            juce::StringArray chosen;
            for (const auto& [var, perBranch] : writes)
            {
                if (perBranch.size() < 2) continue;              // un ramo solo: e' "solo se", non una scelta
                chosen.add (js (var));
                if (s.rate != 'a') s.rate = flow.boxes[(size_t) perBranch.begin()->second].rate;
                for (int bi = 0; bi < (int) branches.size(); ++bi)
                {
                    const auto* fr = branches[(size_t) bi];
                    const juce::String label = bi == 0 ? juce::String ("then") : fr->kind == "else" ? juce::String ("else") : "elseif " + shorten (js (fr->cond), 20);
                    const auto it = perBranch.find (bi);
                    if (it == perBranch.end()) { s.params.push_back ({ label, js (var) + " unchanged" }); continue; }
                    const int bx = it->second;
                    const auto& nd = g.nodes[(size_t) flow.boxes[(size_t) bx].node];
                    const bool copy = nd.kind == K::Expr && nd.ins.size() == 1 && nd.ins[0].vars.size() == 1
                                   && js (nd.ins[0].text).trim() == js (nd.ins[0].vars[0]);
                    s.params.push_back ({ label, shorten (js (nd.outs[0].var) + " = " + js (nd.text).trim(), 30) });
                    if (copy)
                    {
                        for (const auto& e : flow.edges)
                            if (e.to == bx) add.push_back ({ e.from, si, bi + 1, e.rate, label, e.feedback, e.var, false });
                        removed[(size_t) bx] = true;
                    }
                    else
                        add.push_back ({ bx, si, bi + 1, flow.boxes[(size_t) bx].rate, label, false, js (var), false });
                    for (auto& e : flow.edges)
                        if (e.from == bx) e.from = si;
                    s.sink = s.sink || flow.boxes[(size_t) bx].sink;
                    if (s.outNote.isEmpty()) s.outNote = flow.boxes[(size_t) bx].outNote;
                }
            }
            if (! chosen.isEmpty()) s.role = "chooses " + chosen.joinIntoString (", ");
            flow.boxes.push_back (s);
            removed.push_back (false);
            for (auto& e : add) flow.edges.push_back (e);
            boxOfChain[root.id] = si;
            changed = true;
        }
        if (! changed) return;

        // "solo se": dalla scatola della condizione a cio' che sta subito dentro
        // i suoi rami (un if annidato: dalla sua scatola)
        for (int i = 0; i < (int) flow.boxes.size(); ++i)
        {
            if (removed[(size_t) i]) continue;
            const auto& b = flow.boxes[(size_t) i];
            if (b.node < 0 || b.node >= nn) continue;
            int frame = g.nodes[(size_t) b.node].frame;
            if (b.select && frame >= 0 && chainOf (frame) == g.frames[(size_t) frame].chain && g.nodes[(size_t) b.node].kind == K::FrameHead)
                frame = g.frames[(size_t) frame].parent;            // la testa di un if: conta la cornice che lo racchiude
            const int r = chainOf (frame);
            if (r < 0 || boxOfChain.count (r) == 0 || boxOfChain[r] == i) continue;
            bool already = false;
            for (const auto& e : flow.edges) already = already || (e.from == boxOfChain[r] && e.to == i);
            if (! already)
            {
                const auto& fr = g.frames[(size_t) frame];
                flow.edges.push_back ({ boxOfChain[r], i, -1, 'k', fr.kind == "else" ? juce::String ("else") : juce::String ("then"), false, {}, true });
            }
        }

        // via le scatole tolte e i cavi doppi, nell'ordine delle righe
        std::vector<int> order;
        for (size_t i = 0; i < flow.boxes.size(); ++i) if (! removed[i]) order.push_back ((int) i);
        std::stable_sort (order.begin(), order.end(), [&flow] (int x, int y) { return flow.boxes[(size_t) x].line < flow.boxes[(size_t) y].line; });
        std::vector<int> remap (flow.boxes.size(), -1);
        std::vector<Box> boxes;
        for (int i : order) { remap[(size_t) i] = (int) boxes.size(); boxes.push_back (flow.boxes[(size_t) i]); }
        std::vector<Edge> edges;
        std::set<std::tuple<int, int, int>> seen;
        for (auto e : flow.edges)
        {
            if (e.from < 0 || e.to < 0 || remap[(size_t) e.from] < 0 || remap[(size_t) e.to] < 0) continue;
            e.from = remap[(size_t) e.from]; e.to = remap[(size_t) e.to];
            if (e.from == e.to || ! seen.insert ({ e.from, e.to, e.toIn }).second) continue;
            edges.push_back (e);
        }
        flow.boxes = std::move (boxes);
        flow.edges = std::move (edges);
    }

    inline Flow build (const csoundGraph::Orchestra& orc, int blockIndex, const juce::StringArray* sourceLines = nullptr)
    {
        using namespace csoundGraphUI;
        using K = csoundGraph::NodeKind;
        Flow flow;
        if (blockIndex < 0 || blockIndex >= (int) orc.blocks.size()) return flow;
        const auto& g = orc.blocks[(size_t) blockIndex];
        const int n = (int) g.nodes.size();
        auto signalRate = [] (char r) { return r == 'a' || r == 'k' || r == 'f'; };

        // --- uscite ---------------------------------------------------------
        auto audioSink = [&] (const csoundGraph::Node& nd)
        {
            if (nd.kind == K::GlobalIn || nd.kind == K::PField) return false;
            if (nd.audioOut || nd.name == "xout") return true;
            if (nd.channelWrite && nd.rate == 'a') return true;
            for (const auto& o : nd.outs) if (o.global && o.rate == 'a') return true;
            return false;
        };
        std::vector<int> sinks;
        for (int i = 0; i < n; ++i) if (audioSink (g.nodes[(size_t) i])) sinks.push_back (i);
        flow.audio = ! sinks.empty();
        if (sinks.empty())   // strumento di solo controllo: le sue "uscite" sono canali, globali, eventi
            for (int i = 0; i < n; ++i)
            {
                const auto& nd = g.nodes[(size_t) i];
                if (nd.kind == K::GlobalIn || nd.kind == K::PField) continue;
                bool out = nd.channelWrite || ! nd.eventTarget.empty();
                for (const auto& o : nd.outs) out = out || o.global;
                if (out) sinks.push_back (i);
            }

        // --- si risale dalle uscite (slice all'indietro) -----------------------
        std::vector<std::vector<int>> preds ((size_t) n);
        for (const auto& c : g.cables)
            if (c.from >= 0 && c.to >= 0 && c.from < n && c.to < n && c.from != c.to)
                preds[(size_t) c.to].push_back (c.from);
        std::vector<bool> inSlice ((size_t) n, false);
        std::vector<int> stack (sinks);
        while (! stack.empty())
        {
            const int i = stack.back(); stack.pop_back();
            if (inSlice[(size_t) i]) continue;
            inSlice[(size_t) i] = true;
            for (int p : preds[(size_t) i]) if (! inSlice[(size_t) p]) stack.push_back (p);
            // dipende anche dalle condizioni che lo racchiudono ("if kTic == 1":
            // l'evento dentro dipende da kTic, quindi dal metro)
            for (int f = g.nodes[(size_t) i].frame; f >= 0 && f < (int) g.frames.size(); f = g.frames[(size_t) f].parent)
            {
                const auto& fr = g.frames[(size_t) f];
                for (const auto& o : g.frames)       // la sua catena: if, elseif precedenti
                    if (o.chain == fr.chain && o.lineBegin <= fr.lineBegin && o.head >= 0 && o.head < n && ! inSlice[(size_t) o.head])
                        stack.push_back (o.head);
            }
        }

        // --- scatole: segnali e sorgenti di controllo; il resto si piega ------
        std::vector<int> boxOf ((size_t) n, -1);
        const std::set<int> sinkSet (sinks.begin(), sinks.end());
        for (int i = 0; i < n; ++i)
        {
            const auto& nd = g.nodes[(size_t) i];
            // fuori dal flusso: si mostrano comunque (a parte, sotto) i segnali
            // calcolati che non arrivano a nessuna uscita
            bool writesOut = nd.channelWrite || nd.audioOut;        // canali, globali: escono comunque dallo strumento
            for (const auto& o : nd.outs) writesOut = writesOut || o.global;
            const bool dangling = ! inSlice[(size_t) i] && nd.eventTarget.empty() && ! writesOut && ! nd.outs.empty()   // senza uscite (un UDO che scrive canali): non e' "scollegato"
                               && (nd.kind == K::Opcode || nd.kind == K::Expr) && signalRate (nd.rate);
            if (! inSlice[(size_t) i] && ! dangling) continue;
            // "asig init 0" e poi asig riscritta altrove: solo il valore di
            // partenza, non un segnale a se' (non e' "non collegato")
            if (dangling && nd.name == "init" && nd.outs.size() == 1)
            {
                bool rewritten = false;
                for (int j = 0; j < n && ! rewritten; ++j)
                    if (j != i)
                        for (const auto& o : g.nodes[(size_t) j].outs) rewritten = rewritten || o.var == nd.outs[0].var;
                if (rewritten) continue;
            }
            const bool sink = sinkSet.count (i) > 0;
            if (! nd.eventTarget.empty()) continue;      // gli eventi sono schede a parte (sotto)
            bool keep = sink || dangling
                     || ((nd.kind == K::Opcode || nd.kind == K::Expr) && signalRate (nd.rate))
                     || (nd.kind == K::GlobalIn && signalRate (nd.rate))
                     || (nd.kind == K::Opcode && ! nd.outs.empty() && ! nd.outs[0].var.empty() && nd.outs[0].var[0] == 'S');   // un testo che arriva a un'uscita (sprintfk -> chnset)
            // bus che si accumula ("gaRev = gaRev + aSig"): il "receive" del
            // bus stesso e' solo il valore precedente, non una sorgente
            if (keep && nd.kind == K::GlobalIn)
            {
                bool onlyAccumulates = true, any = false;
                for (const auto& c : g.cables)
                    if (c.from == i && c.to >= 0 && c.to < n)
                    {
                        any = true;
                        bool writesSame = false;
                        for (const auto& o : g.nodes[(size_t) c.to].outs) writesSame = writesSame || o.var == nd.name;
                        onlyAccumulates = onlyAccumulates && writesSame;
                    }
                if (any && onlyAccumulates) keep = false;
            }
            if (! keep) continue;
            Box b;
            b.node = i; b.rate = signalRate (nd.rate) ? nd.rate : 'i'; b.sink = sink; b.unused = dangling;
            b.line = nd.line; b.lineEnd = juce::jmax (nd.line, nd.lineEnd);
            if (dangling && ! nd.outs.empty())
            {
                bool read = false;
                for (const auto& c : g.cables) read = read || c.from == i;
                if (! read) b.outNote = js (nd.outs[0].var) + ": nobody reads it";
                b.outVar = js (nd.outs[0].var);
            }
            if (nd.kind == K::GlobalIn)
            {
                b.source = true; b.title = "receive " + js (nd.name); b.group = "source";
                juce::StringArray from;
                for (const auto& l : orc.links)
                    if (l.to == blockIndex && js (l.name) == js (nd.name) && l.from >= 0)
                        from.addIfNotAlreadyThere (js (orc.blocks[(size_t) l.from].title()));
                b.role = from.isEmpty() ? juce::String ("global variable") : "from " + from.joinIntoString (", ");
            }
            else if (nd.kind == K::Expr)
            {
                juce::StringArray lhs;
                for (const auto& o : nd.outs) lhs.add (js (o.var));
                const auto rhs = js (nd.text).trim();
                // "aS = aS * aEnv" -> "aS * aEnv" basta (la variabile scritta e' gia' sul cavo)
                const auto op = js (nd.name);
                const bool compound = op.length() == 2 && op.endsWithChar ('=') && op != "==";   // +=, -=, *=, /=
                b.title = shorten (compound && ! lhs.isEmpty() ? lhs[0] + " " + op + " " + rhs
                                   : lhs.isEmpty() || rhs.contains (lhs[0]) ? rhs : lhs.joinIntoString (", ") + " = " + rhs, 30);
                b.group = "expression"; b.role = "expression";
            }
            else
            {
                b.title = js (nd.name);
                roleOf (b.title, b.group, b.role);
                if (! nd.channel.empty())
                {
                    bool host = false;
                    for (const auto& hc : orc.hostChannels) host = host || hc == nd.channel;
                    if (! nd.channelWrite) { b.source = true; b.group = "source"; b.role = host ? "parameter (host / panel)" : "channel"; }
                    b.title << "  \"" << js (nd.channel) << "\"";
                    b.channel = js (nd.channel);
                }
            }
            if (sink)
            {
                juce::StringArray notes;
                if (nd.audioOut) notes.add ("audio out");
                if (nd.name == "xout") notes.add ("returns to the caller");
                if (! nd.eventTarget.empty()) notes.add ("starts instr " + js (nd.eventTarget));
                if (nd.channelWrite && ! nd.channel.empty()) notes.add ("channel \"" + js (nd.channel) + "\"");
                for (const auto& o : nd.outs)
                    if (o.global)
                    {
                        juce::StringArray to;
                        for (const auto& l : orc.links)
                            if (l.from == blockIndex && l.name == o.var && l.to >= 0)
                                to.addIfNotAlreadyThere (js (orc.blocks[(size_t) l.to].title()));
                        notes.add (js (o.var) + (to.isEmpty() ? juce::String() : " to " + to.joinIntoString (", ")));
                    }
                b.outNote = notes.joinIntoString ("; ");
            }
            boxOf[(size_t) i] = (int) flow.boxes.size();
            flow.boxes.push_back (b);
        }
        // --- eventi: una scheda per ogni nota avviata da questo blocco -------
        for (int i = 0; i < n; ++i)
        {
            const auto& nd = g.nodes[(size_t) i];
            if (nd.eventTarget.empty() || nd.kind != K::Opcode) continue;
            EventCard card;
            card.node = i; card.opcode = js (nd.name); card.target = js (nd.eventTarget);
            juce::StringArray chain;
            for (int f = nd.frame; f >= 0 && f < (int) g.frames.size(); f = g.frames[(size_t) f].parent)
                chain.insert (0, js (g.frames[(size_t) f].kind) + " " + js (g.frames[(size_t) f].cond));
            card.frames = chain;

            // posizione del primo argomento "instr": schedule 0, event 1, schedkwhen 3, schedwhen 1
            int first = 0;
            if (card.opcode == "event" || card.opcode == "event_i" || card.opcode == "schedwhen") first = 1;
            if (card.opcode == "schedkwhen" || card.opcode == "schedkwhennamed") first = 3;

            // lo strumento che riceve la nota: dove usa ciascun p-field
            const csoundGraph::Graph* target = nullptr;
            for (const auto& tg : orc.blocks)
                if (tg.kind == csoundGraph::Graph::Instr)
                    for (const auto& nm : tg.names) if (nm == nd.eventTarget) target = &tg;
            auto usage = [target] (int p)
            {
                if (target == nullptr) return juce::String();
                juce::StringArray uses;
                for (int k = 0; k < (int) target->nodes.size(); ++k)
                {
                    const auto& pn = target->nodes[(size_t) k];
                    if (pn.kind != K::PField || pn.name != "p" + std::to_string (p)) continue;
                    for (const auto& c : target->cables)
                        if (c.from == k && c.to >= 0 && c.to < (int) target->nodes.size())
                        {
                            const auto& u = target->nodes[(size_t) c.to];
                            const auto lbl = c.toIn < (int) u.ins.size() ? js (u.ins[(size_t) c.toIn].label) : juce::String();
                            const auto who = u.kind == K::Expr && ! u.outs.empty() ? js (u.outs[0].var) : js (u.name);
                            uses.addIfNotAlreadyThere (who + (lbl.isNotEmpty() ? " " + lbl : juce::String()));
                        }
                }
                // prima chi fa suono, in fondo stampe e canali di stato
                auto weight = [] (const juce::String& u)
                {
                    const auto op = u.upToFirstOccurrenceOf (" ", false, false);
                    static const juce::StringArray low { "sprintf", "sprintfk", "prints", "printks", "print", "printf", "printf_i", "puts", "chnset", "if" };
                    return low.contains (op) ? 1 : 0;
                };
                std::stable_sort (uses.strings.begin(), uses.strings.end(), [&weight] (const juce::String& x, const juce::String& y) { return weight (x) < weight (y); });
                if (uses.size() > 3) { uses.removeRange (3, uses.size()); uses.add (u8 ("\xe2\x80\xa6")); }
                return uses.joinIntoString (", ");
            };

            for (int k = 0; k < (int) nd.ins.size(); ++k)
            {
                const auto text = shorten (js (nd.ins[(size_t) k].text), 40);
                if (card.opcode.startsWith ("scoreline")) { card.rows.push_back ({ "score", text, {} }); continue; }
                const int p = k - first + 1;            // 1 = instr, 2 = start, 3 = dur, 4...
                if (p < 1) { card.rows.push_back ({ js (nd.ins[(size_t) k].label), text, {} }); continue; }
                const juce::String field = p == 1 ? "instr" : p == 2 ? "start" : p == 3 ? "dur" : "p" + juce::String (p);
                card.rows.push_back ({ field, text, p >= 3 ? usage (p) : juce::String() });
            }
            flow.events.push_back (card);
        }

        flow.hidden = n - (int) flow.boxes.size() - (int) flow.events.size();

        // --- ingressi: cavo se arriva da una scatola, altrimenti riga di testo -
        std::set<std::tuple<int, int, int>> seenEdge;
        for (auto& b : flow.boxes)
        {
            const auto& nd = g.nodes[(size_t) b.node];
            const int me = boxOf[(size_t) b.node];
            for (int k = 0; k < (int) nd.ins.size(); ++k)
            {
                const auto& in = nd.ins[(size_t) k];
                const auto label = js (in.label).isNotEmpty() ? js (in.label) : juce::String ("in " + juce::String (k + 1));
                bool wired = false;
                for (const auto& c : g.cables)
                    if (c.to == b.node && c.toIn == k && c.from >= 0 && c.from < n && boxOf[(size_t) c.from] >= 0)
                    {
                        wired = true;
                        if (seenEdge.insert ({ boxOf[(size_t) c.from], me, k }).second)
                            flow.edges.push_back ({ boxOf[(size_t) c.from], me, k, c.rate, label, c.feedback, js (c.var) });
                    }
                // riga di testo: ingresso non collegato, oppure collegato ma
                // dentro un'espressione ("iAmp * gkVol": il cavo porta gkVol)
                const auto text = js (in.text);
                const bool plainVar = in.vars.size() == 1 && js (in.vars[0]) == text.trim();
                if (text.isNotEmpty() && nd.kind != K::Expr && (! wired || ! plainVar))
                    b.params.push_back ({ label, shorten (text, 34) });
            }
        }

        mergeBranches (flow, g);
        arrange (flow);
        const int nb = (int) flow.boxes.size();
        // --- riassunto in parole --------------------------------------------
        std::vector<int> spine;
        for (int i = 0; i < nb; ++i) if (flow.boxes[(size_t) i].spine) spine.push_back (i);
        std::sort (spine.begin(), spine.end(), [&flow] (int a, int b) { return flow.boxes[(size_t) a].depth < flow.boxes[(size_t) b].depth; });
        const auto arrow = u8 ("  \xe2\x86\x92  ");
        if (! spine.empty())
        {
            juce::StringArray parts;
            for (int i : spine)
            {
                const auto& b = flow.boxes[(size_t) i];
                parts.add (b.title + (b.role.isNotEmpty() && b.group != "expression" ? " (" + b.role.toLowerCase() + ")" : juce::String()));
            }
            const auto& last = flow.boxes[(size_t) spine.back()];
            if (last.outNote.isNotEmpty()) parts.add (last.outNote);
            // uno strumento che fa partire note: la cosa principale sono le note
            if (! flow.audio)
                for (const auto& ev : flow.events)
                    flow.summary.add ("Starts notes of instr " + ev.target + " (" + ev.opcode + ")"
                                      + (ev.frames.isEmpty() ? juce::String() : ", " + ev.frames[ev.frames.size() - 1]));
            flow.summary.add ((flow.audio ? "Audio path: " : flow.events.empty() ? "Main path: " : "Also: ") + parts.joinIntoString (arrow));
        }
        else if (! flow.audio)
            flow.summary.add ("Control instrument: no audio output");
        for (const auto& b : flow.boxes)
            if (b.sink && ! b.spine && b.outNote.isNotEmpty())
            {
                const auto& nd = g.nodes[(size_t) b.node];
                juce::String where;
                for (int f = nd.frame; f >= 0 && f < (int) g.frames.size(); f = g.frames[(size_t) f].parent)
                    where << "  (inside " << js (g.frames[(size_t) f].kind) << " " << js (g.frames[(size_t) f].cond) << ")";
                flow.summary.add ("Output: " + b.title + arrow + b.outNote + where);
            }
        // chi modula la catena: una riga per scatola della spina che riceve
        // segnali da fuori della spina ("gbuzz  <-  jspline (xcps), expseg (kmul)")
        const auto leftArrow = u8 ("  \xe2\x86\x90  ");
        int lines = 0;
        for (int i : spine)
        {
            juce::StringArray from;
            for (const auto& e : flow.edges)
                if (e.to == i && ! flow.boxes[(size_t) e.from].spine)
                    from.addIfNotAlreadyThere (flow.boxes[(size_t) e.from].title + (e.label.isNotEmpty() ? " (" + e.label + ")" : juce::String()));
            if (from.isEmpty()) continue;
            if (++lines > 5) { flow.summary.add (u8 ("\xe2\x80\xa6")); break; }
            flow.summary.add (flow.boxes[(size_t) i].title + leftArrow + from.joinIntoString (", "));
        }
        if (! flow.events.empty())
        {
            juce::StringArray ev;
            for (const auto& c : flow.events)
                ev.add ("instr " + c.target + (c.frames.isEmpty() ? juce::String() : " (" + c.frames[c.frames.size() - 1] + ")"));
            flow.summary.add ("Starts notes: " + ev.joinIntoString (", "));
        }
        {
            juce::StringArray dangling;
            for (const auto& b : flow.boxes)
                if (b.unused)
                {
                    const auto& nd = g.nodes[(size_t) b.node];
                    dangling.add ((nd.outs.empty() ? juce::String() : js (nd.outs[0].var) + " ") + "(" + b.title + ")");
                }
            if (! dangling.isEmpty())
                flow.summary.add ("Not connected to the output (computed, but nothing uses it): " + dangling.joinIntoString (", ") + ".");
        }
        if (flow.hidden > 0)
            flow.summary.add ("Not shown: " + juce::String (flow.hidden) + " boxes that do not reach the output or only compute values once (see Explain).");
        computeStages (flow, g, sourceLines);
        return flow;
    }

    //==========================================================================
    // Blocchi funzionali: il ruolo musicale di ogni scatola, dalla categoria del
    // manuale e dalla posizione nel flusso.
    enum StageKind { Controls, Sources, TimeShape, Spectral, Transform, Space, Output, numStageKinds };

    inline juce::String stageTitle (int k)
    {
        static const char* t[] = { "Controls", "Sound sources", "Shaping in time", "Spectral processing", "Transformation", "Space and effects", "Output" };
        return k >= 0 && k < numStageKinds ? juce::String (t[k]) : juce::String();
    }
    inline juce::Colour stageColour (int k)
    {
        switch (k)
        {
            case Controls:  return juce::Colour (0xffe4572e);
            case Sources:   return juce::Colour (0xff52b788);
            case TimeShape: return juce::Colour (0xffe9c46a);
            case Spectral:  return juce::Colour (0xff2a9d8f);
            case Transform: return juce::Colour (0xfff2a541);
            case Space:     return juce::Colour (0xff9b6fd0);
            default:        return juce::Colour (0xff3b82f6);
        }
    }

    inline void computeStages (Flow& flow, const csoundGraph::Graph& g, const juce::StringArray* sourceLines)
    {
        using namespace csoundGraphUI;
        const int nb = (int) flow.boxes.size();
        std::vector<int> kind ((size_t) nb, -1);
        auto shapeRole = [] (const Box& b)
        {
            return b.role.containsIgnoreCase ("Envelope") || b.role.containsIgnoreCase ("Linear and Exponential")
                || b.title.startsWith ("lfo") || b.title.startsWith ("jspline") || b.title.startsWith ("rspline")
                || b.title.startsWith ("randi") || b.title.startsWith ("randh") || b.title.startsWith ("madsr") || b.title.startsWith ("adsr");
        };
        for (int i = 0; i < nb; ++i)
        {
            const auto& b = flow.boxes[(size_t) i];
            const auto& nd = g.nodes[(size_t) b.node];
            int k = -1;
            if (b.unused) k = -1;
            else if (b.sink) k = Output;
            else if (b.source) k = b.rate == 'a' ? Sources : Controls;
            else if (b.group == "expression") k = -2;                     // dopo: dagli ingressi
            else if (b.group == "Spectral Processing") k = Spectral;      // anche i segnali f (il modello li segna k)
            else if (b.rate != 'a' && b.rate != 'f') k = shapeRole (b) ? TimeShape : Controls;
            else if (b.group == "Spectral Processing" || b.rate == 'f') k = Spectral;
            else if (b.group == "Signal Generators") k = shapeRole (b) ? TimeShape : Sources;
            else if (b.role.containsIgnoreCase ("Reverb") || b.role.containsIgnoreCase ("Delay") || b.role.containsIgnoreCase ("Panning")
                     || b.role.containsIgnoreCase ("Spatial") || b.role.containsIgnoreCase ("Convolution")) k = Space;
            else if (b.group == "Signal I/O") k = nd.audioIn ? Sources : Output;
            else k = Transform;
            kind[(size_t) i] = k;
        }
        // espressioni: il blocco piu' avanti fra quelli da cui ricevono; di controllo se non sono audio
        for (int pass = 0; pass < 3; ++pass)
            for (int i = 0; i < nb; ++i)
            {
                if (flow.boxes[(size_t) i].unused) continue;                 // i non collegati non stanno in nessun blocco
                if (kind[(size_t) i] != -2 && ! (pass > 0 && flow.boxes[(size_t) i].group == "expression")) continue;
                const auto& b = flow.boxes[(size_t) i];
                if (b.rate != 'a' && b.rate != 'f') { kind[(size_t) i] = Controls; continue; }
                int best = -1;
                for (const auto& e : flow.edges)
                    if (e.to == i && ! e.feedback && kind[(size_t) e.from] > Controls && kind[(size_t) e.from] != Output)
                        best = juce::jmax (best, kind[(size_t) e.from]);
                kind[(size_t) i] = best >= 0 ? best : (b.sink ? (int) Output : (int) Transform);
            }

        // sezioni del codice: il commento a riga intera piu' vicino sopra la scatola
        auto sectionOf = [&] (int line) -> juce::String
        {
            if (sourceLines == nullptr) return {};
            for (int l = line - 1; l >= juce::jmax (1, g.lineBegin); --l)
            {
                const auto t = (*sourceLines)[l - 1].trim();
                if (t.startsWithChar (';') || t.startsWith ("//"))
                {
                    // via i segni di cornice (anche ---, ===, ─── unicode) all'inizio e alla fine
                    auto c = t;
                    while (c.isNotEmpty() && ! juce::CharacterFunctions::isLetterOrDigit (c[0])) c = c.substring (1);
                    while (c.isNotEmpty() && ! juce::CharacterFunctions::isLetterOrDigit (c.getLastCharacter()) && c.getLastCharacter() != ')') c = c.dropLastCharacters (1);
                    if (c.length() >= 3) return c;
                }
            }
            return {};
        };

        flow.stageOfBox.assign ((size_t) nb, -1);

        // (la divisione dei controlli in sotto-blocchi e' in Archive/CsoundGraphControlsSplit.archived.txt)
        std::vector<juce::String> sub ((size_t) nb);
        const int nControls = 0;
        auto subTitle = [] (const juce::String& k2)
        {
            if (k2 == "panel")       return juce::String ("Controls: panel and DAW");
            if (k2 == "instruments") return juce::String ("Controls: from other instruments");
            if (k2 == "note")        return juce::String ("Controls: from the note");
            if (k2 == "tables")      return juce::String ("Controls: tables");
            if (k2 == "calc:")       return juce::String ("Controls: calculations");
            return "Controls: " + csoundGraphUI::shorten (k2.fromFirstOccurrenceOf ("calc:", false, false), 30);
        };
        std::vector<std::pair<int, juce::String>> keys;
        for (int k = 0; k < numStageKinds; ++k)
        {
            if (k != Controls || nControls <= 12) { keys.push_back ({ k, {} }); continue; }
            juce::StringArray subs { "panel", "instruments", "note", "tables" };
            for (int i = 0; i < nb; ++i) if (kind[(size_t) i] == Controls && sub[(size_t) i].startsWith ("calc:")) subs.addIfNotAlreadyThere (sub[(size_t) i]);
            for (auto& k2 : subs) keys.push_back ({ k, k2 });
        }

        std::map<int, int> stageIndex;
        for (const auto& [k, k2] : keys)
        {
            Stage st; st.kind = k;
            for (int i = 0; i < nb; ++i) if (kind[(size_t) i] == k && sub[(size_t) i] == k2) st.boxes.push_back (i);
            if (st.boxes.empty()) continue;
            st.title = k2.isEmpty() ? stageTitle (k) : subTitle (k2);
            // le scatole piu' frequenti: "pvsynth x8, tab2pvs x5"
            std::map<juce::String, int> count;
            juce::StringArray order;
            for (int i : st.boxes)
            {
                auto t = flow.boxes[(size_t) i].title;
                if (flow.boxes[(size_t) i].group == "expression") t = "expressions";
                if (! count.count (t)) order.add (t);
                ++count[t];
            }
            std::stable_sort (order.strings.begin(), order.strings.end(), [&count] (const juce::String& a, const juce::String& b) { return count[a] > count[b]; });
            juce::StringArray parts;
            for (int j = 0; j < juce::jmin (3, order.size()); ++j)
                parts.add (order[j] + (count[order[j]] > 1 ? u8 (" \xc3\x97") + juce::String (count[order[j]]) : juce::String()));
            if (order.size() > 3) parts.add (juce::String (order.size() - 3) + " more");
            st.sentence = parts.joinIntoString (", ");
            // il commento che le accomuna (se la maggioranza ne ha uno)
            std::map<juce::String, int> sections;
            for (int i : st.boxes) if (auto c = sectionOf (g.nodes[(size_t) flow.boxes[(size_t) i].node].line); c.isNotEmpty()) ++sections[c];
            int bestCount = 0;
            for (auto& [c, nC] : sections) if (nC > bestCount) { bestCount = nC; st.comment = c; }
            if (bestCount * 2 < (int) st.boxes.size()) st.comment.clear();
            for (int i : st.boxes) flow.stageOfBox[(size_t) i] = (int) flow.stages.size();
            stageIndex[k] = (int) flow.stages.size();
            flow.stages.push_back (st);
        }

        // frecce fra blocchi (una per coppia, con i nomi dei segnali)
        std::map<std::pair<int, int>, size_t> seen;
        for (const auto& e : flow.edges)
        {
            const int a = flow.stageOfBox[(size_t) e.from], b = flow.stageOfBox[(size_t) e.to];
            if (a < 0 || b < 0 || a == b || e.feedback) continue;
            auto it = seen.find ({ a, b });
            if (it == seen.end())
            {
                seen[{ a, b }] = flow.stageEdges.size();
                flow.stageEdges.push_back ({ a, b, e.rate, e.var, 1 });
            }
            else
            {
                auto& se = flow.stageEdges[it->second];
                ++se.count;
                if (e.rate == 'a') se.rate = 'a';
                if (! se.label.contains (e.var) && se.label.length() < 24) se.label << ", " << e.var;
                else if (! se.label.endsWith ("...") && ! se.label.contains (e.var)) se.label << "...";
            }
        }
        // colonne: il cammino piu' lungo fra i blocchi (i controlli a sinistra, l'uscita a destra)
        const int ns = (int) flow.stages.size();
        for (int pass = 0; pass < ns; ++pass)
            for (const auto& se : flow.stageEdges)
                if (flow.stages[(size_t) se.from].kind < flow.stages[(size_t) se.to].kind || flow.stages[(size_t) se.to].kind == Output)
                    flow.stages[(size_t) se.to].column = juce::jmax (flow.stages[(size_t) se.to].column, flow.stages[(size_t) se.from].column + 1);
        int maxCol = 0;
        for (auto& st : flow.stages) maxCol = juce::jmax (maxCol, st.column);
        for (auto& st : flow.stages) if (st.kind == Output) st.column = juce::jmax (st.column, maxCol);
    }

    /** Un blocco aperto: solo le sue scatole, disposte da capo, con le porte
        verso gli altri blocchi ("from Controls", "to Output"). Le porte hanno
        node = -2 - indice del blocco: un tocco apre quel blocco. */
    inline Flow makeStageFlow (const Flow& full, int stage)
    {
        using csoundGraphUI::u8;
        Flow f;
        if (stage < 0 || stage >= (int) full.stages.size()) return full;
        f.audio = full.audio;
        f.stages = full.stages;
        const auto& st = full.stages[(size_t) stage];
        std::map<int, int> remap;
        for (int i : st.boxes)
        {
            Box b = full.boxes[(size_t) i];
            b.depth = 0; b.row = 0; b.spine = false;
            remap[i] = (int) f.boxes.size();
            f.boxes.push_back (b);
        }
        std::map<int, int> inPort, outPort;          // blocco -> scatola porta
        auto port = [&] (int otherStage, bool incoming, char rate, const juce::String& var) -> int
        {
            auto& m = incoming ? inPort : outPort;
            auto it = m.find (otherStage);
            if (it != m.end())
            {
                auto& b = f.boxes[(size_t) it->second];
                if (rate == 'a') b.rate = 'a';
                if (! b.role.contains (var) && b.role.length() < 40) b.role << (b.role.isEmpty() ? "" : ", ") << var;
                return it->second;
            }
            Box b;
            b.node = -2 - otherStage;
            b.rate = rate;
            b.title = (incoming ? "from " : "to ") + full.stages[(size_t) otherStage].title;
            b.role = var;
            b.group = "source";
            b.source = incoming;
            b.sink = ! incoming;
            m[otherStage] = (int) f.boxes.size();
            f.boxes.push_back (b);
            return m[otherStage];
        };
        for (const auto& e : full.edges)
        {
            const bool fromIn = remap.count (e.from) > 0, toIn = remap.count (e.to) > 0;
            if (fromIn && toIn) { auto x = e; x.from = remap[e.from]; x.to = remap[e.to]; f.edges.push_back (x); continue; }
            if (toIn && (size_t) e.from < full.stageOfBox.size() && full.stageOfBox[(size_t) e.from] >= 0)
            {
                auto x = e; x.from = port (full.stageOfBox[(size_t) e.from], true, e.rate, e.var); x.to = remap[e.to];
                f.edges.push_back (x);
            }
            else if (fromIn && (size_t) e.to < full.stageOfBox.size() && full.stageOfBox[(size_t) e.to] >= 0)
            {
                auto x = e; x.from = remap[e.from]; x.to = port (full.stageOfBox[(size_t) e.to], false, e.rate, e.var);
                x.toIn = 0;
                f.edges.push_back (x);
            }
        }
        // porte d'uscita: una freccia sola per blocco (niente cavi doppi sullo stesso ingresso)
        std::set<std::pair<int, int>> seen;
        std::vector<Edge> unique;
        for (auto& e : f.edges)
            if (! f.boxes[(size_t) e.to].sink || f.boxes[(size_t) e.to].node > -2 || seen.insert ({ e.from, e.to }).second) unique.push_back (e);
        f.edges = std::move (unique);
        for (auto& b : f.boxes) if (b.node <= -2 && b.sink) b.outNote = b.title.fromFirstOccurrenceOf ("to ", false, false);
        f.stageOfBox.assign (f.boxes.size(), stage);
        arrange (f);
        f.summary.add (st.title + (st.comment.isNotEmpty() ? u8 ("  \xe2\x80\x94  \xe2\x80\x9c") + st.comment + u8 ("\xe2\x80\x9d") : juce::String()));
        f.summary.add (st.sentence + u8 ("  \xc2\xb7  ") + "tap a \"from\" or \"to\" box to open the next block");
        return f;
    }
}

//==============================================================================
class CsoundGraphFlowCanvas final : public juce::Component
{
public:
    CsoundGraphFlowCanvas() = default;

    std::function<void (int node)> onNodeTapped;         // un tocco: selezione (indice in Graph::nodes)
    std::function<void (int node)> onNodeDoubleTapped;   // doppio tocco: aiuto e contesto
    std::function<void (int stage)> onStageTapped;       // vista d'insieme: un blocco toccato

    /** Vista d'insieme: i blocchi funzionali invece delle scatole. */
    bool hasOverview() const noexcept { return full.stages.size() >= 2; }
    bool isOverview() const noexcept  { return overview; }
    void setOverview (bool shouldShow)
    {
        overview = shouldShow && hasOverview();
        if (overview && focusStage >= 0) { focusStage = -1; flow = full; layout(); }
        applySize();
        repaint();
    }
    /** Il blocco aperto: le sue scatole accese, le altre in penombra (-1 = tutte). */
    void setFocusStage (int stage)
    {
        focusStage = stage >= 0 && stage < (int) full.stages.size() ? stage : -1;
        flow = focusStage >= 0 ? csoundFlow::makeStageFlow (full, focusStage) : full;
        selected = -1; selectedEvent = -1;
        layout();
        repaint();
    }
    int getFocusStage() const noexcept { return focusStage; }
    int getNumBoxes() const noexcept { return (int) flow.boxes.size(); }

    /** Riga del file (da 0) -> colore del blocco, per il margine del code editor. */
    std::map<int, juce::Colour> getLineColours() const
    {
        std::map<int, juce::Colour> m;
        if (! hasOverview()) return m;
        for (int i = 0; i < (int) full.boxes.size(); ++i)
        {
            const int st = i < (int) full.stageOfBox.size() ? full.stageOfBox[(size_t) i] : -1;
            const auto& b = full.boxes[(size_t) i];
            if (st < 0 || b.line <= 0) continue;
            const auto c = csoundFlow::stageColour (full.stages[(size_t) st].kind);
            for (int l = b.line; l <= b.lineEnd; ++l) m[l - 1] = c;
        }
        return m;
    }
    juce::String getStageTitle (int stage) const
    {
        return stage >= 0 && stage < (int) full.stages.size() ? full.stages[(size_t) stage].title : juce::String();
    }
    /** Le scatole di un blocco nello schema (per portarle in vista). */
    juce::Rectangle<float> getStageBounds (int stage) const
    {
        juce::Rectangle<float> r;
        if (stage == focusStage)
            for (const auto& b : flow.boxes) r = r.isEmpty() ? b.r : r.getUnion (b.r);
        return r * zoom;
    }
    std::function<void()>          onEmptyTapped;

    void setFlow (csoundFlow::Flow f)
    {
        full = std::move (f);
        stageOfNode.clear();
        for (size_t i = 0; i < full.boxes.size() && i < full.stageOfBox.size(); ++i)
            if (full.boxes[i].node >= 0 && full.stageOfBox[i] >= 0 && full.stageOfBox[i] < (int) full.stages.size())
                stageOfNode[full.boxes[i].node] = full.stages[(size_t) full.stageOfBox[i]].kind;
        flow = full;
        selected = -1;
        // ricostruito mentre si scrive: il blocco aperto resta aperto
        if (focusStage >= (int) full.stages.size()) focusStage = -1;
        if (focusStage >= 0) flow = csoundFlow::makeStageFlow (full, focusStage);
        if (! hasOverview()) overview = false;
        layoutOverview();
        layout();
        repaint();
    }

    juce::Rectangle<float> getNodeBounds (int node) const
    {
        for (const auto& b : flow.boxes) if (b.node == node) return b.r * zoom;
        for (const auto& c : flow.events) if (c.node == node) return c.r * zoom;
        return {};
    }

    //--------------------------------------------------------------------------
    // Zoom: pinch (iPad, trackpad), Cmd/Ctrl + rotellina. Lo schema e' disegnato
    // in coordinate proprie e scalato; il punto sotto le dita resta fermo.
    float getZoom() const noexcept { return zoom; }
    /** La vista ha cambiato misura: la tela la copre sempre tutta. */
    void viewportResized() { if (contentW > 0.0f) applySize(); }
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
    void clearSelection() { selected = -1; selectedEvent = -1; repaint(); }

    /** Dove comincia la catena audio (la prima scatola della spina; altrimenti
        la prima scatola): la vista ci si porta, anche quando lo schema e' grande
        e sopra ci sono molte righe di controlli. */
    juce::Rectangle<float> getStartBounds() const
    {
        const csoundFlow::Box* best = nullptr;
        for (const auto& b : flow.boxes)
            if (b.spine && (best == nullptr || b.depth < best->depth)) best = &b;
        if (best == nullptr)
            for (const auto& b : flow.boxes)
                if (best == nullptr || b.r.getY() < best->r.getY()) best = &b;
        return best != nullptr ? best->r * zoom : juce::Rectangle<float>();
    }

    /** Seleziona (e illumina) la scatola o la scheda di un nodo; false se non e' nel flusso. */
    bool selectNode (int node)
    {
        for (int i = 0; i < (int) flow.boxes.size(); ++i)
            if (flow.boxes[(size_t) i].node == node) { selected = i; selectedEvent = -1; repaint(); return true; }
        for (const auto& c : flow.events)
            if (c.node == node) { selected = -1; selectedEvent = node; repaint(); return true; }
        return false;
    }

    /** Valori correnti dei canali (motore in esecuzione): badge sulle scatole. */
    void setLiveValues (const std::map<juce::String, juce::String>& v)
    {
        if (v == live) return;
        live = v;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace csoundGraphUI;
        g.fillAll (kBg);
        g.addTransform (juce::AffineTransform::scale (zoom));     // da qui: coordinate dello schema
        if (overview) { paintOverview (g); return; }

        // riassunto in parole, in alto
        float y = pad * 0.5f;
        for (int i = 0; i < flow.summary.size(); ++i)
        {
            g.setColour (i == 0 ? kText : kTextMuted);
            g.setFont (i == 0 ? boldFont (13.5f) : plainFont (12.5f));
            g.drawText (flow.summary[i], juce::Rectangle<float> (pad, y, contentW - 2.0f * pad, 18.0f), juce::Justification::centredLeft, true);
            y += i == 0 ? 22.0f : 18.0f;
        }

        // la spina: fascia sottile dietro la riga 0
        if (spineBand.getWidth() > 0.0f)
        {
            g.setColour (rateColour ('a').withAlpha (0.07f));
            g.fillRoundedRectangle (spineBand, 12.0f);
        }

        std::set<int> lit;
        if (selected >= 0) lit = cone (selected);
        auto dim = [&] (int box)
        {
            if (selected >= 0) return lit.count (box) == 0;
            return focusStage >= 0 && (size_t) box < flow.stageOfBox.size() && flow.stageOfBox[(size_t) box] != focusStage;
        };

        // cavi
        cableHits.clear();
        std::vector<std::pair<juce::Path, juce::String>> liveLabels;
        std::set<juce::String> labelledVars;
        const auto clip = g.getClipBounds().toFloat().expanded (40.0f);
        for (const auto& e : flow.edges)
        {
            const auto& a = flow.boxes[(size_t) e.from];
            const auto& b = flow.boxes[(size_t) e.to];
            const auto p1 = juce::Point<float> (a.r.getRight(), a.r.getY() + 16.0f);
            const auto p2 = inletPoint (e);
            {
                // fuori dall'area da ridisegnare: si registra solo per il tocco
                auto area = juce::Rectangle<float> (p1, p2).expanded (70.0f, 10.0f);
                if (e.feedback || p2.x <= p1.x) area = area.getUnion (a.r).getUnion (b.r).expanded (70.0f, 40.0f);
                if (! area.intersects (clip))
                {
                    juce::Path p;
                    p.startNewSubPath (p1);
                    const float d = juce::jmax (30.0f, (p2.x - p1.x) * 0.5f);
                    if (! (e.feedback || p2.x <= p1.x)) p.cubicTo (p1.x + d, p1.y, p2.x - d, p2.y, p2.x, p2.y);
                    else { const float low = juce::jmax (a.r.getBottom(), b.r.getBottom()) + 26.0f; p.cubicTo (p1.x + 60.0f, low, p2.x - 60.0f, low, p2.x, p2.y); }
                    cableHits.push_back ({ p, e.var });
                    continue;
                }
            }
            juce::Path p;
            p.startNewSubPath (p1);
            const bool backwards = e.feedback || p2.x <= p1.x;
            if (! backwards)
            {
                const float d = juce::jmax (30.0f, (p2.x - p1.x) * 0.5f);
                p.cubicTo (p1.x + d, p1.y, p2.x - d, p2.y, p2.x, p2.y);
            }
            else
            {
                const float low = juce::jmax (a.r.getBottom(), b.r.getBottom()) + 26.0f;
                p.cubicTo (p1.x + 60.0f, low, p2.x - 60.0f, low, p2.x, p2.y);
            }
            const bool faded = dim (e.from) || dim (e.to);
            const bool hot = e.var == liveCables.hover || e.var == liveCables.selected;
            const auto col = (e.control ? kTextMuted : rateColour (e.rate)).brighter (hot ? 0.35f : 0.0f).withAlpha (faded ? 0.12f : 0.9f);
            float thick = e.control ? 1.2f : e.rate == 'a' ? 3.0f : 1.6f;
            if (liveCables.all && e.rate == 'a')            // Live: lo spessore segue il livello
                if (auto it = liveCables.level.find (e.var); it != liveCables.level.end())
                    thick = 1.5f + 6.0f * std::sqrt (juce::jlimit (0.0f, 1.0f, it->second));
            if (hot) thick += 1.2f;
            cableHits.push_back ({ p, e.var });
            if (liveCables.shows (e.var) && liveCables.text.count (e.var) && labelledVars.insert (e.var).second)
                liveLabels.push_back ({ p, e.var });
            g.setColour (col);
            if (backwards || e.control)           // "solo se": tratteggiato, come un'andata all'indietro
            {
                juce::Path dashed;
                const float dashes[] = { e.control ? 3.0f : 6.0f, 4.0f };
                juce::PathStrokeType (thick).createDashedStroke (dashed, p, dashes, 2);
                g.fillPath (dashed);
            }
            else
                g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.fillEllipse (p2.x - 3.5f, p2.y - 3.5f, 7.0f, 7.0f);
            if (e.label.isNotEmpty())
            {
                g.setFont (plainFont (10.5f));
                const float w = textWidth (plainFont (10.5f), e.label) + 2.0f;
                g.drawText (e.label, juce::Rectangle<float> (p2.x - 8.0f - w, p2.y - 14.0f, w, 12.0f), juce::Justification::centredRight, false);
            }
        }


        // dove vanno le uscite (a destra delle scatole d'uscita)
        for (int i = 0; i < (int) flow.boxes.size(); ++i)
        {
            const auto& b = flow.boxes[(size_t) i];
            if (b.outNote.isEmpty()) continue;
            const auto col = b.unused ? kTextMuted.withAlpha (dim (i) ? 0.2f : 0.8f)
                                      : (b.rate == 'a' ? rateColour ('a') : kAccent).withAlpha (dim (i) ? 0.2f : 0.9f);
            bool feeds = false;
            for (const auto& e : flow.edges) feeds = feeds || e.from == i;
            g.setColour (col);
            g.setFont (boldFont (12.0f));
            if (feeds)
            {
                // la scatola alimenta gia' un cavo verso destra: la nota va sotto
                g.drawText (u8 ("\xe2\x86\x92 ") + b.outNote, juce::Rectangle<float> (b.r.getX() + 14.0f, b.r.getBottom() + 3.0f, 300.0f, 16.0f),
                            juce::Justification::centredLeft, true);
                continue;
            }
            const float y0 = b.r.getY() + 16.0f;
            juce::Path arrowPath;
            arrowPath.addArrow ({ b.r.getRight(), y0, b.r.getRight() + 34.0f, y0 }, b.rate == 'a' ? 3.0f : 1.6f, 10.0f, 9.0f);
            g.fillPath (arrowPath);
            if (b.outVar.isNotEmpty())
            {
                // l'uscita di una scatola non collegata: si tocca come un cavo (valore, oscilloscopio, Listen)
                juce::Path stub;
                stub.startNewSubPath (b.r.getRight(), y0);
                stub.lineTo (b.r.getRight() + 34.0f, y0);
                cableHits.push_back ({ stub, b.outVar });
                if (liveCables.shows (b.outVar) && liveCables.text.count (b.outVar) && labelledVars.insert (b.outVar).second)
                    liveLabels.push_back ({ stub, b.outVar });
            }
            g.drawText (b.outNote, juce::Rectangle<float> (b.r.getRight() + 40.0f, y0 - 9.0f, 260.0f, 18.0f), juce::Justification::centredLeft, true);
        }

        // scatole
        for (int i = 0; i < (int) flow.boxes.size(); ++i)
        {
            const auto& b = flow.boxes[(size_t) i];
            if (! b.r.expanded (20.0f).intersects (clip)) continue;
            const float alpha = (dim (i) ? 0.25f : 1.0f) * (b.unused && i != selected ? 0.6f : 1.0f);
            // la striscia ha il colore del suo blocco, lo stesso del margine
            // del codice (Controls arancio...), non quello della categoria
            // dell'opcode (un phasor che fa da controllo non e' "verde")
            const auto st = b.node >= 0 ? stageOfNode.find (b.node) : stageOfNode.end();
            const auto role = st != stageOfNode.end() ? csoundFlow::stageColour (st->second) : csoundFlow::groupColour (b.group);
            g.setColour (kBoxFill.withMultipliedAlpha (alpha));
            g.fillRoundedRectangle (b.r, 8.0f);
            // striscia del ruolo
            g.setColour (role.withMultipliedAlpha (alpha));
            fillRoleStrip (g, b.r);
            g.setColour ((i == selected ? kAccent : rateColour (b.rate).withAlpha (b.spine ? 0.95f : 0.6f)).withMultipliedAlpha (alpha));
            if (b.unused && i != selected)
            {
                // tratteggiata: esiste, ma non arriva all'uscita
                juce::Path outline, dashed;
                outline.addRoundedRectangle (b.r, 8.0f);
                const float dashes[] = { 5.0f, 4.0f };
                juce::PathStrokeType (1.3f).createDashedStroke (dashed, outline, dashes, 2);
                g.fillPath (dashed);
            }
            else
                g.drawRoundedRectangle (b.r, 8.0f, i == selected ? 2.4f : (b.spine ? 1.8f : 1.2f));

            auto t = b.r.reduced (14.0f, 6.0f).withTrimmedLeft (0.0f);
            g.setColour (kText.withMultipliedAlpha (alpha));
            g.setFont (boldFont (13.5f));
            g.drawText (b.title, t.removeFromTop (18.0f), juce::Justification::centredLeft, true);
            if (b.role.isNotEmpty())
            {
                g.setColour (role.withMultipliedAlpha (alpha * 0.95f));
                g.setFont (plainFont (10.5f));
                g.drawText (b.role, t.removeFromTop (14.0f), juce::Justification::centredLeft, true);
            }
            if (b.channel.isNotEmpty())
                if (auto it = live.find (b.channel); it != live.end() && it->second.isNotEmpty())
                {
                    // valore vivo del canale, in alto a destra
                    const auto v = u8 ("\xe2\x97\x8f ") + it->second;
                    g.setFont (boldFont (11.5f));
                    const float w = textWidth (boldFont (11.5f), v) + 14.0f;
                    const juce::Rectangle<float> pill (b.r.getRight() - w - 6.0f, b.r.getY() - 9.0f, w, 18.0f);
                    g.setColour (kAccent.withMultipliedAlpha (alpha));
                    g.fillRoundedRectangle (pill, 9.0f);
                    g.setColour (juce::Colour (0xff0b1116));
                    g.drawText (v, pill, juce::Justification::centred, false);
                }
            for (const auto& prm : b.params)
            {
                auto row = t.removeFromTop (15.0f);
                g.setFont (boldFont (11.0f));
                g.setColour (kTextMuted.withMultipliedAlpha (alpha));
                const float lw = textWidth (boldFont (11.0f), prm.label) + 6.0f;
                g.drawText (prm.label, row.removeFromLeft (lw), juce::Justification::centredLeft, false);
                g.setFont (monoFont (11.0f));
                g.setColour (kText.withMultipliedAlpha (alpha * 0.9f));
                g.drawText (prm.text, row, juce::Justification::centredLeft, true);
            }
        }

        if (unusedTitleY > 0.0f)
        {
            g.setColour (kTextMuted);
            g.setFont (boldFont (11.0f));
            g.drawText ("NOT CONNECTED  " + u8 ("\xc2\xb7") + "  computed, but nothing takes it to an output", juce::Rectangle<float> (pad, unusedTitleY, 600.0f, 16.0f), juce::Justification::centredLeft, false);
        }

        // schede degli eventi
        if (! flow.events.empty())
        {
            g.setColour (kTextMuted);
            g.setFont (boldFont (11.0f));
            g.drawText ("EVENTS  " + u8 ("\xc2\xb7") + "  notes started by this block", juce::Rectangle<float> (pad, eventsTitleY, 600.0f, 16.0f), juce::Justification::centredLeft, false);
        }
        for (const auto& c : flow.events)
        {
            const auto purple = csoundFlow::groupColour ("Instrument Control");
            const bool sel = selectedEvent == c.node;
            g.setColour (kBoxFill);
            g.fillRoundedRectangle (c.r, 8.0f);
            g.setColour (purple);
            fillRoleStrip (g, c.r);
            g.setColour (sel ? kAccent : purple.withAlpha (0.6f));
            g.drawRoundedRectangle (c.r, 8.0f, sel ? 2.4f : 1.2f);

            auto t = c.r.reduced (14.0f, 8.0f);
            for (const auto& fr : c.frames)
            {
                const bool loop = fr.startsWith ("while") || fr.startsWith ("until");
                g.setColour (juce::Colour (0xfff2c14e));
                g.setFont (boldFont (11.5f));
                g.drawText ((loop ? u8 ("\xe2\x86\xbb  repeated ") : u8 ("?  only ")) + fr, t.removeFromTop (17.0f), juce::Justification::centredLeft, true);
            }
            g.setColour (kText);
            g.setFont (boldFont (13.5f));
            g.drawText (c.opcode + u8 ("  \xe2\x86\x92  instr ") + c.target, t.removeFromTop (20.0f), juce::Justification::centredLeft, true);
            for (const auto& row : c.rows)
            {
                auto line = t.removeFromTop (16.0f);
                g.setFont (boldFont (11.0f));
                g.setColour (kTextMuted);
                g.drawText (row.field, line.removeFromLeft (46.0f), juce::Justification::centredLeft, false);
                g.setFont (monoFont (11.0f));
                g.setColour (kText);
                g.drawText (row.text, line.removeFromLeft (300.0f), juce::Justification::centredLeft, true);
                if (row.usedBy.isNotEmpty())
                {
                    g.setFont (plainFont (11.0f));
                    g.setColour (csoundGraphUI::rateColour ('k').withAlpha (0.85f));
                    g.drawText (u8 ("\xe2\x86\x92 ") + row.usedBy, line, juce::Justification::centredLeft, true);
                }
            }
        }

        if (flow.boxes.empty() && flow.events.empty())
        {
            g.setColour (kTextMuted);
            g.setFont (plainFont (14.0f));
            g.drawText ("Nothing reaches an output in this block (see Explain).", juce::Rectangle<float> (0.0f, 0.0f, contentW, contentH).withTrimmedTop (y), juce::Justification::centredTop, true);
        }

        // valori dal vivo: per ultimi, sopra tutto, senza sovrapporsi
        if (! liveLabels.empty())
        {
            std::vector<LiveLabel> ll;
            for (const auto& [path, var] : liveLabels)
                ll.push_back ({ path, var, liveCables.text[var], rateColour (var.startsWithChar ('a') ? 'a' : 'k'),
                                var == liveCables.hover || var == liveCables.selected });
            std::vector<juce::Rectangle<float>> obstacles;
            for (const auto& b : flow.boxes)  obstacles.push_back (b.r);
            for (const auto& c : flow.events) obstacles.push_back (c.r);
            drawLiveLabels (g, std::move (ll), obstacles);
        }
    }

    std::function<void (const juce::String& var, juce::Point<float> where)> onCableTapped;
    std::function<void (const juce::String& var)> onCableHovered;
    void setLiveCables (const csoundGraphUI::LiveCables& lc) { const auto h = liveCables.hover; liveCables = lc; liveCables.hover = h; repaint(); }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
            setZoom (zoom * std::exp (0.9f * (w.deltaY != 0.0f ? w.deltaY : w.deltaX)), e.position);
        else
            csoundGraphUI::forwardWheel (*this, e, w);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (overview)
        {
            int h = -1;
            for (int i = 0; i < (int) full.stages.size(); ++i) if (full.stages[(size_t) i].r.contains (e.position / zoom)) h = i;
            if (h != hoverStage) { hoverStage = h; setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); repaint(); }
            return;
        }
        const auto v = csoundGraphUI::cableAt (cableHits, e.position / zoom);
        if (v != liveCables.hover) { liveCables.hover = v; repaint(); if (onCableHovered) onCableHovered (v); }
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
            if (pinching)
            {
                if (touches.empty()) endPinch();
                return;                 // le dita del pinch non sono tocchi
            }
        }
        if (e.mouseWasDraggedSinceMouseDown()) return;
        const auto pos = e.position / zoom;
        if (overview)
        {
            for (int i = 0; i < (int) full.stages.size(); ++i)
                if (full.stages[(size_t) i].r.contains (pos)) { if (onStageTapped) onStageTapped (i); return; }
            if (e.getNumberOfClicks() >= 2) toggleFitZoom (e.position);
            return;
        }
        for (const auto& c : flow.events)
            if (c.r.contains (pos))
            {
                selected = -1;
                const bool twice = e.getNumberOfClicks() >= 2;
                selectedEvent = twice || selectedEvent != c.node ? c.node : -1;
                repaint();
                if (twice) { if (onNodeDoubleTapped) onNodeDoubleTapped (c.node); }
                else if (selectedEvent >= 0 && onNodeTapped) onNodeTapped (c.node);
                return;
            }
        selectedEvent = -1;
        for (int i = (int) flow.boxes.size(); --i >= 0;)
            if (flow.boxes[(size_t) i].r.contains (pos))
            {
                const bool twice = e.getNumberOfClicks() >= 2;
                selected = twice || selected != i ? i : -1;     // il secondo tocco del doppio non deseleziona
                repaint();
                if (const int nd = flow.boxes[(size_t) i].node; nd <= -2)      // porta verso un altro blocco
                {
                    if (onStageTapped) onStageTapped (-2 - nd);
                    return;
                }
                if (twice) { if (onNodeDoubleTapped) onNodeDoubleTapped (flow.boxes[(size_t) i].node); }
                else if (selected >= 0 && onNodeTapped) onNodeTapped (flow.boxes[(size_t) i].node);
                return;
            }
        // doppio tocco fuori dalle scatole (anche su un cavo): zoom
        if (e.getNumberOfClicks() >= 2) { toggleFitZoom (e.position); return; }
        // un cavo: il suo valore dal vivo
        if (const auto v = csoundGraphUI::cableAt (cableHits, pos); v.isNotEmpty())
        {
            if (onCableTapped) onCableTapped (v, e.position);
            return;
        }
        selected = -1;
        repaint();
        if (onEmptyTapped) onEmptyTapped();
    }

private:
    // striscia del ruolo: dentro il bordo arrotondato della scatola (ritagliata
    // sulla stessa forma), cosi' non sborda dagli angoli
    static void fillRoleStrip (juce::Graphics& g, juce::Rectangle<float> box)
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path shape;
        shape.addRoundedRectangle (box.reduced (1.0f), 7.0f);
        g.reduceClipRegion (shape);
        g.fillRect (box.withWidth (6.0f));
    }

    csoundFlow::Flow full, flow;     // tutto lo strumento; quello disegnato (tutto, o un blocco aperto)
    std::map<int, int> stageOfNode;  // nodo -> tipo del suo blocco (colore della striscia)
    int selected = -1, selectedEvent = -1;
    csoundGraphUI::LiveCables liveCables;
    std::vector<std::pair<juce::Path, juce::String>> cableHits;
    std::map<juce::String, juce::String> live;
    float eventsTitleY = 0.0f;
    float zoom = 1.0f, contentW = 0.0f, contentH = 0.0f;
    bool overview = false;
    int focusStage = -1, hoverStage = -1;
    float ovW = 0.0f, ovH = 0.0f;

    //--------------------------------------------------------------------------
    // Vista d'insieme: un riquadro per blocco funzionale, in colonne secondo il
    // flusso; frecce fra i blocchi con i nomi dei segnali.
    static constexpr float stageW = 250.0f, stageGapX = 120.0f, stageGapY = 28.0f;
    float stageHeight (const csoundFlow::Stage& st) const { return 92.0f + (st.comment.isNotEmpty() ? 18.0f : 0.0f); }
    void layoutOverview()
    {
        std::map<int, std::vector<int>> cols;
        for (int i = 0; i < (int) full.stages.size(); ++i) cols[full.stages[(size_t) i].column].push_back (i);
        float tallest = 0.0f;
        for (auto& [c, list] : cols)
        {
            float h = 0.0f;
            for (int i : list) h += stageHeight (full.stages[(size_t) i]) + stageGapY;
            tallest = juce::jmax (tallest, h);
        }
        const float top = pad * 0.5f + 64.0f;
        float x = pad, right = pad;
        for (auto& [c, list] : cols)
        {
            float h = 0.0f;
            for (int i : list) h += stageHeight (full.stages[(size_t) i]) + stageGapY;
            float y = top + (tallest - h) * 0.5f;
            for (int i : list)
            {
                auto& st = full.stages[(size_t) i];
                st.r = { x, y, stageW, stageHeight (st) };
                y += st.r.getHeight() + stageGapY;
            }
            right = x + stageW;
            x += stageW + stageGapX;
        }
        ovW = juce::jmax (right + pad, pad * 2.0f + 520.0f);
        ovH = top + tallest + pad;
    }

    void paintOverview (juce::Graphics& g)
    {
        using namespace csoundGraphUI;
        cableHits.clear();
        g.setColour (kText);
        g.setFont (boldFont (13.5f));
        g.drawText (full.summary.isEmpty() ? juce::String() : full.summary[0], juce::Rectangle<float> (pad, pad * 0.5f, ovW - 2.0f * pad, 18.0f), juce::Justification::centredLeft, true);
        g.setColour (kTextMuted);
        g.setFont (plainFont (12.5f));
        g.drawText ("The instrument in blocks. Tap a block to see its boxes in the Flow.", juce::Rectangle<float> (pad, pad * 0.5f + 22.0f, ovW - 2.0f * pad, 18.0f), juce::Justification::centredLeft, true);

        std::vector<LiveLabel> labels;
        for (const auto& se : full.stageEdges)
        {
            const auto& a = full.stages[(size_t) se.from].r;
            const auto& b = full.stages[(size_t) se.to].r;
            juce::Path p;
            juce::Point<float> s1 (a.getRight(), a.getCentreY()), s2 (b.getX() - 2.0f, b.getCentreY());
            if (b.getX() > a.getRight())
            {
                const float d = juce::jmax (40.0f, (s2.x - s1.x) * 0.5f);
                p.startNewSubPath (s1);
                p.cubicTo (s1.translated (d, 0.0f), s2.translated (-d, 0.0f), s2);
            }
            else
            {
                // stessa colonna o all'indietro: sotto i blocchi
                const float low = juce::jmax (a.getBottom(), b.getBottom()) + 24.0f;
                s1 = { a.getCentreX(), a.getBottom() }; s2 = { b.getCentreX(), b.getBottom() + 2.0f };
                p.startNewSubPath (s1);
                p.cubicTo ({ s1.x, low }, { s2.x, low }, s2);
            }
            const bool hot = hoverStage >= 0 && (se.from == hoverStage || se.to == hoverStage);
            const auto col = rateColour (se.rate).withAlpha (hoverStage >= 0 && ! hot ? 0.3f : 0.9f);
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (se.rate == 'a' ? 3.0f : 1.6f));
            const float len = p.getLength();
            if (len > 12.0f)
            {
                const auto tip = p.getPointAlongPath (len), back = p.getPointAlongPath (len - 10.0f);
                const auto dir = (tip - back) / juce::jmax (0.001f, tip.getDistanceFrom (back));
                const juce::Point<float> nrm (-dir.y, dir.x);
                juce::Path ah;
                ah.addTriangle (tip, tip - dir * 10.0f + nrm * 5.0f, tip - dir * 10.0f - nrm * 5.0f);
                g.fillPath (ah);
            }
            labels.push_back ({ p, se.label, {}, rateColour (se.rate), hot });
        }

        for (int i = 0; i < (int) full.stages.size(); ++i)
        {
            const auto& st = full.stages[(size_t) i];
            const auto c = csoundFlow::stageColour (st.kind);
            g.setColour (kBoxFill);
            g.fillRoundedRectangle (st.r, 10.0f);
            {
                juce::Graphics::ScopedSaveState save (g);
                juce::Path shape;
                shape.addRoundedRectangle (st.r.reduced (1.0f), 9.0f);
                g.reduceClipRegion (shape);
                g.setColour (c);
                g.fillRect (st.r.withHeight (5.0f));
            }
            g.setColour (i == hoverStage ? c : c.withAlpha (0.55f));
            g.drawRoundedRectangle (st.r, 10.0f, i == hoverStage ? 2.2f : 1.4f);
            auto t = st.r.reduced (14.0f, 10.0f).withTrimmedTop (4.0f);
            g.setColour (kText);
            g.setFont (boldFont (15.5f));
            g.drawText (st.title, t.removeFromTop (22.0f), juce::Justification::centredLeft, true);
            if (st.comment.isNotEmpty())
            {
                g.setColour (juce::Colour (0xff6a9955));
                g.setFont (plainFont (12.0f));
                g.drawText (u8 ("\xe2\x80\x9c") + st.comment + u8 ("\xe2\x80\x9d"), t.removeFromTop (18.0f), juce::Justification::centredLeft, true);
            }
            g.setColour (kTextMuted);
            g.setFont (plainFont (12.0f));
            g.drawFittedText (st.sentence, t.removeFromTop (32.0f).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
            g.setColour (c.withAlpha (0.9f));
            g.setFont (boldFont (11.0f));
            g.drawText (juce::String ((int) st.boxes.size()) + ((int) st.boxes.size() == 1 ? " box" : " boxes") + u8 ("  \xe2\x80\xba"),
                        t, juce::Justification::bottomLeft, false);
        }
        std::vector<juce::Rectangle<float>> obstacles;
        for (const auto& st : full.stages) obstacles.push_back (st.r);
        drawLiveLabels (g, labels, obstacles, true);
    }
    std::map<int, juce::Point<float>> touches;   // dita sullo schema (posizioni sullo schermo)
    float pinchStartDistance = 0.0f, pinchStartZoom = 1.0f;
    bool pinching = false;
    juce::Viewport::ScrollOnDragMode savedDragMode = juce::Viewport::ScrollOnDragMode::all;

    // almeno grande quanto la vista: anche lo spazio intorno a uno schema
    // rimpicciolito riceve i tocchi (doppio tocco per tornare al 100%)
    void applySize()
    {
        int w = (int) std::ceil ((overview ? ovW : contentW) * zoom), h = (int) std::ceil ((overview ? ovH : contentH) * zoom);
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        {
            w = juce::jmax (w, vp->getMaximumVisibleWidth());
            h = juce::jmax (h, vp->getMaximumVisibleHeight());
        }
        setSize (juce::jmax (1, w), juce::jmax (1, h));
    }

    // doppio tocco: tutto lo schema nella vista; di nuovo: 100%.
    // Se lo schema ci sta gia' al 100%: ingrandisce (200%) e poi torna al 100%.
    void toggleFitZoom (juce::Point<float> at)
    {
        auto* vp = findParentComponentOfClass<juce::Viewport>();
        const float cw = overview ? ovW : contentW, ch = overview ? ovH : contentH;
        if (vp == nullptr || cw <= 0.0f || ch <= 0.0f) return;
        const float fit = juce::jmin (1.0f, (float) vp->getMaximumVisibleWidth() / cw, (float) vp->getMaximumVisibleHeight() / ch);
        float target;
        const float fitZoom = juce::jmax (0.2f, fit);          // come il limite di setZoom
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
                vp->setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);   // due dita: zoom, non scorrimento
            }
        pinching = true;
    }
    void endPinch()
    {
        pinching = false;
        pinchStartDistance = 0.0f;
        if (auto* vp = findParentComponentOfClass<juce::Viewport>()) vp->setScrollOnDragMode (savedDragMode);
    }
    float unusedTitleY = 0.0f;     // titolo della fascia "non collegati" (0 = nessuno)
    juce::Rectangle<float> spineBand;
    float summaryH = 0.0f;
    static constexpr float pad = 40.0f, boxW = 210.0f, gapX = 90.0f, gapY = 34.0f;

    // tutto cio' da cui arriva il segnale della scatola e tutto cio' a cui va
    std::set<int> cone (int box) const
    {
        std::set<int> s { box };
        for (int dir = 0; dir < 2; ++dir)
        {
            std::vector<int> st { box };
            while (! st.empty())
            {
                const int c = st.back(); st.pop_back();
                for (const auto& e : flow.edges)
                {
                    const int next = dir == 0 ? (e.to == c ? e.from : -1) : (e.from == c ? e.to : -1);
                    if (next >= 0 && s.insert (next).second) st.push_back (next);
                }
            }
        }
        return s;
    }

    // ingressi sul lato sinistro, in ordine di argomento (calcolati in layout:
    // con migliaia di cavi un conteggio per cavo a ogni repaint costa troppo)
    std::vector<float> inletY;
    void computeInlets()
    {
        inletY.assign (flow.edges.size(), 0.0f);
        std::map<int, std::vector<int>> byBox;
        for (int k = 0; k < (int) flow.edges.size(); ++k) byBox[flow.edges[(size_t) k].to].push_back (k);
        for (auto& [box, list] : byBox)
        {
            std::stable_sort (list.begin(), list.end(), [this] (int x, int y) { return flow.edges[(size_t) x].toIn < flow.edges[(size_t) y].toIn; });
            const auto& b = flow.boxes[(size_t) box];
            const float top = b.r.getY() + 14.0f, bottom = b.r.getBottom() - 10.0f;
            const int count = (int) list.size();
            for (int i = 0; i < count; ++i)
                inletY[(size_t) list[(size_t) i]] = count <= 1 ? b.r.getY() + 16.0f : top + (bottom - top) * (float) i / (float) (count - 1);
        }
    }
    juce::Point<float> inletPoint (const csoundFlow::Edge& e) const
    {
        const auto k = (size_t) (&e - flow.edges.data());
        return { flow.boxes[(size_t) e.to].r.getX(), k < inletY.size() ? inletY[k] : flow.boxes[(size_t) e.to].r.getY() + 16.0f };
    }

    void layout()
    {
        using namespace csoundGraphUI;
        summaryH = flow.summary.isEmpty() ? 0.0f : 22.0f + 18.0f * (float) (flow.summary.size() - 1) + 24.0f;

        int minRow = 0, maxRow = 0, maxDepth = 0;
        // ingressi per scatola: ogni ingresso ha la sua etichetta (14 px), la
        // scatola si allunga quanto serve perche' le etichette non si sovrappongano
        std::map<int, int> inlets;
        for (const auto& e : flow.edges) ++inlets[e.to];
        for (int i = 0; i < (int) flow.boxes.size(); ++i)
        {
            auto& b = flow.boxes[(size_t) i];
            minRow = juce::jmin (minRow, b.row); maxRow = juce::jmax (maxRow, b.row);
            maxDepth = juce::jmax (maxDepth, b.depth);
            const float h = 30.0f + (b.role.isNotEmpty() ? 14.0f : 0.0f) + 15.0f * (float) b.params.size();
            const int n = inlets.count (i) ? inlets[i] : 0;
            const float forInlets = n > 1 ? 24.0f + 15.0f * (float) (n - 1) : 0.0f;
            b.r.setSize (boxW, juce::jmax (40.0f, h, forInlets));
        }
        std::map<int, float> rowH;
        for (const auto& b : flow.boxes) rowH[b.row] = juce::jmax (rowH[b.row], b.r.getHeight());
        std::map<int, float> rowY;
        float y = pad * 0.5f + summaryH;
        int firstUnusedRow = INT_MAX;
        for (const auto& b : flow.boxes) if (b.unused) firstUnusedRow = juce::jmin (firstUnusedRow, b.row);
        unusedTitleY = 0.0f;
        for (int r = minRow; r <= maxRow; ++r)
        {
            if (r == firstUnusedRow) { y += 16.0f; unusedTitleY = y; y += 26.0f; }
            rowY[r] = y;
            if (rowH.count (r)) y += rowH[r] + gapY;      // righe vuote: nessuno spazio
        }
        for (auto& b : flow.boxes)
            b.r.setPosition (pad + (float) b.depth * (boxW + gapX), rowY[b.row]);

        spineBand = {};
        for (const auto& b : flow.boxes)
            if (b.spine) spineBand = spineBand.isEmpty() ? b.r : spineBand.getUnion (b.r);
        if (! spineBand.isEmpty()) spineBand = spineBand.expanded (16.0f, 12.0f);

        // schede degli eventi, sotto lo schema
        if (! flow.events.empty())
        {
            if (flow.boxes.empty()) y = pad * 0.5f + summaryH;
            eventsTitleY = y;
            y += 24.0f;
            for (auto& c : flow.events)
            {
                const float h = 16.0f + 17.0f * (float) c.frames.size() + 20.0f + 16.0f * (float) c.rows.size();
                c.r = { pad, y, 640.0f, h };
                y += h + 14.0f;
            }
            y += pad * 0.5f;
        }

        computeInlets();
        float right = pad + (float) (maxDepth + 1) * (boxW + gapX) + 220.0f;   // + nota d'uscita
        if (! flow.events.empty()) right = juce::jmax (right, pad + 680.0f);
        for (int i = 0; i < flow.summary.size(); ++i)
            right = juce::jmax (right, pad * 2.0f + textWidth (i == 0 ? boldFont (13.5f) : plainFont (12.5f), flow.summary[i]));
        contentW = right + 20.0f;
        contentH = y + pad;
        applySize();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundGraphFlowCanvas)
};
