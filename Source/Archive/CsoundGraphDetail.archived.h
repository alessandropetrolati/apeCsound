#pragma once
/*
    ARCHIVIO - la vecchia vista "Detail" della vista Algorithm (tolta il
    18.11.2026, sostituita dal passo-passo e dalle fasce i/k/a di Explain).
    NON e' inclusa da nessuna parte e non viene compilata.

    Per ripristinarla:
      1. rimettere la classe CsoundGraphCanvas qui sotto in CsoundGraphView.h,
         subito dopo gli #include (prima di CsoundGraphCloseButton);
      2. rimettere in CsoundGraphView i pezzi elencati in fondo (membro
         "CsoundGraphCanvas canvas;", bottone detailButton, callback,
         showPopoverFor, i rami Detail di selectLineInGraph e showBlock,
         setLiveCables / setLiveValues nel timer);
      3. nel menu della vista (showModeMenu) rimettere la voce
         add (2, "Detail", ! flowMode && ! explainMode) e il ramo
         setFlowMode (r == 1); in showsStepBar il passo-passo tornava su
         Detail con: currentBlock >= 0 && ! flowMode && ! explainMode.
*/
#include "../CsoundGraphView.h"   // solo come promemoria delle dipendenze

//==============================================================================
class CsoundGraphCanvas final : public juce::Component
{
public:
    CsoundGraphCanvas() = default;

    std::function<void (int box)>   onBoxTapped;       // tocco su una scatola (popover)
    std::function<void()>           onEmptyTapped;     // tocco fuori dalle scatole

    int  getBlock() const noexcept                       { return block; }
    int  getNumBoxes() const noexcept                    { return (int) boxes.size(); }
    int  getBoxRef (int box) const                       { return boxes[(size_t) box].ref; }
    juce::Rectangle<float> getBoxBounds (int box) const  { return boxes[(size_t) box].r; }
    void clearSelection()                                { selected = -1; repaint(); }
    void setSelected (int box)                           { selected = box; repaint(); }
    void setLiveValues (const std::map<juce::String, juce::String>& v) { if (v != live) { live = v; repaint(); } }
    void setLiveCables (const csoundGraphUI::LiveCables& lc)  { const auto h = liveCables.hover; liveCables = lc; liveCables.hover = h; repaint(); }
    std::function<void (const juce::String& var, juce::Point<float> where)> onCableTapped;
    std::function<void (const juce::String& var)> onCableHovered;

