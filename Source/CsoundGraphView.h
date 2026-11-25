#pragma once
/*
    CsoundGraphView.h — la vista "Grafo" (sola lettura) per apeCsound.

    Alessandro Petrolati — www.apesoft.it · prototipo del 17.11.2026.

    Disegna il modello di CsoundGraphModel.h: una scatola per riga di codice,
    un cavo per variabile, cornici per if / while. Le scatole scendono
    nell'ordine delle righe (in Csound l'ordine conta). Un tocco su una
    scatola apre un popover con l'help e il contesto ("Show in Code" porta
    il cursore dell'editor su quella riga).

    Classi, tutte in questo header (niente .cpp, niente modifica al .jucer):
      (la vecchia vista Detail, CsoundGraphCanvas, e' in Archive/CsoundGraphDetail.archived.h)
      CsoundGraphFlowCanvas / CsoundExplainCanvas / CsoundOrchestraMapCanvas  le viste
      CsoundGraphPopover  help dell'opcode e contesto della scatola toccata
      CsoundGraphView     overlay a tutta finestra nell'editor, con la grafica
                          di apeCsound Guide: titolo, menu dei blocchi (lo
                          stesso foglio degli altri menu, anche su iOS), x;
                          sotto il grafo, che scorre col dito

    NON ancora compilato in Xcode: scritto per JUCE 8 (FontOptions,
    GlyphArrangement::getStringWidth) come il resto di apeCsound.
*/
#include <JuceHeader.h>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <map>
#include <functional>
#include <set>
#include <vector>
#include "CsoundGraphModel.h"
#include "CsoundOpcodeHelp.h"
#include "CsoundActionSheet.h"
#include "CsoundGraphStyle.h"
#include "CsoundGraphFlow.h"
#include "CsoundExplain.h"
#include "CsoundOrchestraMap.h"
#include "CsoundPatcher.h"     // vista Patch (prova, temporanea)


//==============================================================================
// x di chiusura disegnata (come in apeCsound Guide), non testo: in un
// bottone piccolo il carattere x con i margini del LookAndFeel diventava "..."
class CsoundGraphCloseButton final : public juce::Button
{
public:
    CsoundGraphCloseButton() : juce::Button ("Close") { setWantsKeyboardFocus (false); setTitle ("Close"); }
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        using namespace csoundGraphUI;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (kBorder.brighter (down ? 0.2f : highlighted ? 0.1f : 0.0f));
        g.fillEllipse (r);
        const auto c = r.getCentre();
        const float d = r.getWidth() * 0.18f;
        juce::Path x;
        x.startNewSubPath (c.x - d, c.y - d); x.lineTo (c.x + d, c.y + d);
        x.startNewSubPath (c.x + d, c.y - d); x.lineTo (c.x - d, c.y + d);
        g.setColour (kText);
        g.strokePath (x, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

//==============================================================================
// Agganciata / a tutta finestra, con un'icona come la x di chiusura:
// agganciata mostra "ingrandisci" (frecce verso gli angoli), a tutta
// finestra mostra "aggancia" (finestra con il pannello a destra).
class CsoundGraphDockButton final : public juce::Button
{
public:
    CsoundGraphDockButton() : juce::Button ("Dock") { setWantsKeyboardFocus (false); }
    void setDockedLook (bool isDocked) { dockedLook = isDocked; setTitle (isDocked ? "Full window" : "Dock beside the code"); repaint(); }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        using namespace csoundGraphUI;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (kBorder.brighter (down ? 0.2f : highlighted ? 0.1f : 0.0f));
        g.fillEllipse (r);
        const auto c = r.getCentre();
        const float d = r.getWidth() * 0.22f;
        juce::Path p;
        if (dockedLook)
        {
            // ingrandisci: due frecce verso gli angoli opposti
            const float a = d * 0.55f;
            p.startNewSubPath (c.x + d - a, c.y - d); p.lineTo (c.x + d, c.y - d); p.lineTo (c.x + d, c.y - d + a);
            p.startNewSubPath (c.x + d, c.y - d);     p.lineTo (c.x + 1.5f, c.y - 1.5f);
            p.startNewSubPath (c.x - d + a, c.y + d); p.lineTo (c.x - d, c.y + d); p.lineTo (c.x - d, c.y + d - a);
            p.startNewSubPath (c.x - d, c.y + d);     p.lineTo (c.x - 1.5f, c.y + 1.5f);
        }
        else
        {
            // aggancia: una finestra con il pannello a destra
            const juce::Rectangle<float> w (c.x - d * 1.15f, c.y - d * 0.9f, d * 2.3f, d * 1.8f);
            p.addRoundedRectangle (w, 1.5f);
            p.startNewSubPath (c.x + d * 0.2f, w.getY()); p.lineTo (c.x + d * 0.2f, w.getBottom());
        }
        g.setColour (kText);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (! dockedLook)
        {
            g.setColour (kText.withAlpha (0.45f));
            g.fillRect (juce::Rectangle<float> (c.x + d * 0.2f, c.y - d * 0.9f, d * 0.95f, d * 1.8f).reduced (1.0f));
        }
    }

private:
    bool dockedLook = true;
};

//==============================================================================
// Popover del grafo: help dell'opcode e contesto della scatola toccata.
// Componente figlio della vista (non una finestra): funziona uguale su
// desktop e iOS; si chiude con la x, toccando fuori o scegliendo un'azione.
class CsoundGraphPopover final : public juce::Component
{
public:
    struct Action { juce::String text; std::function<void()> run; };

    CsoundGraphPopover()
    {
        viewport.setViewedComponent (&body, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
        addAndMakeVisible (viewport);
        closeButton.onClick = [this] { hide(); };
        addAndMakeVisible (closeButton);
        setVisible (false);
    }

    std::function<void()> onHidden;

    /** anchor: la scatola, in coordinate del genitore. */
    void show (const juce::AttributedString& text, juce::Rectangle<int> anchor, std::vector<Action> newActions)
    {
        auto* parent = getParentComponent();
        if (parent == nullptr) return;

        actions = std::move (newActions);
        buttons.clear();
        for (auto& a : actions)
        {
            auto b = std::make_unique<juce::TextButton> (a.text);
            styleButton (*b);
            auto run = a.run;
            b->onClick = [this, run] { hide(); if (run) run(); };
            addAndMakeVisible (*b);
            buttons.push_back (std::move (b));
        }

        const int w = juce::jmin (400, parent->getWidth() - 16);
        body.layout.createLayout (text, (float) (w - 2 * pad - 14));
        const int textH = (int) std::ceil (body.layout.getHeight()) + 4;
        body.setSize (w - 2 * pad, textH);
        const int barH = buttonRows (w - 2 * pad) * buttonRowH;
        const int maxH = juce::jmax (120, parent->getHeight() - 24);
        const int h = juce::jmin (maxH, textH + 2 * pad + barH + 6);

        // sotto la scatola se c'e' posto, altrimenti sopra; sempre dentro la vista
        int y = anchor.getBottom() + 8;
        if (y + h > parent->getHeight() - 8) y = anchor.getY() - 8 - h;
        y = juce::jlimit (8, juce::jmax (8, parent->getHeight() - h - 8), y);
        const int x = juce::jlimit (8, juce::jmax (8, parent->getWidth() - w - 8), anchor.getX());
        setBounds (x, y, w, h);
        resized();   // i bottoni sono nuovi: se le dimensioni non cambiano setBounds non li dispone
        viewport.setViewPosition (0, 0);
        setVisible (true);
        toFront (false);
        repaint();
    }

    void hide()
    {
        if (! isVisible()) return;
        setVisible (false);
        if (onHidden) onHidden();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace csoundGraphUI;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 10.0f);
        g.setColour (kPopBg);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (kAccent.withAlpha (0.7f));
        g.drawRoundedRectangle (r, 10.0f, 1.2f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (pad);
        closeButton.setBounds (getWidth() - 34, 6, 28, 28);
        // bottoni a destra, dall'ultimo al primo; quando non ci stanno si va
        // a capo (riga sopra): l'ultimo, "Show in Code", resta in basso a destra
        const int rows = buttonRows (r.getWidth());
        if (rows > 0)
        {
            auto area = r.removeFromBottom (rows * buttonRowH);
            int row = rows - 1, x = area.getRight();
            for (int i = (int) buttons.size(); --i >= 0;)
            {
                auto& b = *buttons[(size_t) i];
                const int bw = buttonWidth (b);
                if (x - bw < area.getX() && x != area.getRight()) { --row; x = area.getRight(); }
                b.setBounds (x - bw, area.getY() + row * buttonRowH + 6, bw, buttonRowH - 6);
                x -= bw + 8;
            }
        }
        viewport.setBounds (r);
        body.setSize (juce::jmax (10, r.getWidth() - (body.getHeight() > r.getHeight() ? 8 : 0)), body.getHeight());
    }

private:
    struct Body final : public juce::Component
    {
        juce::TextLayout layout;
        void paint (juce::Graphics& g) override { layout.draw (g, getLocalBounds().toFloat()); }
    };

    static void styleButton (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, csoundGraphUI::kBorder);
        b.setColour (juce::TextButton::textColourOffId, csoundGraphUI::kText);
        b.setWantsKeyboardFocus (false);
        b.getProperties().set ("circular", true);           // pillola, come i bottoni di apeCsound
    }

    static constexpr int pad = 12, buttonRowH = 40;

    static int buttonWidth (juce::TextButton& b) { return juce::jmax (90, b.getBestWidthForHeight (buttonRowH - 6) + 36); }   // pillola: 16 px per lato

    // quante righe servono ai bottoni in una larghezza data
    int buttonRows (int width)
    {
        if (buttons.empty()) return 0;
        int rows = 1, x = width;
        for (int i = (int) buttons.size(); --i >= 0;)
        {
            const int bw = buttonWidth (*buttons[(size_t) i]);
            if (x - bw < 0 && x != width) { ++rows; x = width; }
            x -= bw + 8;
        }
        return rows;
    }
    Body body;
    csoundGraphUI::ScrollViewport viewport;
    CsoundGraphCloseButton closeButton;
    std::vector<Action> actions;
    std::vector<std::unique_ptr<juce::TextButton>> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundGraphPopover)
};

//==============================================================================
// Il valore dal vivo di un cavo toccato (come il mouse sopra un cavo in Max):
// nome, valore, per l'audio un oscilloscopio degli ultimi millisecondi e
// "Listen" per sentire solo quel segnale. Componente figlio della vista.
class CsoundGraphCableInfo final : public juce::Component
{
public:
    std::function<void()> onListenToggled, onClosed;

    CsoundGraphCableInfo()
    {
        addAndMakeVisible (closeButton);
        for (auto* b : { &listenButton })
        {
            b->setWantsKeyboardFocus (false);
            b->setColour (juce::TextButton::buttonColourId, csoundGraphUI::kBorder);
            b->setColour (juce::TextButton::textColourOffId, csoundGraphUI::kText);
            addAndMakeVisible (*b);
        }
        listenButton.getProperties().set ("circular", true);
        listenButton.onClick = [this] { if (onListenToggled) onListenToggled(); };
        closeButton.onClick = [this] { if (onClosed) onClosed(); };
        setVisible (false);
    }

