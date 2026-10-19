#include "CsoundGraphs.h"
#include <cstring>
#include <cmath>

namespace
{
    // Colori della consolle e delle schede (PluginEditor.cpp).
    const juce::Colour kPanelBg    { 0xff10181f }; // stesso fondo della consolle
    const juce::Colour kBorder     { 0xff2a3540 };
    const juce::Colour kText       { 0xffe8eef1 };
    const juce::Colour kTextMuted  { 0xff8a9aa5 };
    const juce::Colour kAccent     { 0xff4aa3b8 };
    const juce::Colour kTileBg     { 0xff1c2730 };
    const juce::Colour kFtable     { 0xffd9ae62 };

    // Valori di WINDAT.polarity (cwindow.h).
    enum { kNoPol = 0, kNegPol = 1, kPosPol = 2, kBiPol = 3 };
}

//==============================================================================
void CsoundGraphStore::clear()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    for (int i = 0; i < numGraphs; ++i)
        graphs[i] = Graph();
    numGraphs = 0;
    changeCount.fetch_add (1);
}

int CsoundGraphStore::open (const char* caption)
{
    if (caption == nullptr)
        caption = "";

    const juce::SpinLock::ScopedLockType sl (lock);

    for (int i = 0; i < numGraphs; ++i)
        if (std::strncmp (graphs[i].caption, caption, captionSize - 1) == 0)
            return i + 1;

    if (numGraphs >= maxGraphs)
        return 0;

    auto& gr = graphs[numGraphs];
    gr = Graph();
    std::strncpy (gr.caption, caption, captionSize - 1);
    gr.caption[captionSize - 1] = 0;
    gr.isFtable = std::strncmp (caption, "ftable", 6) == 0;
    return ++numGraphs;
}

void CsoundGraphStore::draw (int slotPlusOne, const cs_float* data, int numPoints, double absScale, int polarity)
{
    if (slotPlusOne <= 0 || slotPlusOne > maxGraphs || data == nullptr || numPoints <= 0)
        return;

    // Thread audio (display/dispfft a k-rate): se il pannello sta leggendo,
    // questo fotogramma si salta invece di aspettare.
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked())
        return;

    auto& gr = graphs[slotPlusOne - 1];

    if (! gr.isFtable && ! viewerActive.load())
        return;

    if (numPoints <= maxColumns)
    {
        for (int i = 0; i < numPoints; ++i)
            gr.lo[i] = gr.hi[i] = (float) data[i];
        gr.numColumns = numPoints;
    }
    else
    {
        // Colonne min/max: nessun picco perso anche con tabelle lunghe.
        for (int c = 0; c < maxColumns; ++c)
        {
            const int start = (int) ((int64_t) c * numPoints / maxColumns);
            const int end   = juce::jmax (start + 1, (int) ((int64_t) (c + 1) * numPoints / maxColumns));
            auto lo = data[start], hi = data[start];
            for (int i = start + 1; i < end; ++i)
            {
                lo = juce::jmin (lo, data[i]);
                hi = juce::jmax (hi, data[i]);
            }
            gr.lo[c] = (float) lo;
            gr.hi[c] = (float) hi;
        }
        gr.numColumns = maxColumns;
    }

    gr.numPoints = numPoints;
    gr.scale     = (float) (absScale > 0 ? absScale : 1.0);
    gr.polarity  = polarity;
    ++gr.version;
    changeCount.fetch_add (1);
}

void CsoundGraphStore::snapshot (std::vector<Graph>& out)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    out.clear();
    for (int i = 0; i < numGraphs; ++i)
        if (graphs[i].numColumns > 0)
            out.push_back (graphs[i]);
}

