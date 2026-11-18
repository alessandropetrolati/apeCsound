#pragma once
/*
    CsoundOrchestraMap.h - la mappa dell'orchestra: chi fa partire cosa.

    Alessandro Petrolati - www.apesoft.it

    Un csd si capisce prima da fuori: da dove arrivano le note (score, MIDI,
    altri strumenti con schedule / event), quali dati passano fra gli
    strumenti (variabili globali, canali, tabelle create nel codice
    globale) e chi arriva all'uscita audio. Qui:

      - a sinistra le sorgenti: Score, MIDI, i controlli (slider del
        pannello Parameters o della DAW), il codice globale;
      - al centro gli strumenti, in colonne: prima quelli fatti partire da
        fuori, poi quelli fatti partire da loro, e cosi' via;
      - a destra l'uscita audio.

    Frecce bianche = "fa partire note di"; colorate = dati (colore del tempo:
    blu audio, corallo controllo, grigio i-time / tabelle). Mentre Csound
    suona, ogni strumento mostra quante note sta suonando. Un tocco su uno
    strumento lo apre.
*/
#include <JuceHeader.h>
#include <map>
#include <set>
#include <vector>
#include "CsoundGraphModel.h"
#include "CsoundGraphStyle.h"
#include "CsoundExplain.h"

class CsoundOrchestraMapCanvas final : public juce::Component
{
public:
    CsoundOrchestraMapCanvas() = default;
    std::function<void (int block)> onBlockTapped;

    void setOrchestra (const csoundGraph::Orchestra& orc, const csoundExplain::Explanation& ex)
    {
        build (orc, ex);
        layout();
        repaint();
    }

    /** Note attive per blocco (dal motore); vuoto = Csound fermo. */
    void setActiveCounts (const std::map<int, int>& counts)
    {
        if (counts == active) return;
        active = counts;
        repaint();
    }

    /** I nomi degli strumenti (per chiedere al motore le note attive). */
    std::vector<std::pair<int, juce::String>> instrumentNames() const
    {
        std::vector<std::pair<int, juce::String>> out;
        for (const auto& b : boxes) if (b.kind == Instr) out.push_back ({ b.block, b.name });
        return out;
    }