    void show (const juce::String& variable, char variableRate, juce::Point<int> near, const juce::String& noteText = {})
    {
        var = variable; rate = variableRate; note = noteText;
        listenButton.setVisible (rate == 'a');
        setListening (false);
        scope.fill (0.0f);
        const int w = 300, h = (rate == 'a' ? 150 : 64) + (note.isNotEmpty() ? 44 : 0);
        auto* parent = getParentComponent();
        const int pw = parent != nullptr ? parent->getWidth() : 800, ph = parent != nullptr ? parent->getHeight() : 600;
        setBounds (juce::jlimit (8, juce::jmax (8, pw - w - 8), near.x + 12), juce::jlimit (8, juce::jmax (8, ph - h - 8), near.y + 12), w, h);
        setVisible (true);
        toFront (false);
    }

    void setListening (bool on)
    {
        listening = on;
        listenButton.setButtonText (on ? csoundGraphUI::u8 ("\xe2\x96\xa0  Stop") : csoundGraphUI::u8 ("\xe2\x96\xb6  Listen"));
        listenButton.setColour (juce::TextButton::buttonColourId, on ? juce::Colour (0xffc0392b) : csoundGraphUI::kBorder);
    }
    bool isListening() const noexcept { return listening; }
    const juce::String& getVariable() const noexcept { return var; }
    char getRate() const noexcept { return rate; }

    void update (const juce::String& valueText, bool live, const float* samples, int n)
    {
        value = valueText; playing = live;
        if (samples != nullptr) for (int i = 0; i < juce::jmin (n, (int) scope.size()); ++i) scope[(size_t) i] = samples[i];
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace csoundGraphUI;
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 10.0f);
        g.setColour (kPopBg);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (rateColour (rate).withAlpha (0.8f));
        g.drawRoundedRectangle (r, 10.0f, 1.2f);

        auto t = getLocalBounds().reduced (12, 8);
        auto head = t.removeFromTop (22);
        head.removeFromRight (30);
        g.setColour (kText);
        g.setFont (boldFont (14.0f));
        g.drawText (var, head.removeFromLeft (120), juce::Justification::centredLeft, true);
        g.setColour (playing ? rateColour (rate) : kTextMuted);
        g.setFont (boldFont (14.0f));
        g.drawText (playing ? value : juce::String ("no note playing"), head, juce::Justification::centredRight, true);
        g.setColour (kTextMuted);
        g.setFont (plainFont (11.0f));
        g.drawText (rate == 'a' ? "audio - level of the last cycle" : rate == 'k' ? "control - value now" : "init - value of the latest note",
                    t.removeFromTop (16), juce::Justification::centredLeft, true);

        if (rate == 'a')
        {
            auto sc = t.removeFromTop (60).toFloat().reduced (0.0f, 4.0f);
            g.setColour (kBg);
            g.fillRoundedRectangle (sc, 6.0f);
            g.setColour (kBorder);
            g.drawHorizontalLine ((int) sc.getCentreY(), sc.getX() + 4.0f, sc.getRight() - 4.0f);
            float peak = 0.001f;
            for (auto v : scope) peak = juce::jmax (peak, std::abs (v));
            const float scale = 1.0f / juce::jmax (peak, 0.05f);     // ingrandisce i segnali deboli
            juce::Path p;
            const auto in = sc.reduced (6.0f, 6.0f);
            for (int i = 0; i < (int) scope.size(); ++i)
            {
                const float x = in.getX() + in.getWidth() * (float) i / (float) (scope.size() - 1);
                const float y = in.getCentreY() - in.getHeight() * 0.5f * juce::jlimit (-1.0f, 1.0f, scope[(size_t) i] * scale);
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (rateColour ('a'));
            g.strokePath (p, juce::PathStrokeType (1.4f));
        }
        if (note.isNotEmpty())
        {
            g.setColour (juce::Colour (0xfff2c14e));
            g.setFont (plainFont (11.0f));
            g.drawFittedText (note, t.removeFromTop (42).reduced (0, 2), juce::Justification::topLeft, 3, 0.9f);
        }
    }

    void resized() override
    {
        closeButton.setBounds (getWidth() - 34, 6, 26, 26);
        listenButton.setBounds (getLocalBounds().reduced (12, 8).removeFromBottom (34).removeFromRight (120));
    }

private:
    juce::String var, value, note;
    char rate = 'k';
    bool listening = false, playing = false;
    std::array<float, 256> scope {};
    juce::TextButton listenButton;
    CsoundGraphCloseButton closeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundGraphCableInfo)
};

//==============================================================================
// Vista "Algorithm": overlay a tutta finestra dentro l'editor principale,
// con la stessa grafica di apeCsound Guide (titolo, linea, x di chiusura,
// fondo delle schede); sotto, il grafo su fondo piu' scuro. Si chiude con
// la x, con Esc o con "Show in Code" di un popover.
//==============================================================================
// Percorso: Orchestra > instr 10 > Space and effects > reverbsc. Ogni voce
// (tranne l'ultima) si tocca per tornare a quel livello.
class CsoundGraphCrumbs final : public juce::Component
{
public:
    struct Item { juce::String text; std::function<void()> action; };
    void setItems (std::vector<Item> newItems) { items = std::move (newItems); hover = -1; repaint(); }
    bool isEmpty() const noexcept { return items.empty(); }

    void paint (juce::Graphics& g) override
    {
        using namespace csoundGraphUI;
        areas.clear();
        const auto sep = u8 ("  \xe2\x80\xba  ");
        const auto f = plainFont (13.0f), fb = boldFont (13.0f);
        // da destra: se non ci sta tutto si tagliano le prime voci
        float total = 0.0f;
        std::vector<float> w;
        for (size_t i = 0; i < items.size(); ++i) { w.push_back (textWidth (i + 1 == items.size() ? fb : f, items[i].text)); total += w.back() + (i ? textWidth (f, sep) : 0.0f); }
        float x = juce::jmax (0.0f, (float) getWidth() - total);
        const float sw = textWidth (f, sep);
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (i > 0) { g.setColour (kTextMuted); g.setFont (f); g.drawText (sep, juce::Rectangle<float> (x, 0.0f, sw, (float) getHeight()), juce::Justification::centred, false); x += sw; }
            const bool last = i + 1 == items.size();
            const juce::Rectangle<float> r (x, 0.0f, w[i], (float) getHeight());
            areas.push_back (r);
            g.setColour (last ? kText : ((int) i == hover ? kAccent : kTextMuted));
            g.setFont (last ? fb : f);
            g.drawText (items[i].text, r, juce::Justification::centredLeft, false);
            x += w[i];
        }
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        int h = -1;
        for (int i = 0; i + 1 < (int) areas.size(); ++i) if (areas[(size_t) i].contains (e.position) && items[(size_t) i].action) h = i;
        if (h != hover) { hover = h; setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { if (hover >= 0) { hover = -1; repaint(); } }
    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int i = 0; i + 1 < (int) areas.size(); ++i)
            if (areas[(size_t) i].contains (e.position) && items[(size_t) i].action)
            {
                auto action = items[(size_t) i].action;     // l'azione puo' cambiare le voci
                action();
                return;
            }
    }

private:
    std::vector<Item> items;
    std::vector<juce::Rectangle<float>> areas;
    int hover = -1;
};