    /** block: indice in orchestra.blocks, oppure -1 per il livello orchestra.
        Il puntatore deve restare valido finche' non si chiama di nuovo. */
    void setOrchestra (const csoundGraph::Orchestra* o, int blockIndex)
    {
        orc = o;
        block = blockIndex;
        selected = -1;
        if (orc != nullptr && block >= (int) orc->blocks.size()) block = -1;
        layout();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace csoundGraphUI;
        const auto bg = kBg;
        const auto fg = kText;
        g.fillAll (bg);
        if (orc == nullptr) return;

        if (block < 0) { paintOrchestra (g, bg, fg); return; }
        const auto& gr = orc->blocks[(size_t) block];

        // corsie per tipo: init (i), controllo (k), audio (a)
        for (const auto& ln : lanes)
        {
            g.setColour (kLaneFill);
            g.fillRoundedRectangle (ln.r, 9.0f);
            g.setColour (rateColour (ln.rate).withAlpha (0.85f));
            g.setFont (boldFont (11.0f));
            g.drawText (ln.label, ln.r.withHeight (20.0f).translated (10.0f, 3.0f), juce::Justification::centredLeft, false);
        }

        // cornici: dalla piu' esterna alla piu' interna
        for (size_t f = 0; f < frameRects.size(); ++f)
        {
            const auto r = frameRects[f];
            if (r.isEmpty()) continue;
            g.setColour (fg.withAlpha (0.045f));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (fg.withAlpha (0.30f));
            g.drawRoundedRectangle (r, 7.0f, 1.0f);
            const auto& fr = gr.frames[f];
            if (fr.kind == "else" || fr.kind == "elseif")
            {
                g.setFont (plainFont (10.5f));
                g.drawText (js (fr.kind), r.withHeight (13.0f).translated (6.0f, 1.0f), juce::Justification::centredLeft, false);
            }
        }

        // cavi
        std::set<std::pair<int, int>> labelled;
        cableHits.clear();
        std::vector<std::pair<juce::Path, juce::String>> liveLabels;
        for (const auto& c : gr.cables)
        {
            if (c.from < 0 || c.to < 0 || c.from >= (int) boxes.size() || c.to >= (int) boxes.size()) continue;
            const auto p1 = outletPos (c.from, c.fromOut);
            const auto p2 = inletPos (c.to, c.toIn);
            const auto var = js (c.var);
            const bool hot = var == liveCables.hover || var == liveCables.selected;
            const auto col = rateColour (c.rate).brighter (hot ? 0.35f : 0.0f);
            float thick = c.rate == 'a' ? 2.4f : 1.2f;
            if (liveCables.all && c.rate == 'a')            // Live: lo spessore segue il livello
                if (auto it = liveCables.level.find (var); it != liveCables.level.end())
                    thick = 1.2f + 5.0f * std::sqrt (juce::jlimit (0.0f, 1.0f, it->second));
            if (hot) thick += 1.2f;
            juce::Path p;
            p.startNewSubPath (p1);
            if (! c.feedback && p2.y > p1.y)
            {
                const float d = juce::jmax (16.0f, (p2.y - p1.y) * 0.45f);
                p.cubicTo (p1.x, p1.y + d, p2.x, p2.y - d, p2.x, p2.y);
                g.setColour (col.withAlpha (0.85f));
                g.strokePath (p, juce::PathStrokeType (thick));
            }
            else
            {
                // all'indietro: valore del ciclo precedente
                const float side = juce::jmax (p1.x, p2.x) + 60.0f;
                p.cubicTo (side, p1.y + 50.0f, side, p2.y - 50.0f, p2.x, p2.y);
                juce::Path dashed;
                const float dashes[] = { 5.0f, 4.0f };
                juce::PathStrokeType (thick).createDashedStroke (dashed, p, dashes, 2);
                g.setColour (col);
                g.fillPath (dashed);
            }
            cableHits.push_back ({ p, var });
            const bool firstOfOutlet = labelled.insert ({ c.from, c.fromOut }).second;
            if (liveCables.shows (var) && liveCables.text.count (var) && (firstOfOutlet || hot))
                liveLabels.push_back ({ p, var });
            else if (firstOfOutlet)
            {
                g.setColour (col);
                g.setFont (plainFont (10.0f));
                g.drawText (var, juce::Rectangle<float> (p1.x + 4.0f, p1.y + 1.0f, 120.0f, 11.0f), juce::Justification::centredLeft, false);
            }
        }

        // scatole
        for (int i = 0; i < (int) boxes.size(); ++i)
        {
            const auto& b = boxes[(size_t) i];
            const auto& n = gr.nodes[(size_t) i];
            const bool special = isSpecial (n);
            const bool head = n.kind == csoundGraph::NodeKind::FrameHead || n.kind == csoundGraph::NodeKind::Jump || n.kind == csoundGraph::NodeKind::Label;
            auto fill = kBoxFill;
            if (special) fill = kBoxFill.interpolatedWith (juce::Colour (0xff3b82f6), 0.22f);
            if (head)    fill = kBoxFill.interpolatedWith (juce::Colour (0xfff2c14e), 0.22f);
            if (n.kind == csoundGraph::NodeKind::Undefined) fill = kBoxFill.interpolatedWith (juce::Colours::red, 0.30f);
            g.setColour (fill);
            g.fillRoundedRectangle (b.r, 5.0f);
            if (i == selected)
            {
                g.setColour (kAccent.withAlpha (0.35f));
                g.drawRoundedRectangle (b.r.expanded (3.0f), 7.0f, 3.0f);
            }
            g.setColour (i == selected ? kAccent : rateColour (n.rate).withAlpha (n.rate == 'i' ? 0.55f : 0.95f));
            g.drawRoundedRectangle (b.r, 5.0f, i == selected ? 2.2f : 1.3f);

            // ingressi (sopra) e uscite (sotto)
            g.setColour (fg.withAlpha (0.55f));
            for (int k = 0; k < (int) n.ins.size(); ++k)
            {
                const auto p = inletPos (i, k);
                g.fillRect (juce::Rectangle<float> (p.x - 3.0f, p.y, 6.0f, 2.5f));
            }
            for (int k = 0; k < (int) n.outs.size(); ++k)
            {
                const auto p = outletPos (i, k);
                g.setColour (rateColour (n.outs[(size_t) k].rate));
                g.fillRect (juce::Rectangle<float> (p.x - 3.0f, p.y - 2.5f, 6.0f, 2.5f));
            }

            auto tr = b.r.reduced (8.0f, 0.0f);
            g.setColour (fg);
            g.setFont (boldFont());
            const float tw = textWidth (boldFont(), b.title);
            g.drawText (b.title, tr.removeFromLeft (tw + 2.0f), juce::Justification::centredLeft, false);
            if (b.rest.isNotEmpty())
            {
                tr.removeFromLeft (5.0f);
                g.setColour (fg.withAlpha (0.78f));
                g.setFont (plainFont());
                g.drawText (b.rest, tr, juce::Justification::centredLeft, false);
            }
            if (! n.channel.empty())
                if (auto it = live.find (js (n.channel)); it != live.end() && it->second.isNotEmpty())
                {
                    g.setColour (kAccent);
                    g.setFont (boldFont (11.0f));
                    g.drawText (u8 ("\xe2\x97\x8f ") + it->second, juce::Rectangle<float> (b.r.getRight() + 6.0f, b.r.getY(), 120.0f, b.r.getHeight()),
                                juce::Justification::centredLeft, false);
                }
            if (n.line > 0 && ! special)
            {
                g.setColour (fg.withAlpha (0.35f));
                g.setFont (plainFont (9.5f));
                g.drawText (juce::String (n.line), juce::Rectangle<float> (b.r.getX() - 34.0f, b.r.getY(), 30.0f, b.r.getHeight()), juce::Justification::centredRight, false);
            }
        }

        // valori dal vivo: per ultimi, sopra tutto, senza sovrapporsi
        if (! liveLabels.empty())
        {
            std::vector<LiveLabel> ll;
            for (const auto& [path, var] : liveLabels)
                ll.push_back ({ path, var, liveCables.text[var], rateColour (var.startsWithChar ('a') ? 'a' : 'k'),
                                var == liveCables.hover || var == liveCables.selected });
            std::vector<juce::Rectangle<float>> obstacles;
            for (const auto& b : boxes) obstacles.push_back (b.r);
            drawLiveLabels (g, std::move (ll), obstacles);
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { csoundGraphUI::forwardWheel (*this, e, w); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto v = block >= 0 ? csoundGraphUI::cableAt (cableHits, e.position) : juce::String();
        if (v != liveCables.hover) { liveCables.hover = v; repaint(); if (onCableHovered) onCableHovered (v); }
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        if (liveCables.hover.isNotEmpty()) { liveCables.hover.clear(); repaint(); if (onCableHovered) onCableHovered ({}); }
    }

    // Tocco al rilascio, non alla pressione: un trascinamento (scorrimento
    // col dito su iOS) non apre il popover.
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (orc == nullptr || e.mouseWasDraggedSinceMouseDown()) return;
        for (int i = (int) boxes.size(); --i >= 0;)
        {
            if (! boxes[(size_t) i].r.contains (e.position)) continue;
            selected = i;
            repaint();
            if (onBoxTapped) onBoxTapped (i);
            return;
        }
        if (block >= 0)
            if (const auto v = csoundGraphUI::cableAt (cableHits, e.position); v.isNotEmpty())
            {
                if (onCableTapped) onCableTapped (v, e.position);
                return;
            }
        selected = -1;
        repaint();
        if (onEmptyTapped) onEmptyTapped();
    }

private:
    struct Box { juce::Rectangle<float> r; juce::String title, rest; int ref = -1; };

    const csoundGraph::Orchestra* orc = nullptr;
    int block = -1, selected = -1;
    std::vector<Box> boxes;
    std::vector<juce::Rectangle<float>> frameRects;
    std::map<juce::String, juce::String> live;     // valori vivi dei canali
    csoundGraphUI::LiveCables liveCables;          // valori dal vivo sui cavi
    std::vector<std::pair<juce::Path, juce::String>> cableHits;   // per il mouse / il tocco
    struct Lane { juce::Rectangle<float> r; juce::String label; char rate; };
    std::vector<Lane> lanes;

    static constexpr float boxH = 24.0f, rowH = 40.0f, colGap = 30.0f, laneGap = 26.0f, pad = 44.0f, laneTitleH = 24.0f;

    static bool isSpecial (const csoundGraph::Node& n)
    {
        return n.kind == csoundGraph::NodeKind::PField || n.kind == csoundGraph::NodeKind::GlobalIn || n.kind == csoundGraph::NodeKind::Undefined;
    }

    juce::Point<float> inletPos (int node, int in) const
    {
        const auto& r = boxes[(size_t) node].r;
        const int n = block >= 0 ? (int) orc->blocks[(size_t) block].nodes[(size_t) node].ins.size() : 1;
        const float x = n <= 1 ? r.getX() + 12.0f : r.getX() + 10.0f + (float) in * (r.getWidth() - 20.0f) / (float) (n - 1);
        return { x, r.getY() };
    }

    juce::Point<float> outletPos (int node, int out) const
    {
        const auto& r = boxes[(size_t) node].r;
        const int n = block >= 0 ? (int) orc->blocks[(size_t) block].nodes[(size_t) node].outs.size() : 1;
        const float x = n <= 1 ? r.getX() + 12.0f : r.getX() + 10.0f + (float) out * (r.getWidth() - 20.0f) / (float) (n - 1);
        return { x, r.getBottom() };
    }

    void describe (const csoundGraph::Node& n, Box& b) const
    {
        using namespace csoundGraphUI;
        using K = csoundGraph::NodeKind;
        juce::String rest;
        switch (n.kind)
        {
            case K::PField:    b.title = js (n.name); break;
            case K::GlobalIn:  b.title = "receive"; rest = js (n.name); break;
            case K::Undefined: b.title = "?"; rest = js (n.name); break;
            case K::Label:     b.title = js (n.name) + ":"; break;
            case K::FrameHead: b.title = js (n.name); rest = js (n.text); break;
            case K::Jump:      b.title = js (n.name); rest = js (n.text); break;
            case K::Macro:     b.title = js (n.name); rest = js (n.text).fromFirstOccurrenceOf (js (n.name), false, false); break;
            case K::Expr:
                for (size_t i = 0; i < n.outs.size(); ++i) b.title << (i ? ", " : "") << js (n.outs[i].var);
                rest = js (n.name) + " " + js (n.text);
                break;
            case K::Opcode:
                b.title = js (n.name);
                for (const auto& in : n.ins) if (in.isConst) rest << (rest.isEmpty() ? "" : " ") << js (in.text);
                break;
        }
        b.rest = shorten (rest);
    }

    void layout()
    {
        boxes.clear();
        frameRects.clear();
        lanes.clear();
        if (orc == nullptr) { setSize (10, 10); return; }
        if (block < 0) { layoutOrchestra(); return; }

        using namespace csoundGraphUI;
        const auto& gr = orc->blocks[(size_t) block];
        const int n = (int) gr.nodes.size();
        boxes.resize ((size_t) n);
        std::vector<float> width ((size_t) n, 60.0f);
        for (int i = 0; i < n; ++i)
        {
            describe (gr.nodes[(size_t) i], boxes[(size_t) i]);
            const auto& b = boxes[(size_t) i];
            width[(size_t) i] = juce::jmax (46.0f, textWidth (boldFont(), b.title) + (b.rest.isEmpty() ? 0.0f : textWidth (plainFont(), b.rest) + 7.0f) + 20.0f);
        }

        // DISPOSIZIONE
        // Righe: una scatola per riga, nell'ordine del codice (in Csound
        // l'ordine conta), con un piccolo stacco quando si entra o si esce
        // da una cornice.
        // Corsie: la colonna dipende dal tipo della scatola - a sinistra
        // init (i, solo alla nascita della nota), al centro controllo (k),
        // a destra audio (a, e gli spettri f). Il segnale scorre quindi da
        // sinistra a destra e dall'alto in basso, come la gerarchia dei
        // tipi di Csound. Le sorgenti (p-field, variabili globali) stanno in
        // una fascia in alto, sopra la loro corsia.
        // Dentro una corsia: una scatola continua sotto la sua sorgente se
        // ne e' l'ultima scatola (la catena resta dritta), altrimenti prende
        // la prima colonna "libera" (la cui ultima scatola non ha piu'
        // lettori), altrimenti apre una colonna nuova.
        auto laneOf = [] (const csoundGraph::Node& nd)
        {
            using K = csoundGraph::NodeKind;
            if (nd.rate == 'a' || nd.rate == 'f') return 2;
            if (nd.rate == 'k') return 1;
            if (nd.rate == 'i') return 0;
            if (nd.kind == K::FrameHead || nd.kind == K::Jump || nd.kind == K::Label) return 1;
            return 0;
        };

        std::vector<std::set<int>> sources ((size_t) n);
        std::vector<int> remaining ((size_t) n, 0);
        for (const auto& c : gr.cables)
            if (! c.feedback && c.from != c.to && c.from >= 0 && c.to >= 0 && c.from < n && c.to < n)
                if (sources[(size_t) c.to].insert (c.from).second)
                    ++remaining[(size_t) c.from];

        bool anySpecial = false;
        for (int i = 0; i < n; ++i) anySpecial = anySpecial || isSpecial (gr.nodes[(size_t) i]);

        std::vector<int> laneOfNode ((size_t) n, 0), colOf ((size_t) n, -1);
        std::vector<int> tail[3];
        std::vector<float> colW[3];
        std::vector<float> yOf ((size_t) n, 0.0f);
        const float topY = pad + laneTitleH;
        float y = topY + (anySpecial ? rowH + 10.0f : 0.0f);
        int prevFrame = -1;

        for (int id = 0; id < n; ++id)
        {
            const auto& node = gr.nodes[(size_t) id];
            const int lane = laneOf (node);
            laneOfNode[(size_t) id] = lane;
            if (isSpecial (node)) { yOf[(size_t) id] = topY; continue; }

            auto& tl = tail[lane];
            int col = -1;
            char bestRate = 0; int bestIn = INT_MAX;
            for (const auto& c : gr.cables)
                if (c.to == id && ! c.feedback && c.from != id && c.from >= 0 && c.from < n && laneOfNode[(size_t) c.from] == lane)
                {
                    const int cc = colOf[(size_t) c.from];
                    if (cc < 0 || tl[(size_t) cc] != c.from) continue;
                    // a parita' di corsia: prima il cavo audio, poi il primo ingresso
                    const char r = c.rate == 'a' ? 3 : c.rate == 'k' ? 2 : 1;
                    if (r > bestRate || (r == bestRate && c.toIn < bestIn)) { bestRate = r; bestIn = c.toIn; col = cc; }
                }
            for (int src : sources[(size_t) id]) --remaining[(size_t) src];
            if (col < 0)
                for (int c = 0; c < (int) tl.size(); ++c)
                    if (remaining[(size_t) tl[(size_t) c]] <= 0) { col = c; break; }
            if (col < 0) { col = (int) tl.size(); tl.push_back (id); colW[lane].push_back (0.0f); }
            else tl[(size_t) col] = id;
            colOf[(size_t) id] = col;
            colW[lane][(size_t) col] = juce::jmax (colW[lane][(size_t) col], width[(size_t) id]);

            if (node.frame != prevFrame) y += 12.0f;
            prevFrame = node.frame;
            yOf[(size_t) id] = y;
            y += rowH;
        }

        // x delle colonne, corsia per corsia (le corsie vuote non occupano posto)
        std::vector<float> colX[3];
        float laneLeft[3] = { 0, 0, 0 }, laneRight[3] = { 0, 0, 0 };
        float x = pad;
        for (int lane = 0; lane < 3; ++lane)
        {
            // le sorgenti della fascia alta di questa corsia, una accanto all'altra
            float topW = 0.0f;
            for (int i = 0; i < n; ++i)
                if (isSpecial (gr.nodes[(size_t) i]) && laneOfNode[(size_t) i] == lane) topW += width[(size_t) i] + 12.0f;
            if (colW[lane].empty() && topW <= 0.0f) { laneLeft[lane] = laneRight[lane] = -1.0f; continue; }

            laneLeft[lane] = x;
            float cx = x + 12.0f;
            for (size_t c = 0; c < colW[lane].size(); ++c) { colX[lane].push_back (cx); cx += colW[lane][c] + colGap; }
            // fascia alta: prima i p-field in ordine (p3, p4...), poi le globali
            std::vector<int> top;
            for (int i = 0; i < n; ++i)
                if (isSpecial (gr.nodes[(size_t) i]) && laneOfNode[(size_t) i] == lane) top.push_back (i);
            auto key = [&gr] (int i)
            {
                const auto& nd = gr.nodes[(size_t) i];
                return nd.kind == csoundGraph::NodeKind::PField ? std::atoi (nd.name.c_str() + 1) : 1000 + i;
            };
            std::stable_sort (top.begin(), top.end(), [&key] (int a, int b) { return key (a) < key (b); });
            float tx = x + 12.0f;
            for (int i : top)
            {
                boxes[(size_t) i].r = { tx, yOf[(size_t) i], width[(size_t) i], boxH };
                tx += width[(size_t) i] + 12.0f;
            }
            laneRight[lane] = juce::jmax (cx - colGap + 12.0f, tx);
            x = laneRight[lane] + laneGap;
        }
        for (int i = 0; i < n; ++i)
            if (! isSpecial (gr.nodes[(size_t) i]))
            {
                const int lane = laneOfNode[(size_t) i];
                boxes[(size_t) i].r = { colX[lane][(size_t) colOf[(size_t) i]], yOf[(size_t) i], width[(size_t) i], boxH };
            }

        lanes.clear();
        static const char* laneNames[3] = { "init  \xc2\xb7  i-rate (once per note)", "control  \xc2\xb7  k-rate", "audio  \xc2\xb7  a-rate" };
        static const char laneRates[3] = { 'i', 'k', 'a' };
        for (int lane = 0; lane < 3; ++lane)
            if (laneLeft[lane] >= 0.0f)
                lanes.push_back ({ juce::Rectangle<float> (laneLeft[lane], pad - 6.0f, laneRight[lane] - laneLeft[lane], y - pad + 14.0f),
                                   juce::String::fromUTF8 (laneNames[lane]), laneRates[lane] });

        // cornici: dalle piu' interne (indice alto) alle piu' esterne
        frameRects.assign (gr.frames.size(), {});
        for (int f = (int) gr.frames.size(); --f >= 0;)
        {
            juce::Rectangle<float> r;
            bool any = false;
            auto add = [&] (juce::Rectangle<float> q) { if (q.isEmpty()) return; r = any ? r.getUnion (q) : q; any = true; };
            for (int i = 0; i < n; ++i) if (gr.nodes[(size_t) i].frame == f) add (boxes[(size_t) i].r);
            for (size_t k = 0; k < gr.frames.size(); ++k) if (gr.frames[k].parent == f) add (frameRects[k]);
            if (any) frameRects[(size_t) f] = r.expanded (9.0f, 8.0f).withTop (r.getY() - 15.0f);
        }

        float right = x;
        for (const auto& r : frameRects) right = juce::jmax (right, r.getRight() + colGap);
        setSize ((int) (right + pad + 80.0f), (int) (y + pad + 20.0f));
    }

    //==========================================================================
    // livello orchestra: uno strumento = una scatola
    struct Arc { int from, to; juce::String name; char rate; bool event; };
    std::vector<Arc> arcs;

    void layoutOrchestra()
    {
        using namespace csoundGraphUI;
        arcs.clear();
        std::vector<int> boxOfBlock (orc->blocks.size(), -1);
        float y = pad;
        for (size_t b = 0; b < orc->blocks.size(); ++b)
        {
            const auto& gr = orc->blocks[b];
            if (gr.kind == csoundGraph::Graph::Udo || (b == 0 && gr.nodes.empty())) continue;
            Box bx;
            bx.ref = (int) b;
            bx.title = js (gr.title());
            juce::String sub;
            for (const auto& nm : gr.names)
            {
                auto it = orc->scoreNotes.find (nm);
                if (it != orc->scoreNotes.end()) sub << "score: " << it->second << (it->second == 1 ? " note" : " notes");
            }
            if (! gr.pfields.empty())
            {
                if (sub.isNotEmpty()) sub << csoundGraphUI::u8 ("  \xc2\xb7  ");
                for (int p : gr.pfields) sub << "p" << p << " ";
            }
            bx.rest = sub.trim();
            bx.r = { pad + 170.0f, y, 250.0f, 40.0f };
            boxOfBlock[b] = (int) boxes.size();
            boxes.push_back (bx);
            y += 66.0f;
        }
        std::set<juce::String> seen;
        for (const auto& l : orc->links)
        {
            if (l.from < 0 || l.to < 0 || boxOfBlock[(size_t) l.from] < 0 || boxOfBlock[(size_t) l.to] < 0) continue;
            const juce::String key = juce::String (l.from) + ">" + juce::String (l.to) + js (l.kind) + js (l.name);
            if (! seen.insert (key).second) continue;
            arcs.push_back ({ boxOfBlock[(size_t) l.from], boxOfBlock[(size_t) l.to], js (l.name), l.rate, l.kind == "evento" });
        }
        setSize ((int) (pad * 2.0f + 170.0f + 250.0f + 260.0f), (int) (y + pad));
    }

    void paintOrchestra (juce::Graphics& g, juce::Colour bg, juce::Colour fg)
    {
        juce::ignoreUnused (bg);
        using namespace csoundGraphUI;
        int k = 0;
        for (const auto& a : arcs)
        {
            const auto& r1 = boxes[(size_t) a.from].r;
            const auto& r2 = boxes[(size_t) a.to].r;
            const float out = 46.0f + 16.0f * (float) (k++ % 9);
            juce::Path p;
            juce::Point<float> p1, p2, mid;
            if (a.event)
            {
                p1 = { r1.getX(), r1.getCentreY() - 6.0f }; p2 = { r2.getX(), r2.getCentreY() + 6.0f };
                p.startNewSubPath (p1);
                p.cubicTo (p1.x - out, p1.y, p2.x - out, p2.y, p2.x, p2.y);
                mid = { p1.x - out * 0.75f - 110.0f, (p1.y + p2.y) * 0.5f - 6.0f };
            }
            else
            {
                p1 = { r1.getRight(), r1.getCentreY() + 6.0f }; p2 = { r2.getRight(), r2.getCentreY() - 6.0f };
                p.startNewSubPath (p1);
                p.cubicTo (p1.x + out, p1.y, p2.x + out, p2.y, p2.x, p2.y);
                mid = { p1.x + out * 0.75f + 4.0f, (p1.y + p2.y) * 0.5f - 6.0f };
            }
            const auto col = rateColour (a.rate);
            g.setColour (col);
            if (a.event)
            {
                juce::Path dashed;
                const float dashes[] = { 5.0f, 4.0f };
                juce::PathStrokeType (1.3f).createDashedStroke (dashed, p, dashes, 2);
                g.fillPath (dashed);
            }
            else
                g.strokePath (p, juce::PathStrokeType (a.rate == 'a' ? 2.4f : 1.2f));
            g.fillEllipse (p2.x - 3.0f, p2.y - 3.0f, 6.0f, 6.0f);     // il pallino segna l'arrivo
            // etichetta su un punto della curva diverso per ogni arco: due
            // archi fra gli stessi blocchi non si scrivono uno sopra l'altro
            const float frac = 0.30f + 0.12f * (float) ((k - 1) % 4);
            const auto at = p.getPointAlongPath (p.getLength() * frac);
            juce::ignoreUnused (mid);
            g.setFont (plainFont (10.5f));
            g.drawText (a.name, a.event ? juce::Rectangle<float> (at.x - 114.0f, at.y - 6.0f, 110.0f, 12.0f)
                                        : juce::Rectangle<float> (at.x + 4.0f, at.y - 6.0f, 110.0f, 12.0f),
                        a.event ? juce::Justification::centredRight : juce::Justification::centredLeft, false);
        }
        for (size_t bi = 0; bi < boxes.size(); ++bi)
        {
            const auto& b = boxes[bi];
            g.setColour (kBoxFill);
            g.fillRoundedRectangle (b.r, 7.0f);
            g.setColour ((int) bi == selected ? kAccent : fg.withAlpha (0.45f));
            g.drawRoundedRectangle (b.r, 7.0f, (int) bi == selected ? 2.2f : 1.3f);
            g.setColour (fg);
            g.setFont (boldFont (13.5f));
            g.drawText (b.title, b.r.reduced (10.0f, 4.0f).removeFromTop (17.0f), juce::Justification::centredLeft, true);
            g.setColour (fg.withAlpha (0.6f));
            g.setFont (plainFont (10.5f));
            g.drawText (b.rest, b.r.reduced (10.0f, 4.0f).removeFromBottom (14.0f), juce::Justification::centredLeft, true);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundGraphCanvas)
};


/*==============================================================================
   Pezzi tolti da CsoundGraphView (testo originale, nell'ordine):

------------------------------------------------------------------------------
        viewport.setViewedComponent (&canvas, false);
------------------------------------------------------------------------------
        popover.onHidden = [this] { canvas.clearSelection(); };

------------------------------------------------------------------------------
        detailButton.setTooltip ("Detail: every line of the code as a box");

------------------------------------------------------------------------------
        detailButton.onClick = [this] { setFlowMode (false); };

------------------------------------------------------------------------------
        for (auto* b : { &flowButton, &detailButton, &explainButton, &modeButton, &codeButton })
------------------------------------------------------------------------------
        canvas.onCableHovered     = [this] (const juce::String& v) { hoverVar = v; };

------------------------------------------------------------------------------
        canvas.onCableTapped     = [this] (const juce::String& v, juce::Point<float> p) { openCableInfo (v, getLocalPoint (&canvas, p).toInt()); };

------------------------------------------------------------------------------
        canvas.onBoxTapped   = [this] (int box) { showPopoverFor (box); if (canvas.getBlock() >= 0) syncCodeToNode (box); };
        canvas.onEmptyTapped = [this] { popover.hide(); };

------------------------------------------------------------------------------
        for (auto* b : { &flowButton, &detailButton, &explainButton, &codeButton, &liveButton })
------------------------------------------------------------------------------
    void selectLineInGraph (int line)
    {
        if (currentBlock < 0) return;
        codePane.setHighlightedLine (line);
        const auto& g = orchestra.blocks[(size_t) currentBlock];
        popover.hide();
        for (int i = 0; i < (int) g.nodes.size(); ++i)
        {
            if (g.nodes[(size_t) i].line != line) continue;
            juce::Rectangle<int> r;
            if (flowMode)
            {
                if (! flowCanvas.selectNode (i)) continue;
                r = flowCanvas.getNodeBounds (i).toNearestInt();
            }
            else
            {
                canvas.setSelected (i);
                r = canvas.getBoxBounds (i).toNearestInt();
            }
            viewport.setViewPosition (juce::jmax (0, r.getCentreX() - viewport.getWidth() / 2), juce::jmax (0, r.getCentreY() - viewport.getHeight() / 2));
            return;
        }
        if (flowMode) flowCanvas.clearSelection();   // riga non nel flusso (non arriva all'uscita)
        else          canvas.clearSelection();
    }

------------------------------------------------------------------------------
        canvas.setLiveCables (liveState);

------------------------------------------------------------------------------
    juce::TextButton flowButton { "Flow" }, detailButton { "Detail" }, explainButton { "Explain" }, modeButton;
------------------------------------------------------------------------------
    CsoundGraphCanvas canvas;

------------------------------------------------------------------------------
        canvas.setOrchestra (&orchestra, b);

------------------------------------------------------------------------------
        pill (detailButton, b >= 0 && ! flowMode && ! explainMode);

------------------------------------------------------------------------------
    void showPopoverFor (int box)
    {
        if (canvas.getBlock() >= 0) { showPopoverForNode (box, canvas.getBoxBounds (box), &canvas); return; }
        Help h;
        std::vector<CsoundGraphPopover::Action> actions;
        describeBlock (canvas.getBoxRef (box), h, actions);
        const auto r = getLocalArea (&canvas, canvas.getBoxBounds (box).toNearestInt());
        popover.show (h.s, r, std::move (actions));
    }


------------------------------------------------------------------------------
            canvas.setLiveValues (values);

------------------------------------------------------------------------------
                                     : map  ? static_cast<juce::Component*> (&mapCanvas) : &canvas, false);
==============================================================================*/