    int instrumentCount() const { int n = 0; for (const auto& b : boxes) n += b.kind == Instr ? 1 : 0; return n; }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { csoundGraphUI::forwardWheel (*this, e, w); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        int h = -1;
        for (int i = 0; i < (int) boxes.size(); ++i)
            if (boxes[(size_t) i].block >= 0 && boxes[(size_t) i].area.contains (e.position)) h = i;
        if (h != hover) { hover = h; setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { if (hover >= 0) { hover = -1; repaint(); } }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown()) return;
        for (const auto& b : boxes)
            if (b.block >= 0 && b.area.contains (e.position))
            {
                if (onBlockTapped) onBlockTapped (b.block);
                return;
            }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace csoundGraphUI;
        g.fillAll (kBg);

        // titoli delle colonne
        g.setFont (boldFont (10.5f));
        g.setColour (kTextMuted);
        for (const auto& [x, t] : columnTitles)
            g.drawText (t.toUpperCase(), juce::Rectangle<float> (x, 14.0f, 260.0f, 16.0f), juce::Justification::centredLeft, false);

        // frecce
        std::vector<LiveLabel> labels;
        for (const auto& e : edges)
        {
            const auto p = edgePath (e);
            const bool hot = hover >= 0 && (e.from == hover || e.to == hover);
            const auto col = e.trigger ? kText.withAlpha (0.85f) : e.colour;
            g.setColour (hover >= 0 && ! hot ? col.withAlpha (0.25f) : col);
            g.strokePath (p, juce::PathStrokeType (e.trigger ? 1.8f : 1.3f));
            arrowHead (g, p);
            if (e.label.isNotEmpty())
                labels.push_back ({ p, e.label, {}, e.trigger ? kTextMuted : e.colour, hot });
        }

        // scatole
        for (int i = 0; i < (int) boxes.size(); ++i)
        {
            const auto& b = boxes[(size_t) i];
            const int playing = b.block >= 0 && active.count (b.block) ? active.at (b.block) : 0;
            const bool src = b.kind != Instr;
            g.setColour (src ? kBar : kBoxFill);
            g.fillRoundedRectangle (b.area, 8.0f);
            g.setColour (playing > 0 ? kAccent : (i == hover ? kText.withAlpha (0.7f) : kBorder));
            g.drawRoundedRectangle (b.area, 8.0f, playing > 0 || i == hover ? 1.8f : 1.0f);

            auto r = b.area.reduced (12.0f, 8.0f);
            g.setFont (boldFont (src ? 13.0f : 14.0f));
            g.setColour (kText);
            g.drawText (b.title, r.removeFromTop (18.0f), juce::Justification::centredLeft, true);
            if (b.sub.isNotEmpty())
            {
                g.setFont (plainFont (11.5f));
                g.setColour (kTextMuted);
                g.drawFittedText (b.sub, r.removeFromTop (src ? 16.0f : 30.0f).toNearestInt(), juce::Justification::topLeft, src ? 1 : 2, 0.9f);
            }
            if (b.sub2.isNotEmpty())
            {
                g.setFont (plainFont (11.0f));
                g.setColour (kTextMuted.withAlpha (0.8f));
                g.drawText (b.sub2, r.removeFromTop (15.0f), juce::Justification::centredLeft, true);
            }
            if (playing > 0)
            {
                const auto t = juce::String (playing) + (playing == 1 ? " note" : " notes");
                const auto f = boldFont (10.5f);
                const float w = textWidth (f, t) + 22.0f;
                const juce::Rectangle<float> pill (b.area.getRight() - w - 8.0f, b.area.getY() + 7.0f, w, 17.0f);
                g.setColour (kAccent.withAlpha (0.22f));
                g.fillRoundedRectangle (pill, 8.5f);
                g.setColour (kAccent);
                g.fillEllipse (pill.getX() + 7.0f, pill.getCentreY() - 3.0f, 6.0f, 6.0f);
                g.setFont (f);
                g.drawText (t, pill.withTrimmedLeft (16.0f), juce::Justification::centredLeft, false);
            }
        }

        std::vector<juce::Rectangle<float>> obstacles;
        for (const auto& b : boxes) obstacles.push_back (b.area);
        drawLiveLabels (g, labels, obstacles);
    }

private:
    enum Kind { Score, Midi, Controls, Global, Instr, Output };
    struct Box
    {
        Kind kind = Instr;
        int block = -1;                 // blocco dell'orchestra (strumento, codice globale), -1 = sorgente / uscita
        juce::String name, title, sub, sub2;
        int column = 0;
        juce::Rectangle<float> area;
    };
    struct Edge { int from = 0, to = 0; juce::String label; juce::Colour colour; bool trigger = false; float fromY = 0.0f, toY = 0.0f; int names = 1; int lane = -1; };

    // salta una colonna o torna indietro: passa sotto le scatole
    bool usesLane (const Edge& e) const
    {
        const int a = boxes[(size_t) e.from].column, b = boxes[(size_t) e.to].column;
        return b - a > 1 || b < a;
    }

    std::vector<Box> boxes;
    std::vector<Edge> edges;
    std::vector<std::pair<float, juce::String>> columnTitles;
    std::map<int, int> active;
    int hover = -1;

    static constexpr float sourceW = 140.0f, instrW = 210.0f, gapX = 104.0f, gapY = 26.0f;
    float laneTop = 0.0f;              // sotto le scatole: le frecce lunghe passano di qui

    int boxForBlock (int block) const
    {
        for (int i = 0; i < (int) boxes.size(); ++i) if (boxes[(size_t) i].block == block) return i;
        return -1;
    }
    int boxOfKind (Kind k) const
    {
        for (int i = 0; i < (int) boxes.size(); ++i) if (boxes[(size_t) i].kind == k) return i;
        return -1;
    }
    int addBox (Kind k, int block, const juce::String& title, const juce::String& sub = {})
    {
        Box b; b.kind = k; b.block = block; b.title = title; b.sub = sub;
        boxes.push_back (b);
        return (int) boxes.size() - 1;
    }
    void addEdge (int from, int to, const juce::String& label, juce::Colour c, bool trigger)
    {
        if (from < 0 || to < 0) return;
        for (auto& e : edges)
            if (e.from == from && e.to == to && e.trigger == trigger)
            {
                // stesse due scatole: un'unica freccia con i nomi insieme
                if (label.isNotEmpty() && ! e.label.contains (label))
                {
                    ++e.names;
                    e.label = e.label.length() > 18 ? (e.label.endsWith ("...") ? e.label : e.label + ", ...") : e.label + ", " + label;
                }
                return;
            }
        edges.push_back ({ from, to, label, c, trigger });
    }