//==============================================================================
CsoundGraphsPanel::CsoundGraphsPanel (CsoundGraphStore& s)
    : store (s)
{
    viewport.setViewedComponent (&grid, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

CsoundGraphsPanel::~CsoundGraphsPanel()
{
    store.setViewerActive (false);
}

void CsoundGraphsPanel::visibilityChanged()
{
    // display/dispfft vengono registrati solo mentre il pannello e' visibile.
    if (isVisible())
    {
        store.setViewerActive (true);
        refresh (true);
        startTimerHz (25);
    }
    else
    {
        stopTimer();
        store.setViewerActive (false);
    }
}

void CsoundGraphsPanel::timerCallback()
{
    refresh (false);
}

void CsoundGraphsPanel::refresh (bool force)
{
    const auto change = store.getChangeCount();
    if (! force && change == lastChange)
        return;

    lastChange = change;
    const auto oldCount = grid.graphs.size();
    store.snapshot (grid.graphs);

    if (force || oldCount != grid.graphs.size())
    {
        resized();
        repaint(); // messaggio "No graphs yet" mostrato/nascosto
    }

    grid.repaint();
}

void CsoundGraphsPanel::paint (juce::Graphics& g)
{
    g.fillAll (kPanelBg);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (10, 0);
    g.setColour (kTextMuted);
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("GRAPHS", header, juce::Justification::centredLeft, false);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (grid.graphs.empty() ? juce::String() : juce::String ((int) grid.graphs.size()),
                header, juce::Justification::centredRight, false);

    if (grid.graphs.empty())
    {
        g.setColour (kTextMuted);
        g.setFont (juce::Font (juce::FontOptions (12.5f)));
        g.drawFittedText ("No graphs yet. Function tables appear when they are created (f, ftgen, GEN);\n"
                          "signals with display or dispfft, e.g.  display aSig, 0.1   dispfft aSig, 0.1, 1024",
                          getLocalBounds().withTrimmedTop (headerHeight).reduced (12),
                          juce::Justification::centred, 4);
    }
}

void CsoundGraphsPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop (headerHeight).reduced (8, 0).withTrimmedBottom (8);
    viewport.setBounds (area);

    // Riquadri alti quanto il pannello (una riga visibile, le altre si
    // scorrono), entro limiti ragionevoli.
    grid.tileHeight = juce::jlimit (80, 200, area.getHeight() - Grid::gap);

    const int w = juce::jmax (1, area.getWidth() - viewport.getScrollBarThickness());
    grid.setSize (w, grid.heightForWidth (w));
}

//==============================================================================
int CsoundGraphsPanel::Grid::heightForWidth (int width) const
{
    const int cols = juce::jmax (1, (width + gap) / (minTileWidth + gap));
    const int rows = ((int) graphs.size() + cols - 1) / cols;
    return juce::jmax (1, rows * (tileHeight + gap));
}

void CsoundGraphsPanel::Grid::paint (juce::Graphics& g)
{
    const int width = getWidth();
    const int cols = juce::jmax (1, (width + gap) / (minTileWidth + gap));
    const float tileW = (float) (width - (cols - 1) * gap) / (float) cols;

    for (size_t idx = 0; idx < graphs.size(); ++idx)
    {
        const auto& gr = graphs[idx];
        const int col = (int) idx % cols, row = (int) idx / cols;
        juce::Rectangle<float> tile (col * (tileW + gap), (float) (row * (tileHeight + gap)), tileW, (float) tileHeight);

        g.setColour (kTileBg);
        g.fillRoundedRectangle (tile, 8.0f);

        auto content = tile.reduced (10.0f, 8.0f);
        auto header = content.removeFromTop (18.0f);

        g.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold)));
        g.setColour (gr.isFtable ? kFtable : kAccent);
        g.drawText (juce::String::fromUTF8 (gr.caption).trimCharactersAtEnd (": "), header, juce::Justification::centredLeft, true);

        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.setColour (kTextMuted);
        g.drawText (juce::String (gr.numPoints) + " pts  ·  max " + juce::String (gr.scale, 3),
                    header, juce::Justification::centredRight, true);

        content.removeFromTop (4.0f);
        auto plot = content;

        // Asse Y secondo la polarita' calcolata da Csound.
        float yTop = gr.scale, yBottom = -gr.scale;
        if (gr.polarity == kPosPol) yBottom = 0.0f;
        else if (gr.polarity == kNegPol) yTop = 0.0f;

        auto toY = [&] (float v)
        {
            const float t = (yTop - v) / juce::jmax (1.0e-9f, yTop - yBottom);
            return plot.getY() + juce::jlimit (0.0f, 1.0f, t) * plot.getHeight();
        };

        // Linea dello zero.
        g.setColour (kBorder);
        if (yTop > 0.0f && yBottom < 0.0f)
            g.drawHorizontalLine ((int) toY (0.0f), plot.getX(), plot.getRight());
        g.drawRect (plot, 1.0f);

        const int n = gr.numColumns;
        if (n <= 0)
            continue;

        const auto colour = gr.isFtable ? kFtable : kAccent;
        const float dx = n > 1 ? plot.getWidth() / (float) (n - 1) : 0.0f;

        bool envelope = false;
        for (int i = 0; i < n && ! envelope; ++i)
            envelope = gr.lo[i] != gr.hi[i];

        if (envelope)
        {
            // Tabelle lunghe: banda min/max per colonna.
            juce::Path band;
            band.startNewSubPath (plot.getX(), toY (gr.hi[0]));
            for (int i = 1; i < n; ++i)
                band.lineTo (plot.getX() + i * dx, toY (gr.hi[i]));
            for (int i = n - 1; i >= 0; --i)
                band.lineTo (plot.getX() + i * dx, toY (gr.lo[i]));
            band.closeSubPath();
            g.setColour (colour.withAlpha (0.55f));
            g.fillPath (band);
            g.setColour (colour);
            g.strokePath (band, juce::PathStrokeType (1.0f));
        }
        else
        {
            juce::Path line;
            line.startNewSubPath (plot.getX(), toY (gr.lo[0]));
            for (int i = 1; i < n; ++i)
                line.lineTo (plot.getX() + i * dx, toY (gr.lo[i]));
            g.setColour (colour);
            g.strokePath (line, juce::PathStrokeType (1.4f));
        }
    }
}