class CsoundGraphView final : public juce::Component,
                              private juce::Timer
{
public:
    std::function<juce::String()> getText;                                  // il testo del csd
    std::function<juce::Array<CsoundLiveOpcodeInfo>()> getOpcodes;          // facoltativa: opcode del motore in esecuzione
    std::function<void (int line)> onShowInCode;                            // riga del file, da 1 (la vista si chiude)
    std::function<std::map<juce::String, juce::String>()> getChannelValues; // facoltativa: valori correnti dei canali del motore
    // Valori dal vivo sui cavi: le variabili dello strumento in esecuzione,
    // lette dal motore senza ricompilare (vedi CsoundAudioProcessor::setLiveVariables)
    std::function<void (const juce::String& instr, const juce::StringArray& vars)> setLiveVariables;   // vuoto = stop
    std::function<float (int)> getLiveValue;
    std::function<char (int)>  getLiveRate;
    std::function<bool()>      isLiveFound;
    std::function<void (int)>  setLiveListen, setLiveScope;
    std::function<void (float*, int)> copyLiveScope;
    // Mappa dell'orchestra: quante note suona ogni strumento (facoltative)
    std::function<void (bool)> setActiveNotesWanted;
    std::function<int (const juce::String& instr)> getActiveNotes;

    void show()
    {
        refresh (true);
        setVisible (true);
        startLiveForBlock();
        if (setActiveNotesWanted) setActiveNotesWanted (currentBlock < 0 && ! explainMode);
        updateCrumbs();             // ora la vista e' visibile: i colori dei blocchi nell'editor
        if (docked)
        {
            setAlpha (1.0f);      // una chiusura precedente con dissolvenza la lascia trasparente
            toFront (false);      // agganciata: la tastiera resta all'editor, si continua a scrivere
            return;
        }
        toFront (true);
        juce::Desktop::getInstance().getAnimator().fadeIn (this, 140);
        grabKeyboardFocus();
    }

    // Agganciata (non modale): accanto all'editor, che resta usabile; si
    // aggiorna mentre si scrive. Altrimenti: a tutta finestra come la Guida.
    std::function<void (bool docked)> onDockChanged;
    std::function<void()> onVisibilityChanged;      // l'editor ridistribuisce lo spazio (vista agganciata)
    std::function<int()>  getCaretLine;             // riga del cursore dell'editor (da 1)
    void visibilityChanged() override { if (onVisibilityChanged) onVisibilityChanged(); }
    bool isDocked() const noexcept { return docked; }
    void setDocked (bool shouldDock)
    {
        docked = shouldDock;
        setWantsKeyboardFocus (! docked);      // agganciata: un clic sul grafo non toglie la tastiera all'editor
        dockButton.setDockedLook (docked);
        dockButton.setTooltip (docked ? "Show the view on the whole window" : "Dock the view beside the code: keep writing and watch the diagram follow");
        if (onDockChanged) onDockChanged (docked);
        resized();
        repaint();
    }

    /** Agganciata: scatola della riga del cursore dell'editor (senza popover). */
    void revealLine (int line)
    {
        // il cursore si e' mosso? (solo allora si puo' cambiare strumento: un
        // blocco scelto dal menu resta anche se il cursore e' in un altro)
        const bool caretMoved = line != lastCaretLine;
        lastCaretLine = line;
        if (! isVisible() || currentBlock < 0 || line == lastRevealedLine) return;
        lastRevealedLine = line;
        // il blocco giusto: se il cursore va in un altro strumento si passa a quello
        const auto& cur = orchestra.blocks[(size_t) currentBlock];
        if (line < cur.lineBegin || line > cur.lineEnd)
        {
            if (! caretMoved) return;
            for (int b = 0; b < (int) orchestra.blocks.size(); ++b)
            {
                const auto& g = orchestra.blocks[(size_t) b];
                if (g.kind != csoundGraph::Graph::Header && line >= g.lineBegin && line <= g.lineEnd) { showBlock (b); break; }
            }
        }
        selectLineInGraph (line);
    }

    void hide()
    {
        popover.hide();
        stopLive();                   // chiudendo la vista: niente piu' letture, suono normale
        if (setActiveNotesWanted) setActiveNotesWanted (false);
        if (onLineColours) onLineColours ({});
        if (docked)
        {
            setVisible (false);       // agganciata: subito, l'editor si riprende lo spazio
            return;
        }
        juce::Desktop::getInstance().getAnimator().fadeOut (this, 120);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (showsStepBar() && (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey))
        {
            step (key == juce::KeyPress::leftKey ? -1 : +1);
            return true;
        }
        if (key == juce::KeyPress::escapeKey)
        {
            if (cableInfo.isVisible()) closeCableInfo();
            else if (popover.isVisible()) popover.hide();
            else if (currentBlock >= 0) goUp();      // Esc: un livello su, poi chiude
            else                     hide();
            return true;
        }
        return false;
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == this && ! docked && closeBounds().contains (e.getPosition()))
            hide();
    }

    CsoundGraphView()
    {
        using namespace csoundGraphUI;
        setWantsKeyboardFocus (true);
        // Menu di navigazione: lo stesso foglio (CsoundActionSheet) di tutti
        // i menu di apeCsound, quindi usabile anche su iOS.
        navButton.setColour (juce::TextButton::buttonColourId, kBorder);
        navButton.setColour (juce::TextButton::textColourOffId, kText);
        navButton.setWantsKeyboardFocus (false);
        navButton.getProperties().set ("circular", true);    // pillola, come i bottoni di apeCsound
        navButton.setTooltip ("Choose what to show: the whole orchestra, an instrument or a user-defined opcode");
        navButton.onClick = [this] { showNavigationMenu(); };
        addAndMakeVisible (navButton);

        backButton.setButtonText (juce::String::fromUTF8 ("\xe2\x80\xb9 Blocks"));
        backButton.setColour (juce::TextButton::buttonColourId, kBorder);
        backButton.setColour (juce::TextButton::textColourOffId, kText);
        backButton.setWantsKeyboardFocus (false);
        backButton.getProperties().set ("circular", true);
        backButton.setTooltip ("Up one level: from a block to its instrument, from an instrument to the orchestra");
        backButton.onClick = [this] { goUp(); };
        addAndMakeVisible (backButton);

        info.setJustificationType (juce::Justification::centredRight);
        info.setColour (juce::Label::textColourId, kTextMuted);
        info.setMinimumHorizontalScale (0.7f);
        addAndMakeVisible (info);
        locationTitle.setJustificationType (juce::Justification::centred);
        locationTitle.setFont (csoundGraphUI::boldFont (16.0f));
        locationTitle.setColour (juce::Label::textColourId, kText);
        locationTitle.setMinimumHorizontalScale (0.7f);
        locationTitle.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (locationTitle);
        addChildComponent (crumbs);

        viewport.setViewedComponent (&flowCanvas, false);
        viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);   // dito / trascinamento
        addAndMakeVisible (viewport);

        addChildComponent (popover);

        // Flow / Detail: due pillole affiancate, quella attiva in teal
        for (auto* b : { &flowButton, &explainButton, &modeButton, &codeButton })
        {
            b->getProperties().set ("circular", true);
            b->setWantsKeyboardFocus (false);
            b->setColour (juce::TextButton::buttonColourId, kBorder);
            b->setColour (juce::TextButton::textColourOffId, kText);
            addAndMakeVisible (*b);
        }
        flowButton.setTooltip ("Flow: the algorithm - only what reaches the output, as a signal chain");
        flowButton.onClick   = [this] { setFlowMode (true); };
        explainButton.setTooltip ("Explain: the code with a sentence beside each line, saying what it does");
        explainButton.onClick = [this] { explainMode = ! explainMode; showBlock (currentBlock); };   // di nuovo: torna al grafo
        explainCanvas.onLineTapped = [this] (int line) { goToCode (line); };
        mapCanvas.onBlockTapped = [this] (int b) { explainMode = false; flowMode = true; showBlock (b); };
        modeButton.setVisible (false);
        modeButton.onClick = [this] { showModeMenu(); };

        // "Code": il codice accanto al grafo, sincronizzato (tocchi una riga
        // e si accende la sua scatola, tocchi una scatola e si accende la riga)
        codeButton.setTooltip ("Show the code beside the diagram: tap a line to find its box, tap a box to find its line");
        codeButton.onClick = [this] { splitMode = ! splitMode; showBlock (currentBlock); };
        codePane.setNotesVisible (false);
        codePane.onLineTapped = [this] (int line) { selectLineInGraph (line); };
        addChildComponent (cableInfo);
        cableInfo.onClosed = [this] { closeCableInfo(); };
        cableInfo.onListenToggled = [this]
        {
            const bool on = ! cableInfo.isListening();
            cableInfo.setListening (on);
            if (setLiveListen) setLiveListen (on ? liveVars.indexOf (cableInfo.getVariable()) : -1);
        };
        flowCanvas.onCableHovered = [this] (const juce::String& v) { hoverVar = v; };
        flowCanvas.onCableTapped = [this] (const juce::String& v, juce::Point<float> p) { openCableInfo (v, getLocalPoint (&flowCanvas, p).toInt()); };

        dockButton.setDockedLook (docked);
        dockButton.onClick = [this] { setDocked (! docked); };
        dockButton.setTooltip ("Show the view on the whole window");
        setWantsKeyboardFocus (false);   // agganciata (predefinito): la tastiera resta all'editor
        addAndMakeVisible (dockButton);

        // "Live": i valori su tutti i cavi, lo spessore dei cavi audio segue il livello
        liveButton.setButtonText ("Live");
        liveButton.getProperties().set ("circular", true);
        liveButton.setWantsKeyboardFocus (false);
        liveButton.setTooltip ("Show the live value on every cable while Csound plays (hover or tap a cable to see just one)");
        liveButton.onClick = [this] { liveState.all = ! liveState.all; updatePills(); pushLive(); };
        addAndMakeVisible (liveButton);
        codeViewport.setViewedComponent (&codePane, false);
        codeViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
        addChildComponent (codeViewport);

        // Flow: un tocco seleziona la scatola (e la sua riga nel codice accanto),
        // doppio tocco apre l'aiuto con il contesto
        flowCanvas.onNodeTapped       = [this] (int node)
        {
            popover.hide();
            syncCodeToNode (node);
            if (currentBlock >= 0 && node >= 0 && node < (int) orchestra.blocks[(size_t) currentBlock].nodes.size())
            {
                const auto& nd = orchestra.blocks[(size_t) currentBlock].nodes[(size_t) node];
                crumbLeaf = csoundGraphUI::js (nd.kind == csoundGraph::NodeKind::Expr && ! nd.outs.empty() ? nd.outs[0].var : nd.name);
                updateCrumbs();
                resized();
            }
        };
        flowCanvas.onNodeDoubleTapped = [this] (int node) { syncCodeToNode (node); showPopoverForNode (node, flowCanvas.getNodeBounds (node), &flowCanvas); };
        flowCanvas.onEmptyTapped = [this] { popover.hide(); };
        flowCanvas.onStageTapped = [this] (int stage) { openStage (stage); };

        // Patch (come Max / Pure Data), voce temporanea del menu della vista
        patcher.onNodeTapped       = [this] (int node) { popover.hide(); syncCodeToNode (node); };
        patcher.onNodeDoubleTapped = [this] (int node) { syncCodeToNode (node); showPopoverForNode (node, patcher.getNodeBounds (node), &patcher); };
        patcher.onBlockOpen        = [this] (int b) { explainMode = false; showBlock (b); };
        patcher.onEmptyTapped      = [this] { popover.hide(); };
        patcher.onCableHovered     = [this] (const juce::String& v) { hoverVar = v; };
        patcher.onCableTapped      = [this] (const juce::String& v, juce::Point<float> p) { openCableInfo (v, getLocalPoint (&patcher, p).toInt()); };

        // Passo passo (vista Detail): le righe del blocco una alla volta,
        // nell'ordine in cui Csound le esegue, con il testo della riga.
        for (auto* b : { &prevStep, &nextStep })
        {
            b->getProperties().set ("circular", true);
            b->setWantsKeyboardFocus (false);
            b->setColour (juce::TextButton::buttonColourId, kBorder);
            b->setColour (juce::TextButton::textColourOffId, kText);
            addChildComponent (*b);
        }
        prevStep.setButtonText (u8 ("\xe2\x97\x80"));
        nextStep.setButtonText (u8 ("\xe2\x96\xb6"));
        prevStep.setTooltip ("Previous step (Left arrow)");
        nextStep.setTooltip ("Next step, in the order Csound runs the lines (Right arrow)");
        prevStep.onClick = [this] { step (-1); };
        nextStep.onClick = [this] { step (+1); };
        stepLabel.setColour (juce::Label::textColourId, kText);
        stepLabel.setFont (plainFont (12.5f));
        stepLabel.setMinimumHorizontalScale (0.6f);
        addChildComponent (stepLabel);

        startTimerHz (15);
    }

    ~CsoundGraphView() override
    {
        stopTimer();
        if (setLiveVariables) setLiveVariables ({}, {});   // editor chiuso con la vista aperta: il motore smette di leggere
    }

    /** Rilegge il testo e ricostruisce il grafo (solo se il testo e' cambiato). */
    void refresh (bool force = false)
    {
        if (! getText) return;
        const auto text = getText();
        if (! force && built && text == lastText) return;
        lastText = text;
        built = true;
        sourceLines.clear();
        sourceLines.addLines (text);

        int liveCount = 0;
        juce::Array<CsoundLiveOpcodeInfo> live;
        if (getOpcodes) { live = getOpcodes(); liveCount = live.size(); }
        if (! tableReady || liveCount != lastLiveCount)
        {
            table = csoundGraph::OpcodeTable();
            liveInfo.clear();
            for (const auto& h : CsoundOpcodeHelpData::table) table.add (h.name, h.syntax);
            for (const auto& o : live)
            {
                table.addName (o.name.toStdString());
                if (liveInfo.find (o.name) == liveInfo.end()) liveInfo[o.name] = o;
            }
            tableReady = true;
            lastLiveCount = liveCount;
        }

        const auto previous = currentTitle;
        orchestra = csoundGraph::parse (text.toStdString(), table);
        explainCacheValid = false;

        int firstInstr = -1, keep = -1;
        for (size_t b = 0; b < orchestra.blocks.size(); ++b)
        {
            const auto& g = orchestra.blocks[b];
            if (b == 0 && g.nodes.empty()) continue;
            if (firstInstr < 0 && g.kind == csoundGraph::Graph::Instr) firstInstr = (int) b;
            if (csoundGraphUI::js (g.title()) == previous) keep = (int) b;
        }
        showBlock (keep >= 0 ? keep : (previous == "Orchestra" ? -1 : firstInstr));
    }

    void resized() override
    {
        const auto area = safeArea();
        // Intestazione a pila, come in iOS:   [< genitore]   titolo   [vista v] [Dock] [x]
        // La radice e' l'orchestra (Map). Un tocco nel diagramma entra (strumento,
        // blocco), il pulsante a sinistra torna al livello sopra e dice quale.
        for (auto* b : { &flowButton, &explainButton, &codeButton, &liveButton })
            b->setVisible (false);
        navButton.setVisible (false);
        crumbs.setVisible (false);
        info.setVisible (false);
        dockButton.setBounds (dockBounds());
        auto head = juce::Rectangle<int> (area.getX() + 12, area.getY() + 14, juce::jmax (0, dockBounds().getX() - 10 - area.getX() - 12), 28);

        const auto fullHead = head;
        // vista (a destra, prima di Dock): Blocks / Explain e le opzioni
        modeButton.setVisible (true);
        modeButton.setButtonText ((explainMode ? juce::String ("Explain") : patchMode ? juce::String ("Patch") : currentBlock < 0 ? juce::String ("Map")
                                   : flowCanvas.hasOverview() ? juce::String ("Blocks") : juce::String ("Flow"))
                                  + csoundGraphUI::u8 (" \xe2\x96\xbe"));
        modeButton.setTooltip ("Diagram or Explain, and their options (code beside the diagram, live values)");
        const int modeW = juce::jmin (120, juce::jmax (70, head.getWidth() / 4));
        modeButton.setBounds (head.removeFromRight (modeW));

        // indietro (a sinistra), solo sotto la radice
        const bool showBack = currentBlock >= 0;
        backButton.setButtonText (csoundGraphUI::u8 ("\xe2\x80\xb9 ") + (belowBlocks() ? currentTitle : juce::String ("Orchestra")));
        backButton.setVisible (showBack);
        const int bw = (int) std::ceil (csoundGraphUI::textWidth (csoundGraphUI::plainFont (15.0f), backButton.getButtonText())) + 28;
        const int backW = showBack ? juce::jmin (juce::jmax (90, bw), juce::jmax (90, fullHead.getWidth() / 3)) : 0;
        if (showBack) backButton.setBounds (fullHead.withWidth (backW));

        // titolo: dove sei, centrato nell'intestazione (come in iOS); se lo spazio
        // non basta, nello spazio fra i due bottoni
        auto titleArea = fullHead.reduced (juce::jmax (backW, modeW) + 10, 0);
        if (titleArea.getWidth() < 120)
            titleArea = fullHead.withTrimmedLeft (backW + 8).withTrimmedRight (modeW + 8);
        locationTitle.setBounds (titleArea);
        locationTitle.setVisible (titleArea.getWidth() > 40);
        locationTitle.setText (locationText(), juce::dontSendNotification);

        if (explainMode && explainCanvas.getWidth() != juce::jmax (320, bodyArea().getWidth() - 12))
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<CsoundGraphView> (this)] { if (safe) safe->showBlock (safe->currentBlock); });

        viewport.setBounds (bodyArea());
        codeViewport.setVisible (showsCodePane());
        if (showsCodePane())
        {
            auto body = bodyArea();
            codeViewport.setBounds (body.removeFromLeft (juce::jmax (260, body.getWidth() * 42 / 100)));
            body.removeFromLeft (8);
            viewport.setBounds (body);
        }

        flowCanvas.viewportResized();
        patcher.viewportResized();
        const bool stepBar = showsStepBar();
        prevStep.setVisible (stepBar);
        nextStep.setVisible (stepBar);
        stepLabel.setVisible (stepBar);
        if (stepBar)
        {
            auto bar = stepBarArea().reduced (10, 7);
            prevStep.setBounds (bar.removeFromLeft (52));
            bar.removeFromLeft (6);
            nextStep.setBounds (bar.removeFromLeft (52));
            bar.removeFromLeft (12);
            stepLabel.setBounds (bar);
        }
        if (popover.isVisible()) popover.hide();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace csoundGraphUI;
        g.fillAll (kCardBg);                                  // a tutto schermo, come la Guida
        if (docked)
        {
            g.setColour (kBorder);
            g.fillRect (0, 0, 2, getHeight());                // bordo verso l'editor
        }
        const auto area = safeArea();

        if (! docked && area.getWidth() >= 720)
        {
            g.setColour (kText);
            g.setFont (titleFont());
            juce::ignoreUnused (area);   // il titolo e' quello del livello (locationTitle)
        }

        g.setColour (kBorder);
        g.drawLine ((float) area.getX() + 16.0f, (float) area.getY() + 54.5f, (float) area.getRight() - 16.0f, (float) area.getY() + 54.5f, 1.0f);

        g.setColour (kBg);                                    // il grafo su fondo piu' scuro
        g.fillRoundedRectangle (bodyArea().toFloat(), 8.0f);
        if (showsStepBar())
        {
            g.setColour (kBar);
            g.fillRoundedRectangle (stepBarArea().toFloat(), 8.0f);
        }

        if (docked) return;           // agganciata: niente x, come gli altri pannelli (si chiude dal menu o con Cmd+6)
        const auto close = closeBounds().toFloat();
        g.setColour (kTextMuted);
        juce::Path x;
        x.startNewSubPath (close.getX() + 9.0f, close.getY() + 9.0f); x.lineTo (close.getRight() - 9.0f, close.getBottom() - 9.0f);
        x.startNewSubPath (close.getRight() - 9.0f, close.getY() + 9.0f); x.lineTo (close.getX() + 9.0f, close.getBottom() - 9.0f);
        g.strokePath (x, juce::PathStrokeType (1.8f));
    }