    static bool isMidiOpcode (const std::string& n)
    {
        static const std::set<std::string> m { "cpsmidi", "cpsmidib", "notnum", "veloc", "ampmidi", "ampmidid", "pchmidi", "pchmidib", "octmidi", "octmidib",
                                               "midinoteoncps", "midinoteonkey", "midinoteonoct", "midinoteonpch", "midichn", "midictrl", "mxadsr", "aftouch", "pchbend" };
        return m.count (n) > 0;
    }

    void build (const csoundGraph::Orchestra& orc, const csoundExplain::Explanation& ex)
    {
        using namespace csoundGraphUI;
        using csoundGraph::Graph;
        boxes.clear();
        edges.clear();
        hover = -1;

        // sorgenti
        const bool anyScore = ! orc.scoreNotes.empty();
        bool anyMidi = false;
        std::set<std::string> massigned;     // massign: canale -> strumento
        for (const auto& n : orc.blocks.empty() ? std::vector<csoundGraph::Node> {} : orc.blocks[0].nodes)
            if (n.name == "massign" && n.ins.size() >= 2) { anyMidi = true; massigned.insert (n.ins[1].text); }
        for (size_t b = 1; b < orc.blocks.size(); ++b)
            for (const auto& n : orc.blocks[b].nodes) anyMidi = anyMidi || isMidiOpcode (n.name);

        if (anyScore) addBox (Score, -1, "Score", "<CsScore>: timed notes");
        if (anyMidi)  addBox (Midi, -1, "MIDI", "keyboard, controllers");
        if (! orc.hostChannels.empty()) addBox (Controls, -1, "Controls", "sliders: Parameters / DAW");
        const bool globalCode = ! orc.blocks.empty() && ! orc.blocks[0].nodes.empty();
        if (globalCode) addBox (Global, 0, "Global code", "runs once, before any note");

        // strumenti
        std::map<int, juce::String> summary;
        for (const auto& s : ex.summaries) if (! s.text.isEmpty()) summary[s.line] = s.text[0];
        for (size_t b = 1; b < orc.blocks.size(); ++b)
        {
            const auto& g = orc.blocks[b];
            if (g.kind != Graph::Instr) continue;
            const int i = addBox (Instr, (int) b, js (g.title()));
            auto& bx = boxes[(size_t) i];
            bx.name = g.names.empty() ? juce::String() : js (g.names[0]);
            auto it = summary.find (g.lineBegin);
            bx.sub = it != summary.end() ? it->second : juce::String ("No audio: control, events or messages.");
            juce::StringArray facts;
            for (const auto& nm : g.names)
                if (auto sc = orc.scoreNotes.find (nm); sc != orc.scoreNotes.end())
                    facts.add (juce::String (sc->second) + (sc->second == 1 ? " score note" : " score notes"));
            juce::String pf;
            for (int p : g.pfields) if (p > 3) pf << "p" << p << " ";
            if (pf.isNotEmpty()) facts.add ("uses " + pf.trim().replace (" ", ", "));
            bx.sub2 = facts.joinIntoString (u8 ("  \xc2\xb7  "));
        }
        bool anyOut = false;
        for (size_t b = 1; b < orc.blocks.size(); ++b)
            if (orc.blocks[b].kind == Graph::Instr)
                for (const auto& n : orc.blocks[b].nodes) anyOut = anyOut || n.audioOut;
        if (anyOut) addBox (Output, -1, "Audio output", "speakers / DAW");

        // frecce: sorgenti -> strumenti
        for (size_t b = 1; b < orc.blocks.size(); ++b)
        {
            const auto& g = orc.blocks[b];
            if (g.kind != Graph::Instr) continue;
            const int to = boxForBlock ((int) b);
            for (const auto& nm : g.names)
                if (auto sc = orc.scoreNotes.find (nm); sc != orc.scoreNotes.end())
                    addEdge (boxOfKind (Score), to, juce::String (sc->second) + (sc->second == 1 ? " note" : " notes"), kText, true);
            bool midi = false;
            for (const auto& n : g.nodes) midi = midi || isMidiOpcode (n.name);
            for (const auto& nm : g.names) midi = midi || massigned.count (nm) > 0;
            if (midi) addEdge (boxOfKind (Midi), to, "notes", kText, true);
            for (const auto& n : g.nodes)
                if (! n.channel.empty() && ! n.channelWrite)
                    for (const auto& hc : orc.hostChannels)
                        if (hc == n.channel) addEdge (boxOfKind (Controls), to, "\"" + js (hc) + "\"", rateColour ('k'), false);
            for (auto& e : edges)       // tanti controlli: il numero, non l'elenco
                if (e.to == to && e.from == boxOfKind (Controls) && e.names > 1)
                    e.label = juce::String (e.names) + " controls";
            bool out = false;
            for (const auto& n : g.nodes) out = out || n.audioOut;
            if (out) addEdge (to, boxOfKind (Output), {}, rateColour ('a'), false);
        }
        // fra blocchi: eventi, bus globali, canali
        for (const auto& l : orc.links)
        {
            const int from = boxForBlock (l.from), to = boxForBlock (l.to);
            if (l.kind == "evento") addEdge (from, to, js (l.name) + (from == to ? juce::String (" itself") : juce::String()), kText, true);
            else addEdge (from, to, js (l.name), rateColour (l.rate == '?' ? 'k' : l.rate), false);
        }

        // colonne: profondita' degli inneschi (chi parte da fuori = 1)
        std::map<int, int> depth;
        for (const auto& b : boxes) if (b.kind == Instr) depth[b.block] = 1;
        for (int pass = 0; pass < 8; ++pass)
            for (const auto& l : orc.links)
                if (l.kind == "evento" && l.from > 0 && l.to > 0 && l.from != l.to && depth.count (l.from) && depth.count (l.to))
                    depth[l.to] = juce::jmin (8, juce::jmax (depth[l.to], depth[l.from] + 1));
        int maxDepth = 1;
        for (auto& b : boxes)
        {
            b.column = b.kind == Instr ? depth[b.block] : b.kind == Output ? 99 : 0;
            if (b.kind == Instr) maxDepth = juce::jmax (maxDepth, b.column);
        }
        for (auto& b : boxes) if (b.kind == Output) b.column = maxDepth + 1;
    }

    void layout()
    {
        columnTitles.clear();
        std::map<int, std::vector<int>> cols;
        for (int i = 0; i < (int) boxes.size(); ++i) cols[boxes[(size_t) i].column].push_back (i);

        auto boxH = [] (const Box& b) { return b.kind == Instr ? 82.0f : 50.0f; };
        float tallest = 0.0f;
        for (const auto& [c, list] : cols)
        {
            float h = 0.0f;
            for (int i : list) h += boxH (boxes[(size_t) i]) + gapY;
            tallest = juce::jmax (tallest, h);
        }

        float x = 24.0f;
        const int last = cols.empty() ? 0 : cols.rbegin()->first;
        for (const auto& [c, list] : cols)
        {
            const float w = (c == 0 || (c == last && boxes[(size_t) list[0]].kind == Output)) ? sourceW : instrW;
            float h = 0.0f;
            for (int i : list) h += boxH (boxes[(size_t) i]) + gapY;
            float y = 44.0f + (tallest - h) * 0.5f;
            for (int i : list)
            {
                auto& b = boxes[(size_t) i];
                b.area = { x, y, w, boxH (b) };
                y += boxH (b) + gapY;
            }
            const auto& first = boxes[(size_t) list[0]];
            columnTitles.push_back ({ x, first.kind == Output ? juce::String ("Output")
                                         : c == 0 ? juce::String ("Where notes and data come from")
                                         : c == 1 ? juce::String ("Instruments started from outside")
                                                  : "Started by column " + juce::String (c - 1) });
            x += w + gapX + (c == 0 ? 20.0f : 0.0f);
        }
        // porte: le frecce di una scatola si distribuiscono lungo il lato,
        // in ordine di altezza dell'altra estremita' (niente frecce sovrapposte)
        for (int i = 0; i < (int) boxes.size(); ++i)
        {
            std::vector<int> outs, ins;
            for (int k = 0; k < (int) edges.size(); ++k)
            {
                if (edges[(size_t) k].from == i) outs.push_back (k);
                if (edges[(size_t) k].to == i)   ins.push_back (k);
            }
            const auto& a = boxes[(size_t) i].area;
            auto spread = [&] (std::vector<int>& list, bool out)
            {
                std::stable_sort (list.begin(), list.end(), [&] (int x, int y)
                {
                    const auto& ox = boxes[(size_t) (out ? edges[(size_t) x].to : edges[(size_t) x].from)].area;
                    const auto& oy = boxes[(size_t) (out ? edges[(size_t) y].to : edges[(size_t) y].from)].area;
                    return ox.getCentreY() < oy.getCentreY();
                });
                for (int n = 0; n < (int) list.size(); ++n)
                {
                    const float y = a.getY() + a.getHeight() * (float) (n + 1) / (float) (list.size() + 1);
                    (out ? edges[(size_t) list[(size_t) n]].fromY : edges[(size_t) list[(size_t) n]].toY) = y;
                }
            };
            spread (outs, true);
            spread (ins, false);
        }

        float bottom = 0.0f;
        for (const auto& b : boxes) bottom = juce::jmax (bottom, b.area.getBottom());
        laneTop = bottom + 26.0f;
        int lanes = 0;
        for (int k = 0; k < (int) edges.size(); ++k)
            if (usesLane (edges[(size_t) k])) edges[(size_t) k].lane = lanes++;
        setSize ((int) (x + 20.0f), (int) (laneTop + 14.0f * (float) lanes + 40.0f));
    }