private:
    static juce::Font titleFont() { return juce::Font (juce::FontOptions (20.0f, juce::Font::bold)); }

    // Come GuideView: su iOS si resta dentro l'area sicura (notch, barra di
    // stato, indicatore home), togliendo solo la parte che ci cade davvero.
    juce::Rectangle<int> safeArea() const
    {
        auto area = getLocalBounds();
       #if JUCE_IOS
        if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        {
            const auto safeOnScreen = display->safeAreaInsets.subtractedFrom (display->totalArea);
            const auto safeLocal = getLocalArea (nullptr, safeOnScreen).getIntersection (area);
            if (! safeLocal.isEmpty()) area = safeLocal;
        }
       #endif
        return area;
    }
    juce::Rectangle<int> closeBounds() const { const auto a = safeArea(); return { a.getRight() - 16 - 28, a.getY() + 14, 28, 28 }; }
    juce::Rectangle<int> dockBounds() const  { const auto c = closeBounds(); return docked ? c : juce::Rectangle<int> { c.getX() - 8 - 28, c.getY(), 28, 28 }; }
    static constexpr int stepBarH = 58;
    bool showsStepBar() const                { return currentBlock >= 0 && explainMode; }
    bool showsCodePane() const               { return splitMode && currentBlock >= 0 && ! explainMode && getWidth() >= 640; }

    // codice accanto al grafo: riga di un nodo evidenziata e portata in vista
    void syncCodeToNode (int node)
    {
        if (! showsCodePane() || currentBlock < 0) return;
        const auto& g = orchestra.blocks[(size_t) currentBlock];
        if (node < 0 || node >= (int) g.nodes.size()) return;
        const int line = g.nodes[(size_t) node].line;
        codePane.setHighlightedLine (line);
        const auto r = codePane.getLineBounds (line);
        if (! r.isEmpty()) codeViewport.setViewPosition (0, juce::jmax (0, r.getCentreY() - codeViewport.getHeight() / 2));
    }

    // tocco su una riga del codice: la sua scatola nel grafo
    void selectLineInGraph (int line)
    {
        if (currentBlock < 0) return;
        codePane.setHighlightedLine (line);
        const auto& g = orchestra.blocks[(size_t) currentBlock];
        popover.hide();
        if (patchMode)
        {
            for (int i = 0; i < (int) g.nodes.size(); ++i)
            {
                if (g.nodes[(size_t) i].line != line || ! patcher.selectNode (i)) continue;
                const auto r = patcher.getNodeBounds (i).toNearestInt();
                viewport.setViewPosition (juce::jmax (0, r.getCentreX() - viewport.getWidth() / 2), juce::jmax (0, r.getCentreY() - viewport.getHeight() / 2));
                return;
            }
            patcher.clearSelection();
            return;
        }
        for (int i = 0; i < (int) g.nodes.size(); ++i)
        {
            if (g.nodes[(size_t) i].line != line || ! flowCanvas.selectNode (i)) continue;
            const auto r = flowCanvas.getNodeBounds (i).toNearestInt();
            viewport.setViewPosition (juce::jmax (0, r.getCentreX() - viewport.getWidth() / 2), juce::jmax (0, r.getCentreY() - viewport.getHeight() / 2));
            return;
        }
        flowCanvas.clearSelection();   // riga non nel flusso (non arriva all'uscita)
    }
    juce::Rectangle<int> bodyArea() const
    {
        auto r = safeArea().withTrimmedTop (62).reduced (16, 0).withTrimmedBottom (16);
        if (showsStepBar()) r.removeFromBottom (stepBarH + 8);
        return r;
    }
    juce::Rectangle<int> stepBarArea() const
    {
        return safeArea().withTrimmedTop (62).reduced (16, 0).withTrimmedBottom (16).removeFromBottom (stepBarH);
    }
    // --- valori dal vivo sui cavi -------------------------------------------
    // le variabili locali dei cavi del blocco (le globali vivono altrove)
    void startLiveForBlock()
    {
        liveVars.clear();
        if (! setLiveVariables) return;
        if (currentBlock < 0 || ! isVisible()) { stopLive(); return; }
        const auto& g = orchestra.blocks[(size_t) currentBlock];
        if (g.kind != csoundGraph::Graph::Instr || g.names.empty()) { stopLive(); return; }
        for (const auto& c : g.cables)
        {
            const auto v = csoundGraphUI::js (c.var);
            if (v.isNotEmpty() && ! v.startsWithChar ('g') && (c.rate == 'a' || c.rate == 'k' || c.rate == 'i'))
                liveVars.addIfNotAlreadyThere (v);
        }
        // anche le variabili scritte e mai lette (scatole "non collegate")
        for (const auto& nd : g.nodes)
            for (const auto& o : nd.outs)
            {
                const auto v = csoundGraphUI::js (o.var);
                if (v.isNotEmpty() && ! o.global && ! o.isArray && (o.rate == 'a' || o.rate == 'k' || o.rate == 'i'))
                    liveVars.addIfNotAlreadyThere (v);
            }
        while (liveVars.size() > 256) liveVars.remove (liveVars.size() - 1);   // = CsoundAudioProcessor::kMaxLiveVars
        liveInstrName = csoundGraphUI::js (g.names[0]);
        liveRequested = false;        // il motore legge solo quando serve (vedi readLive)
        if (setLiveVariables) setLiveVariables ({}, {});
    }

    void updatePills()
    {
        liveButton.setColour (juce::TextButton::buttonColourId, liveState.all ? csoundGraphUI::kAccent : csoundGraphUI::kBorder);
        liveButton.setColour (juce::TextButton::textColourOffId, liveState.all ? juce::Colour (0xff0b1116) : csoundGraphUI::kText);
    }

    void stopLive()
    {
        closeCableInfo();
        liveRequested = false;
        hoverVar.clear();
        if (setLiveVariables) setLiveVariables ({}, {});
        liveVars.clear();
        liveState.text.clear();
        liveState.level.clear();
        pushLive();
    }

    void pushLive()
    {
        flowCanvas.setLiveCables (liveState);
        patcher.setLiveCables (liveState);
    }

    void readLive()
    {
        if (liveVars.isEmpty() || ! getLiveValue || ! getLiveRate || ! setLiveVariables) return;

        // Il motore legge le variabili SOLO quando qualcuno le guarda: Live
        // acceso, il mouse sopra un cavo, la finestra di un cavo aperta.
        // Altrimenti (e a vista chiusa) il thread audio non fa nulla in piu'.
        const bool needed = liveState.all || hoverVar.isNotEmpty() || cableInfo.isVisible() || tourVar.isNotEmpty();
        if (needed != liveRequested)
        {
            liveRequested = needed;
            if (needed) setLiveVariables (liveInstrName, liveVars);
            else
            {
                setLiveVariables ({}, {});
                liveState.text.clear();
                liveState.level.clear();
                pushLive();
            }
        }
        if (! liveRequested) return;

        const bool found = isLiveFound && isLiveFound();
        // dopo un Apply il motore e' nuovo e le letture sono state tolte:
        // senza note trovate si richiede di nuovo, circa ogni secondo
        if (! found && ! cableInfo.isListening() && (++relink % 15) == 0)
            setLiveVariables (liveInstrName, liveVars);
        liveState.text.clear();
        liveState.level.clear();
        if (found)
            for (int i = 0; i < liveVars.size(); ++i)
            {
                const char r = getLiveRate (i);
                if (r == 0) continue;
                const float v = getLiveValue (i);
                if (r == 'a')
                {
                    liveState.level[liveVars[i]] = v;
                    liveState.text[liveVars[i]] = v > 1.0e-5f ? juce::String (20.0f * std::log10 (v), 1) + " dB" : csoundGraphUI::u8 ("-\xe2\x88\x9e dB");
                }
                else
                {
                    auto t = juce::String (v, 3);
                    if (t.containsChar ('.')) t = t.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
                    liveState.text[liveVars[i]] = t == "-0" ? juce::String ("0") : t;
                }
            }
        pushLive();
        if (tourVar.isNotEmpty()) updateTourLabel();
        if (cableInfo.isVisible())
        {
            const auto& v = cableInfo.getVariable();
            std::array<float, 256> buf {};
            const bool audio = cableInfo.getRate() == 'a' && copyLiveScope;
            if (audio) copyLiveScope (buf.data(), (int) buf.size());
            cableInfo.update (liveState.text.count (v) ? liveState.text[v] : juce::String ("-"), found && liveState.text.count (v) > 0,
                              audio ? buf.data() : nullptr, (int) buf.size());
        }
    }

    void openCableInfo (const juce::String& var, juce::Point<int> near)
    {
        popover.hide();
        int index = liveVars.indexOf (var);
        if (index < 0 && var.isNotEmpty() && ! var.startsWithChar ('g') && liveVars.size() > 0)
        {
            // non fra quelle lette (oltre il limite): prende il posto dell'ultima
            liveVars.set (liveVars.size() - 1, var);
            index = liveVars.size() - 1;
            liveRequested = false;          // readLive la richiede di nuovo al motore
        }
        if (index < 0)
        {
            cableInfo.setVisible (false);
            return;
        }
        if (cableInfo.isListening() && setLiveListen) setLiveListen (-1);
        const char r = getLiveRate ? getLiveRate (index) : 0;
        // una variabile riscritta su piu' righe ha una sola memoria: si legge
        // (e si ascolta) il valore dopo l'ultima riga che la scrive
        juce::String note;
        if (currentBlock >= 0)
        {
            juce::StringArray lines;
            int last = 0;
            for (const auto& nd : orchestra.blocks[(size_t) currentBlock].nodes)
                for (const auto& o : nd.outs)
                    if (csoundGraphUI::js (o.var) == var && nd.line > 0) { lines.addIfNotAlreadyThere (juce::String (nd.line)); last = juce::jmax (last, nd.line); }
            if (lines.size() > 1)
                note = var + " is rewritten on lines " + lines.joinIntoString (", ") + ": one variable, one memory, so you see and hear its value after line "
                     + juce::String (last) + ". To follow each step, give each step its own name.";
        }
        cableInfo.show (var, r != 0 ? r : (var.startsWithChar ('a') ? 'a' : 'k'), near, note);
        liveState.selected = var;
        if (setLiveScope) setLiveScope (cableInfo.getRate() == 'a' ? index : -1);
        pushLive();
    }

    void closeCableInfo()
    {
        if (cableInfo.isListening() && setLiveListen) setLiveListen (-1);
        cableInfo.setListening (false);
        cableInfo.setVisible (false);
        if (setLiveScope) setLiveScope (-1);
        liveState.selected.clear();
        pushLive();
    }

    // Visita guidata (Explain): le righe nell'ordine in cui Csound le
    // esegue - prima il passaggio di inizializzazione (le righe i, una volta
    // quando nasce la nota), poi il ciclo di controllo che si ripete (righe
    // k e a, nell'ordine del file). Ogni passo accende la riga, dice cosa fa
    // e, mentre Csound suona, il valore della sua uscita.
    std::vector<int> tourOrder() const
    {
        std::vector<int> order;
        if (currentBlock < 0) return order;
        const auto& g = orchestra.blocks[(size_t) currentBlock];
        std::set<int> seen;
        for (int pass = 0; pass < 2; ++pass)
        {
            std::vector<int> part;
            for (int i = 0; i < (int) g.nodes.size(); ++i)
            {
                const auto& nd = g.nodes[(size_t) i];
                const char r = csoundExplain::timing::rateOf (nd);
                if (r == 0 || nd.line <= 0 || (pass == 0) != (r == 'i')) continue;
                part.push_back (i);
            }
            std::stable_sort (part.begin(), part.end(), [&g] (int x, int y) { return g.nodes[(size_t) x].line < g.nodes[(size_t) y].line; });
            for (int i : part)
                if (seen.insert (g.nodes[(size_t) i].line).second) order.push_back (i);
        }
        return order;
    }

    void step (int dir)
    {
        if (currentBlock < 0 || ! explainMode) return;
        const auto order = tourOrder();
        if (order.empty()) return;
        stepIndex = juce::jlimit (0, (int) order.size() - 1, stepIndex < 0 ? 0 : stepIndex + dir);
        const auto& g = orchestra.blocks[(size_t) currentBlock];
        const auto& nd = g.nodes[(size_t) order[(size_t) stepIndex]];
        explainCanvas.setHighlightedLine (nd.line);
        const auto r = explainCanvas.getLineBounds (nd.line);
        if (! r.isEmpty()) viewport.setViewPosition (0, juce::jmax (0, r.getCentreY() - viewport.getHeight() / 3));
        const char rate = csoundExplain::timing::rateOf (nd);
        tourVar = nd.outs.empty() ? juce::String() : csoundGraphUI::js (nd.outs[0].var);
        const auto dot = csoundGraphUI::u8 ("  \xc2\xb7  ");
        tourHead = juce::String (stepIndex + 1) + "/" + juce::String ((int) order.size()) + dot
                 + (rate == 'i' ? juce::String ("once, at note start") : rate == 'k' ? juce::String ("repeats at control rate") : juce::String ("repeats on every sample"))
                 + dot + "line " + juce::String (nd.line) + "   " + sourceText (nd.line, nd.line);
        const auto& ex = explanation();
        auto it = ex.notes.find (nd.line);
        tourNote = it != ex.notes.end() ? it->second : juce::String();
        updateTourLabel();
        prevStep.setEnabled (stepIndex > 0);
        nextStep.setEnabled (stepIndex < (int) order.size() - 1);
    }

    // le spiegazioni: ricalcolate solo quando il testo cambia (il passo-passo
    // e ogni showBlock le usano; su un csd grande costano decine di ms)
    const csoundExplain::Explanation& explanation()
    {
        if (! explainCacheValid || explainCacheText != lastText)
        {
            explainCache = csoundExplain::explain (orchestra);
            explainCacheText = lastText;
            explainCacheValid = true;
        }
        return explainCache;
    }
    csoundExplain::Explanation explainCache;
    juce::String explainCacheText;
    bool explainCacheValid = false;

    // Sotto i blocchi dello strumento: un blocco aperto o tutte le scatole (Flow)
    bool belowBlocks() const { return ! explainMode && ! patchMode && flowCanvas.getFocusStage() >= 0; }

    // Un livello su, sempre lo stesso percorso:
    // scatole (un blocco o tutto il Flow) -> blocchi dello strumento -> orchestra
    void goUp()
    {
        if (currentBlock < 0) return;
        if (belowBlocks()) { setOverview (true); return; }
        showBlock (-1);
    }

    juce::String currentTitleFor (int b) const
    {
        return b >= 0 && b < (int) orchestra.blocks.size() ? csoundGraphUI::js (orchestra.blocks[(size_t) b].title()) : juce::String ("Orchestra");
    }

    // vista d'insieme (blocchi) <-> Flow
    void setOverview (bool on)
    {
        popover.hide();
        closeCableInfo();
        flowCanvas.setOverview (on);
        if (on) viewport.setViewPosition (0, 0);
        crumbLeaf.clear();
        updateCrumbs();
        resized();
    }
    void openStage (int stage)
    {
        flowCanvas.setOverview (false);
        flowCanvas.setFocusStage (stage);
        info.setText (juce::String (flowCanvas.getNumBoxes()) + " boxes", juce::dontSendNotification);
        crumbLeaf.clear();
        updateCrumbs();
        resized();
        viewport.setViewPosition (0, 0);
    }

    void updateTourLabel()
    {
        if (stepIndex < 0) return;
        juce::String second = tourNote;
        if (tourVar.isNotEmpty() && liveState.text.count (tourVar))
            second = tourVar + " now: " + liveState.text[tourVar] + (second.isEmpty() ? juce::String() : "   " + second);
        stepLabel.setText (tourHead + "\n" + second, juce::dontSendNotification);
    }

    juce::TextButton navButton, backButton;
    juce::TextButton flowButton { "Flow" }, explainButton { "Explain" }, modeButton;
    CsoundExplainCanvas explainCanvas;
    juce::TextButton codeButton { "Code" };
    CsoundGraphCableInfo cableInfo;
    int tick = 0;
    CsoundGraphDockButton dockButton;
    bool docked = true;            // si apre agganciata accanto all'editor (Full: a tutta finestra)
    int lastRevealedLine = -1;
    int lastCaretLine = -1;
    juce::Rectangle<int> flowStart;   // Flow: inizio della catena audio (vedi showBlock)
    juce::String lastPlacedView;      // blocco + vista dell'ultimo posizionamento dello scroll        // ultima riga del cursore vista (per cambiare strumento solo se si muove)
    juce::TextButton liveButton;
    csoundGraphUI::LiveCables liveState;
    juce::StringArray liveVars;                // indice = indice nel processor
    juce::String liveInstrName;
    juce::String hoverVar;                     // cavo sotto il mouse (Flow o Detail)
    bool liveRequested = false;                // il motore sta leggendo le variabili
    int relink = 0;
    CsoundExplainCanvas codePane;
    csoundGraphUI::ScrollViewport codeViewport;
    bool splitMode = false;
    bool explainMode = false;      // il codice con le note in parole (anche per l'orchestra intera)
    CsoundGraphFlowCanvas flowCanvas;
    CsoundPatcherCanvas patcher;   // vista Patch (prova, temporanea): un patch come in Max / Pure Data
    bool patchMode = false;        // la vista Patch al posto di Map / Blocks / Flow
    bool patchAll = false;         // Patch: anche il calcolo di controllo (altrimenti solo il percorso del suono)
    bool flowMode = true;          // vista Flusso predefinita negli strumenti
    juce::TextButton prevStep, nextStep;
    juce::Label stepLabel;
    int stepIndex = -1;
    juce::String tourVar, tourHead, tourNote;    // visita guidata: uscita della riga, testo
    bool groupByTime = false;                    // Explain: righe raggruppate per i / k / a
    juce::Label info;
    juce::Label locationTitle;
    CsoundGraphCrumbs crumbs;
    juce::String crumbLeaf;                       // la scatola selezionata (ultima voce del percorso)