    juce::Path edgePath (const Edge& e) const
    {
        const auto& a = boxes[(size_t) e.from].area;
        const auto& b = boxes[(size_t) e.to].area;
        juce::Path p;
        if (e.lane >= 0)
        {
            const float ly = laneTop + 14.0f * (float) e.lane;
            const juce::Point<float> s (a.getRight(), e.fromY), t (b.getX() - 2.0f, e.toY);
            const float r = 36.0f;
            const float sx = s.x + 22.0f, tx = t.x - 22.0f;
            p.startNewSubPath (s);
            p.cubicTo ({ sx + r, s.y }, { sx, ly }, { sx + r, ly });
            p.lineTo (tx - r, ly);
            p.cubicTo ({ tx, ly }, { tx - r, t.y }, t);
        }
        else if (b.getX() > a.getRight())
        {
            const juce::Point<float> s (a.getRight(), e.fromY), t (b.getX() - 2.0f, e.toY);
            const float dx = juce::jmax (40.0f, (t.x - s.x) * 0.5f);
            p.startNewSubPath (s);
            p.cubicTo (s.translated (dx, 0.0f), t.translated (-dx, 0.0f), t);
        }
        else
        {
            // stessa colonna o all'indietro: un giro sulla destra
            const juce::Point<float> s (a.getRight(), e.fromY), t (b.getRight() + 2.0f, e.toY);
            const float out = juce::jmax (a.getRight(), b.getRight()) + (e.from == e.to ? 70.0f : 46.0f + 10.0f * (float) (std::abs (e.from - e.to) % 4));
            p.startNewSubPath (s);
            p.cubicTo ({ out, s.y }, { out, t.y }, t);
        }
        return p;
    }

    static void arrowHead (juce::Graphics& g, const juce::Path& p)
    {
        const float len = p.getLength();
        if (len < 12.0f) return;
        const auto tip = p.getPointAlongPath (len);
        const auto back = p.getPointAlongPath (len - 9.0f);
        const auto dir = (tip - back) / juce::jmax (0.001f, tip.getDistanceFrom (back));
        const juce::Point<float> n (-dir.y, dir.x);
        juce::Path a;
        a.addTriangle (tip, tip - dir * 9.0f + n * 4.5f, tip - dir * 9.0f - n * 4.5f);
        g.fillPath (a);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundOrchestraMapCanvas)
};