public:
    /** Colori dei blocchi per il margine del code editor (riga da 0); vuoto = niente. */
    std::function<void (const std::map<int, juce::Colour>&)> onLineColours;
private:
    void updateCrumbs()
    {
        std::vector<CsoundGraphCrumbs::Item> items;
        if (currentBlock >= 0 && ! explainMode)
        {
            juce::Component::SafePointer<CsoundGraphView> safe (this);
            items.push_back ({ "Orchestra", [safe] { if (safe) safe->showBlock (-1); } });
            items.push_back ({ currentTitle, flowCanvas.hasOverview() ? std::function<void()> ([safe] { if (safe) safe->setOverview (true); }) : std::function<void()>() });
            const int st = flowCanvas.getFocusStage();
            if (st >= 0 && ! flowCanvas.isOverview())
                items.push_back ({ flowCanvas.getStageTitle (st), [safe, st] { if (safe) safe->openStage (st); } });
            if (crumbLeaf.isNotEmpty() && ! flowCanvas.isOverview()) items.push_back ({ crumbLeaf, {} });
        }
        crumbs.setItems (std::move (items));
        updateNavText();
        const bool blocks = ! patchMode && (flowCanvas.isOverview() || flowCanvas.getFocusStage() >= 0);      // colori solo se si usano i blocchi
        if (onLineColours) onLineColours (isVisible() && currentBlock >= 0 && ! explainMode && blocks ? flowCanvas.getLineColours() : std::map<int, juce::Colour>());
    }
    csoundGraphUI::ScrollViewport viewport;
    CsoundOrchestraMapCanvas mapCanvas;
    CsoundGraphPopover popover;
    csoundGraph::Orchestra orchestra;
    csoundGraph::OpcodeTable table;
    std::map<juce::String, CsoundLiveOpcodeInfo> liveInfo;
    juce::StringArray sourceLines;
    juce::String lastText, currentTitle = "?";
    int currentBlock = -1;
    bool built = false, tableReady = false;
    int lastLiveCount = -1;

    //--------------------------------------------------------------------------
    void showBlock (int b)
    {
        popover.hide();
        if (b >= (int) orchestra.blocks.size()) b = -1;
        currentBlock = b;
        lastRevealedLine = -1;          // grafo ricostruito: la riga del cursore va ri-evidenziata
        stepIndex = -1;
        tourVar.clear();
        stepLabel.setText (csoundGraphUI::u8 ("Follow the code in the order Csound runs it:  \xe2\x96\xb6  or the Right arrow\n")
                           + "first what happens once when a note starts, then what repeats while it plays", juce::dontSendNotification);
        prevStep.setEnabled (false);
        nextStep.setEnabled (true);

        const bool patchView = patchMode && ! explainMode;
        if (patchView)
        {
            if (b >= 0)
            {
                auto pf = csoundFlow::build (orchestra, b, &sourceLines);
                patcher.setPatch (csoundPatch::fromFlow (pf, orchestra.blocks[(size_t) b], &sourceLines, ! patchAll));
            }
            else
                patcher.setPatch (csoundPatch::fromOrchestra (orchestra, &sourceLines));
        }
        const bool flow = b >= 0 && flowMode && ! explainMode && ! patchView;
        int shown = 0, hidden = 0;
        if (flow)
        {
            auto f = csoundFlow::build (orchestra, b, &sourceLines);
            shown = (int) f.boxes.size(); hidden = f.hidden;
            const auto keyNow = currentTitleFor (b) + "/flow";
            flowCanvas.setFlow (std::move (f));
            // un altro strumento: si parte sempre dai blocchi (se ce ne sono almeno due);
            // mentre si scrive resta dov'era
            if (keyNow != lastPlacedView) { flowCanvas.setFocusStage (-1); flowCanvas.setOverview (true); }
            else flowCanvas.setOverview (flowCanvas.isOverview());
            // schema piu' grande della vista: si parte dall'inizio della catena
            // audio (sopra possono esserci decine di righe di controlli)
            flowStart = flowCanvas.getStartBounds().toNearestInt();
        }
        if (explainMode)
        {
            const auto& ex = explanation();
            int first = 1, last = sourceLines.size();
            if (b >= 0) { first = orchestra.blocks[(size_t) b].lineBegin; last = orchestra.blocks[(size_t) b].lineEnd; }
            explainCanvas.setGroupedByTime (groupByTime && b >= 0);
            explainCanvas.setContent (sourceLines, ex, first, last, juce::jmax (320, bodyArea().getWidth() - 12));
            shown = (int) ex.notes.size();
        }
        const bool map = b < 0 && ! explainMode && ! patchView;
        if (map) mapCanvas.setOrchestra (orchestra, explanation());
        if (setActiveNotesWanted) setActiveNotesWanted ((map || (patchView && b < 0)) && isVisible());
        if (! map) mapCanvas.setActiveCounts ({});
        if (! (patchView && b < 0)) patcher.setActiveCounts ({});
        viewport.setViewedComponent (explainMode ? static_cast<juce::Component*> (&explainCanvas)
                                     : patchView ? static_cast<juce::Component*> (&patcher)
                                     : flow ? static_cast<juce::Component*> (&flowCanvas)
                                     : static_cast<juce::Component*> (&mapCanvas), false);
        auto pill = [] (juce::TextButton& bt, bool on)
        {
            bt.setColour (juce::TextButton::buttonColourId, on ? csoundGraphUI::kAccent : csoundGraphUI::kBorder);
            bt.setColour (juce::TextButton::textColourOffId, on ? juce::Colour (0xff0b1116) : csoundGraphUI::kText);
        };
        pill (flowButton, flow);
        pill (explainButton, explainMode);
        pill (codeButton, splitMode);
        pill (liveButton, liveState.all);
        closeCableInfo();
        startLiveForBlock();
        if (splitMode && b >= 0 && ! explainMode)
        {
            const auto& gb = orchestra.blocks[(size_t) b];
            codePane.setContent (sourceLines, {}, gb.lineBegin, gb.lineEnd, juce::jmax (260, bodyArea().getWidth() * 42 / 100 - 12));
            codePane.setHighlightedLine (0);
        }
        currentTitle = b >= 0 ? csoundGraphUI::js (orchestra.blocks[(size_t) b].title()) : juce::String ("Orchestra");
        updateNavText();

        juce::String t;
        const auto dot = csoundGraphUI::u8 ("  \xc2\xb7  ");   // non ASCII: mai come const char* (jassert di juce::String)
        if (explainMode)
            t << "Tap a line to go to the code";
        else if (flow)
            t << shown << " boxes" + dot << hidden << " hidden";
        else if (b >= 0)
        {
            const auto& g = orchestra.blocks[(size_t) b];
            t << (int) g.nodes.size() << " boxes" + dot << (int) g.cables.size() << " cables";
            if (! g.frames.empty()) t << dot << (int) g.frames.size() << " frames";
            if (! g.warnings.empty()) t << dot << (int) g.warnings.size() << " warnings";
        }
        else
            t << mapCanvas.instrumentCount() << " instruments" + dot << "tap one to open it";
        if (orchestra.unparsed > 0) t << dot << orchestra.unparsed << " lines not understood";
        info.setText (t, juce::dontSendNotification);
        if (currentTitle + (flow ? "/flow" : "/other") != lastPlacedView) crumbLeaf.clear();
        updateCrumbs();
        resized();
        // solo quando si apre un altro blocco (non a ogni ricostruzione mentre si scrive)
        const auto key = currentTitle + (patchView ? "/patch" : flow ? "/flow" : "/other");
        if (patchView && key != lastPlacedView)
            viewport.setViewPosition (0, 0);          // un patch si legge dall'alto
        else if (flow && key != lastPlacedView && flowCanvas.isOverview())
            viewport.setViewPosition (0, 0);
        else if (flow && key != lastPlacedView)
        {
            if (! flowStart.isEmpty() && (flowStart.getBottom() > viewport.getHeight() || flowStart.getRight() > viewport.getWidth()))
                viewport.setViewPosition (juce::jmax (0, flowStart.getX() - 40), juce::jmax (0, flowStart.getCentreY() - viewport.getHeight() / 2));
            else
                viewport.setViewPosition (0, 0);
        }
        lastPlacedView = key;
    }

    // Il bottone di navigazione dice dove sei: "instr 10", "instr 10 > Spectral processing"
    juce::String locationText() const
    {
        if (currentBlock < 0) return explainMode ? juce::String ("Orchestra (the whole file)") : juce::String ("Orchestra");
        if (belowBlocks()) return flowCanvas.getStageTitle (flowCanvas.getFocusStage());
        return currentTitle;
    }

    void updateNavText()
    {
        auto t = currentTitle;
        if (currentBlock >= 0 && ! explainMode && flowCanvas.getFocusStage() >= 0)
            t << csoundGraphUI::u8 (" \xe2\x80\xba ") << flowCanvas.getStageTitle (flowCanvas.getFocusStage());
        navButton.setButtonText (t + csoundGraphUI::u8 (" \xe2\x96\xbe"));
        navButton.setTooltip ("Where you are: the orchestra, an instrument or one of its blocks - tap to go anywhere");
    }

    // Tutta la navigazione in un menu: l'orchestra, i blocchi dello strumento
    // aperto, gli altri strumenti. Lineare: Orchestra > strumento > blocco.
    void showNavigationMenu()
    {
        std::vector<CsoundActionSheetItem> items;
        auto add = [&items] (int id, const juce::String& text, bool ticked)
        {
            CsoundActionSheetItem it;
            it.id = id; it.text = text; it.ticked = ticked;
            items.push_back (it);
        };
        auto header = [&items] (const juce::String& text)
        {
            CsoundActionSheetItem it;
            it.sectionHeader = text;
            items.push_back (it);
        };

        add (1, "Orchestra", currentBlock < 0);
        for (auto kind : { csoundGraph::Graph::Instr, csoundGraph::Graph::Udo, csoundGraph::Graph::Header })
        {
            bool first = true;
            for (size_t b = 0; b < orchestra.blocks.size(); ++b)
            {
                const auto& g = orchestra.blocks[b];
                if (g.kind != kind || (b == 0 && g.nodes.empty())) continue;
                if (first)
                {
                    header (kind == csoundGraph::Graph::Instr ? "Instruments" : kind == csoundGraph::Graph::Udo ? "User-defined opcodes" : "Global code");
                    first = false;
                }
                add ((int) b + 2, csoundGraphUI::js (g.title()), (int) b == currentBlock);
            }
        }

        juce::Component::SafePointer<CsoundGraphView> safeThis (this);
        CsoundActionSheet::show (navButton, "Show", std::move (items), [safeThis] (int result)
        {
            if (safeThis == nullptr || result <= 0) return;
            // lo strumento gia' aperto: si torna ai suoi blocchi
            if (result - 2 == safeThis->currentBlock && result >= 2 && ! safeThis->explainMode) { safeThis->setOverview (true); return; }
            safeThis->showBlock (result - 2);
        });
    }

    //--------------------------------------------------------------------------
    // Testo del popover
    struct Help
    {
        juce::AttributedString s;
        Help() { s.setWordWrap (juce::AttributedString::byWord); s.setLineSpacing (2.0f); }
        void title (const juce::String& t)     { s.append (t + "\n", csoundGraphUI::boldFont (16.0f), csoundGraphUI::kText); }
        void sub (const juce::String& t)       { s.append (t + "\n", csoundGraphUI::plainFont (11.5f), csoundGraphUI::kTextMuted); }
        void code (const juce::String& t)      { s.append (t + "\n", csoundGraphUI::monoFont (12.0f), csoundGraphUI::kAccent); }
        void text (const juce::String& t)      { s.append (t + "\n", csoundGraphUI::plainFont (13.0f), csoundGraphUI::kText); }
        void section (const juce::String& t)   { s.append ("\n" + t.toUpperCase() + "\n", csoundGraphUI::boldFont (10.5f), csoundGraphUI::kTextMuted); }
        void item (const juce::String& k, const juce::String& v, juce::Colour c = csoundGraphUI::kText)
        {
            s.append (k + "  ", csoundGraphUI::boldFont (12.5f), c);
            s.append (v + "\n", csoundGraphUI::plainFont (12.5f), csoundGraphUI::kText);
        }
    };

    static juce::String rateText (char r)
    {
        switch (r)
        {
            case 'i': return "i-rate: computed once, when the note starts";
            case 'k': return "k-rate: every control cycle (every ksmps samples)";
            case 'a': return "a-rate: every sample, in blocks of ksmps";
            case 'f': return "f-signal: a spectral frame (phase vocoder)";
            case 'S': return "string";
            default:  return {};
        }
    }

    juce::String sourceText (int first, int last) const
    {
        juce::StringArray out;
        for (int l = first; l <= juce::jmax (first, last) && l <= sourceLines.size(); ++l)
            if (l >= 1) out.add (sourceLines[l - 1].trim());
        return out.joinIntoString ("\n");
    }

    const csoundGraph::Graph* findUdo (const juce::String& name) const
    {
        for (const auto& g : orchestra.blocks)
            if (g.kind == csoundGraph::Graph::Udo)
                for (const auto& nm : g.names)
                    if (csoundGraphUI::js (nm) == name) return &g;
        return nullptr;
    }

    // node: indice in Graph::nodes del blocco corrente (stesso indice nelle due viste)
    void showPopoverForNode (int node, juce::Rectangle<float> boxInCanvas, juce::Component* from)
    {
        if (currentBlock < 0) return;
        Help h;
        std::vector<CsoundGraphPopover::Action> actions;
        describeNode (currentBlock, node, h, actions);
        popover.show (h.s, getLocalArea (from, boxInCanvas.toNearestInt()), std::move (actions));
    }

    void showModeMenu()
    {
        std::vector<CsoundActionSheetItem> items;
        auto add = [&items] (int id, const juce::String& t, bool on) { CsoundActionSheetItem it; it.id = id; it.text = t; it.ticked = on; items.push_back (it); };
        if (currentBlock >= 0)
            add (7, flowCanvas.hasOverview() ? "Blocks" : "Flow", ! explainMode && ! patchMode);
        else
            add (1, "Map", ! explainMode && ! patchMode);
        add (3, "Explain", explainMode);
        add (9, "Patch", patchMode && ! explainMode);      // prova, temporanea
        if (currentBlock >= 0 && explainMode)
        {
            items.push_back (CsoundActionSheetItem::separator());
            add (6, "Group lines by time (i, k, a)", groupByTime);
        }
        if (currentBlock >= 0 && ! explainMode)
        {
            items.push_back (CsoundActionSheetItem::separator());
            if (patchMode) add (8, "Control objects too (not only the sound path)", patchAll);
            add (5, "Code beside the diagram", splitMode);
            if (orchestra.blocks[(size_t) currentBlock].kind == csoundGraph::Graph::Instr)
                add (4, "Live values on all cables", liveState.all);
        }
        juce::Component::SafePointer<CsoundGraphView> safeThis (this);
        CsoundActionSheet::show (modeButton, "View", std::move (items), [safeThis] (int r)
        {
            if (safeThis == nullptr || r <= 0) return;
            if (r == 9) { safeThis->explainMode = false; safeThis->patchMode = true; safeThis->showBlock (safeThis->currentBlock); return; }
            if (r == 8) { safeThis->patchAll = ! safeThis->patchAll; safeThis->showBlock (safeThis->currentBlock); return; }
            const bool leavingPatch = safeThis->patchMode && (r == 1 || r == 3 || r == 7);
            if (r == 1 || r == 3 || r == 7) safeThis->patchMode = false;
            if (r == 4) { safeThis->liveState.all = ! safeThis->liveState.all; safeThis->updatePills(); safeThis->pushLive(); safeThis->resized(); return; }
            if (r == 5) { safeThis->splitMode = ! safeThis->splitMode; safeThis->showBlock (safeThis->currentBlock); return; }
            if (r == 6) { safeThis->groupByTime = ! safeThis->groupByTime; safeThis->showBlock (safeThis->currentBlock); return; }
            if (r == 7 && safeThis->currentBlock >= 0)
            {
                if (safeThis->explainMode || leavingPatch) { safeThis->explainMode = false; safeThis->showBlock (safeThis->currentBlock); }
                safeThis->setOverview (true);
                return;
            }
            if (r == 3) { safeThis->explainMode = true; safeThis->showBlock (safeThis->currentBlock); }
            else if (safeThis->currentBlock < 0) { safeThis->explainMode = false; safeThis->showBlock (-1); }
            else safeThis->setFlowMode (true);
        });
    }

    void setFlowMode (bool shouldUseFlow)
    {
        if (flowMode == shouldUseFlow && ! explainMode) return;
        flowMode = shouldUseFlow;
        explainMode = false;
        showBlock (currentBlock);
    }

    void describeBlock (int b, Help& h, std::vector<CsoundGraphPopover::Action>& actions)
    {
        using namespace csoundGraphUI;
        if (b < 0 || b >= (int) orchestra.blocks.size()) return;
        const auto& g = orchestra.blocks[(size_t) b];
        h.title (js (g.title()));
        h.sub ("Lines " + juce::String (g.lineBegin) + "-" + juce::String (g.lineEnd));
        h.text (g.kind == csoundGraph::Graph::Udo
                    ? "A user-defined opcode: a reusable piece of code called like any opcode."
                    : "An instrument is a template: every note (score line, schedule, event, MIDI) creates an instance of it that runs until the note ends.");

        h.section ("Starts");
        bool any = false;
        for (const auto& nm : g.names)
        {
            auto it = orchestra.scoreNotes.find (nm);
            if (it != orchestra.scoreNotes.end()) { h.item ("score", juce::String (it->second) + (it->second == 1 ? " note" : " notes")); any = true; }
        }
        for (const auto& l : orchestra.links)
            if (l.to == b && l.kind == "evento" && l.from >= 0)
            { h.item ("started by", js (orchestra.blocks[(size_t) l.from].title())); any = true; }
        if (! any) h.text ("No score line or event in this file starts it (MIDI, a button or the host can).");

        if (! g.pfields.empty())
        {
            juce::String pf;
            for (int p : g.pfields) pf << "p" << p << " ";
            h.section ("Note parameters it reads");
            h.text (pf.trim());
        }

        juce::StringArray ins, outs;
        for (const auto& l : orchestra.links)
        {
            if (l.kind == "evento") continue;
            if (l.to == b && l.from >= 0)   ins.addIfNotAlreadyThere (js (l.name) + "  from " + js (orchestra.blocks[(size_t) l.from].title()));
            if (l.from == b && l.to >= 0)   outs.addIfNotAlreadyThere (js (l.name) + "  to " + js (orchestra.blocks[(size_t) l.to].title()));
        }
        for (const auto& l : orchestra.links)
            if (l.kind == "evento" && l.from == b && l.to >= 0)
                outs.addIfNotAlreadyThere ("starts " + js (orchestra.blocks[(size_t) l.to].title()));
        if (! ins.isEmpty())  { h.section ("Receives"); for (auto& t : ins)  h.text (t); }
        if (! outs.isEmpty()) { h.section ("Sends");    for (auto& t : outs) h.text (t); }

        h.section ("Contents");
        h.text (juce::String ((int) g.nodes.size()) + " boxes, " + juce::String ((int) g.cables.size()) + " cables");

        actions.push_back ({ "Open", [this, b] { showBlock (b); } });
        if (g.lineBegin > 0)
            actions.push_back ({ "Show in Code", [this, line = g.lineBegin] { goToCode (line); } });
    }

    void describeNode (int b, int box, Help& h, std::vector<CsoundGraphPopover::Action>& actions)
    {
        using namespace csoundGraphUI;
        using K = csoundGraph::NodeKind;
        const auto& g = orchestra.blocks[(size_t) b];
        if (box < 0 || box >= (int) g.nodes.size()) return;
        const auto& n = g.nodes[(size_t) box];
        const auto name = js (n.name);

        // --- che cos'e' ---
        switch (n.kind)
        {
            case K::Opcode:
            {
                h.title (name);
                if (const auto* udo = findUdo (name))
                {
                    h.sub ("User-defined opcode, declared at line " + juce::String (udo->lineBegin));
                    h.code ("outputs: " + js (udo->udoOutTypes) + "    inputs: " + js (udo->udoInTypes));
                    const int udoBlock = (int) (udo - orchestra.blocks.data());
                    actions.push_back ({ "Open Opcode", [this, udoBlock] { showBlock (udoBlock); } });
                }
                else if (const auto* help = CsoundOpcodeHelpData::find (name))
                {
                    h.sub (juce::String (help->category).replace (":", u8 ("  \xe2\x80\xba  ")));
                    juce::StringArray syn;
                    syn.addLines (juce::String::fromUTF8 (help->syntax));
                    for (auto& l : syn) h.code (l);
                    h.text (juce::String::fromUTF8 (help->description));
                    actions.push_back ({ "Manual", [name] { juce::URL ("https://csound.com/manual/opcodes/" + juce::URL::addEscapeChars (name, true) + "/").launchInDefaultBrowser(); } });
                }
                else if (auto it = liveInfo.find (name); it != liveInfo.end())
                {
                    h.sub ("Reported by the running Csound engine (no manual entry here)");
                    h.code ("outputs: " + (it->second.outTypes.isEmpty() ? juce::String ("none") : it->second.outTypes)
                            + "    inputs: " + (it->second.inTypes.isEmpty() ? juce::String ("none") : it->second.inTypes));
                    actions.push_back ({ "Manual", [name] { juce::URL ("https://csound.com/manual/opcodes/" + juce::URL::addEscapeChars (name, true) + "/").launchInDefaultBrowser(); } });
                }
                else
                    h.sub ("Not a known opcode: check the spelling, or press Apply so that the engine reports its opcodes");
                break;
            }
            case K::Expr:
                h.title (name == "=" ? juce::String ("= assignment") : name);
                h.text (n.accumulates ? "Adds to the variable on the left (or writes one element of an array): the typical way to mix several sources into one bus."
                                      : "Computes the expression on the right and stores the result in the variable on the left.");
                break;
            case K::PField:
            {
                h.title (name);
                const int p = name.substring (1).getIntValue();
                h.text (p == 1 ? "p1: the instrument number (or name) of this note."
                      : p == 2 ? "p2: the start time of this note, in seconds (beats with a tempo)."
                      : p == 3 ? "p3: the duration of this note; negative = held until turned off."
                      : "p" + juce::String (p) + ": a note parameter - the " + juce::String (p) + "th value of the score line, schedule or event that starts this instrument.");
                h.text ("Every note has its own values: the instrument is a template, the p-fields are its inputs.");
                break;
            }
            case K::GlobalIn:
            {
                h.title ("receive  " + name);
                h.text ("A global variable: written somewhere else (another instrument, the global code, or this instrument in an earlier cycle) and read here. All instances share the same value.");
                juce::StringArray from;
                for (const auto& l : orchestra.links)
                    if (l.to == b && js (l.name) == name && l.from >= 0)
                        from.addIfNotAlreadyThere (js (orchestra.blocks[(size_t) l.from].title()));
                if (! from.isEmpty()) h.item ("written by", from.joinIntoString (", "));
                break;
            }
            case K::Undefined:
                h.title ("? " + name);
                h.text ("Read here but never written in this block and not a global: Csound will report an error (a typo, or a variable written after this line?).");
                break;
            case K::FrameHead:
                h.title (name);
                if (name == "while" || name == "until")
                    h.text ("A loop: the boxes inside the frame repeat " + juce::String (name == "while" ? "while the condition is true" : "until the condition becomes true")
                            + ", all within the same cycle (init pass or control cycle).");
                else
                    h.text ("A condition: the boxes inside the frame run only when it is true. With an i-rate condition it is decided once, when the note starts; with a k-rate condition, at every control cycle.");
                h.code (js (n.text));
                break;
            case K::Jump:
                h.title (name);
                if (name == "igoto" || name == "cigoto")
                    h.text ("Jumps only in the init pass: during the performance the opcodes after it still run, possibly never initialised (this can crash Csound). Use goto to skip both passes.");
                else if (name == "kgoto" || name == "ckgoto")
                    h.text ("Jumps only during the performance (control cycles); the init pass still initialises every opcode.");
                else
                    h.text ("Jumps to a label, both in the init pass and during the performance.");
                h.code (js (n.text));
                break;
            case K::Label:
                h.title (name + ":");
                h.text ("A label: the target of a jump (goto, kgoto, igoto, reinit...).");
                break;
            case K::Macro:
                h.title (name);
                h.text ("A macro: replaced by its text before Csound compiles the code.");
                break;
        }

        // --- nel codice ---
        h.section ("In this code");
        if (n.line > 0)
        {
            h.item ("line " + juce::String (n.line), {}, kTextMuted);
            h.code (sourceText (n.line, n.lineEnd));
        }
        if (const auto rt = rateText (n.rate); rt.isNotEmpty() && n.kind != K::Label)
            h.item ("runs", rt, rateColour (n.rate));

        // cornici che contengono la scatola, dalla piu' interna
        for (int f = n.frame; f >= 0 && f < (int) g.frames.size(); f = g.frames[(size_t) f].parent)
        {
            const auto& fr = g.frames[(size_t) f];
            h.item ("inside", js (fr.kind) + (fr.cond.empty() ? juce::String() : "  " + js (fr.cond)));
        }

        // ingressi: da dove arrivano
        bool insHeader = false;
        for (int i = 0; i < (int) n.ins.size(); ++i)
        {
            const auto& in = n.ins[(size_t) i];
            juce::String from;
            for (const auto& c : g.cables)
                if (c.to == box && c.toIn == i && c.from >= 0 && c.from < (int) g.nodes.size())
                {
                    const auto& src = g.nodes[(size_t) c.from];
                    const bool source = src.kind == K::PField || src.kind == K::GlobalIn;
                    from << (from.isEmpty() ? "" : ", ")
                         << (source ? js (src.name) + (src.kind == K::PField ? " (note parameter)" : " (global)")
                                    : src.line > 0 ? "line " + juce::String (src.line) : js (src.name))
                         << (c.feedback ? " (previous cycle)" : "");
                }
            if (! insHeader) { h.section ("Inputs"); insHeader = true; }
            const auto label = js (in.label).isNotEmpty() ? js (in.label) : juce::String ("in " + juce::String (i + 1));
            h.item (label, js (in.text) + (from.isNotEmpty() ? u8 ("   \xe2\x86\x90 ") + from : juce::String (in.isConst ? "   (constant)" : "")));
        }

        // uscite: chi le legge
        if (! n.outs.empty())
        {
            h.section ("Outputs");
            for (int o = 0; o < (int) n.outs.size(); ++o)
            {
                const auto& out = n.outs[(size_t) o];
                juce::StringArray readers;
                for (const auto& c : g.cables)
                    if (c.from == box && c.fromOut == o && c.to >= 0 && c.to < (int) g.nodes.size())
                        readers.addIfNotAlreadyThere ("line " + juce::String (g.nodes[(size_t) c.to].line) + (c.feedback ? " (next cycle)" : ""));
                h.item (js (out.var), (readers.isEmpty() ? juce::String (out.global ? "global: read by other blocks" : "not read in this block")
                                                          : u8 ("\xe2\x86\x92 ") + readers.joinIntoString (", ")),
                        rateColour (out.rate));
            }
        }

        // traccia: da dove arriva tutto (all'indietro fino alle sorgenti) e
        // dove va a finire (in avanti fino alle uscite), seguendo i cavi
        {
            auto walk = [&g] (int start, bool back)
            {
                std::vector<int> found;
                std::set<int> seen { start };
                std::vector<int> st { start };
                while (! st.empty())
                {
                    const int c = st.back(); st.pop_back();
                    for (const auto& cb : g.cables)
                    {
                        if (cb.feedback) continue;
                        const int next = back ? (cb.to == c ? cb.from : -1) : (cb.from == c ? cb.to : -1);
                        if (next >= 0 && next < (int) g.nodes.size() && seen.insert (next).second) { st.push_back (next); found.push_back (next); }
                    }
                }
                return found;
            };
            static const std::set<std::string> midiOps { "cpsmidi", "notnum", "veloc", "ampmidi", "midinoteoncps", "midinoteonkey", "pchmidi", "octmidi", "midictrl", "ctrl7", "pchbend", "aftouch" };
            juce::StringArray from, to;
            for (int i : walk (box, true))
            {
                const auto& x = g.nodes[(size_t) i];
                if (x.kind == K::PField)                    from.addIfNotAlreadyThere (js (x.name) + " (note parameter)");
                else if (x.kind == K::GlobalIn)             from.addIfNotAlreadyThere (js (x.name) + " (global)");
                else if (! x.channel.empty() && ! x.channelWrite) from.addIfNotAlreadyThere (u8 ("\"") + js (x.channel) + "\" (control)");
                else if (midiOps.count (x.name))            from.addIfNotAlreadyThere ("MIDI (" + js (x.name) + ")");
                else if (x.audioIn)                         from.addIfNotAlreadyThere ("audio input");
            }
            for (int i : walk (box, false))
            {
                const auto& x = g.nodes[(size_t) i];
                if (x.audioOut)                             to.addIfNotAlreadyThere ("audio output (line " + juce::String (x.line) + ")");
                else if (! x.channel.empty() && x.channelWrite) to.addIfNotAlreadyThere ("channel \"" + js (x.channel) + "\"");
                else if (! x.eventTarget.empty())           to.addIfNotAlreadyThere ("notes of instr " + js (x.eventTarget));
                for (const auto& o : x.outs) if (o.global)  to.addIfNotAlreadyThere (js (o.var) + " (global, read elsewhere)");
            }
            if (n.audioOut) to.clear();
            if (! from.isEmpty() || ! to.isEmpty())
            {
                h.section ("Trace");
                if (! from.isEmpty()) h.item ("comes from", from.joinIntoString (", "), rateColour ('k'));
                if (! to.isEmpty())   h.item ("ends in", to.joinIntoString (", "), rateColour ('a'));
                h.sub ("Following the cables all the way back to the sources and forward to the outputs. In Flow, the lit boxes are this chain.");
            }
        }

        // canali, eventi, uscita audio
        if (! n.channel.empty())
        {
            const auto ch = js (n.channel);
            bool host = false;
            for (const auto& hc : orchestra.hostChannels) host = host || js (hc) == ch;
            h.section ("Channel");
            h.text (n.channelWrite ? "Writes channel \"" + ch + "\": shown in the Monitor of the Parameters panel."
                    : host ? "Reads channel \"" + ch + "\": nothing in this file writes it, so it comes from outside - a parameter of the Parameters panel or the host."
                           : "Reads channel \"" + ch + "\", written elsewhere in this file.");
        }
        if (! n.eventTarget.empty())
        {
            h.section ("Event");
            h.text ("Starts a note of instr " + js (n.eventTarget) + ".");
        }
        if (n.audioOut) { h.section ("Output"); h.text ("Sends audio to the outputs of apeCsound."); }
        if (n.audioIn)  { h.section ("Input");  h.text ("Reads the audio inputs of apeCsound."); }

        if (n.line > 0)
            actions.push_back ({ "Show in Code", [this, line = n.line] { goToCode (line); } });
    }

    void goToCode (int line)
    {
        if (! docked) hide();     // agganciata la vista resta accanto al codice
        if (onShowInCode) onShowInCode (line);
    }

    void timerCallback() override
    {
        if (! isShowing()) return;
        readLive();                               // valori dei cavi: 15 volte al secondo
        if ((++tick % 4) != 0) return;            // il resto: circa 4 volte al secondo
        refresh();
        if (docked && getCaretLine) revealLine (getCaretLine());
        if (currentBlock < 0 && ! explainMode && getActiveNotes)
        {
            std::map<int, int> counts;
            for (const auto& [block, name] : mapCanvas.instrumentNames())
                if (const int n = getActiveNotes (name); n > 0) counts[block] = n;
            mapCanvas.setActiveCounts (counts);
            if (patchMode)
            {
                std::map<int, int> pc;
                for (const auto& [block, name] : patcher.instrumentNames())
                    if (const int n = getActiveNotes (name); n > 0) pc[block] = n;
                patcher.setActiveCounts (pc);
            }
        }   // anche dopo una ricostruzione mentre si scrive
        if (getChannelValues)
        {
            const auto values = getChannelValues();
            flowCanvas.setLiveValues (values);
            patcher.setLiveValues (values);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundGraphView)
};
