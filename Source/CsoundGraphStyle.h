#pragma once
/*
    CsoundGraphStyle.h - palette, font e piccole funzioni comuni alle viste
    del grafo (CsoundGraphView.h, CsoundGraphFlow.h).
*/
#include <JuceHeader.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace csoundGraphUI
{
    inline juce::String js (const std::string& s)      { return juce::String (juce::CharPointer_UTF8 (s.c_str())); }
    inline juce::String u8 (const char* s)             { return juce::String::fromUTF8 (s); }   // letterali con caratteri non ASCII

    // Palette di apeCsound (pannello Parametri, consolle), con lo sfondo
    // del grafo piu' scuro del pannello perche' cavi e scatole risaltino.
    const juce::Colour kBg        { 0xff090d12 };
    const juce::Colour kBar       { 0xff10181f };
    const juce::Colour kLaneFill  { 0xff0e151c };
    const juce::Colour kBoxFill   { 0xff17212a };
    const juce::Colour kBorder    { 0xff2a3540 };
    const juce::Colour kText      { 0xffe8eef1 };
    const juce::Colour kTextMuted { 0xff8a9aa5 };
    const juce::Colour kAccent    { 0xff4aa3b8 };
    const juce::Colour kPopBg     { 0xff1c2730 };
    const juce::Colour kCardBg    { 0xff141d24 };   // fondo di apeCsound Guide / About / menu

    inline juce::Colour rateColour (char r)
    {
        switch (r)
        {
            case 'a': return juce::Colour (0xff3b82f6);   // audio: blu
            case 'k': return juce::Colour (0xffe4572e);   // controllo: corallo
            case 'f': return juce::Colour (0xff2a9d8f);   // spettro
            case 'S': return juce::Colour (0xff9b6fd0);   // stringa
            default:  return juce::Colour (0xff8a8a8a);   // i-rate e sconosciuto
        }
    }

    inline juce::Font plainFont (float h = 12.5f) { return juce::Font (juce::FontOptions (h)); }
    inline juce::Font boldFont  (float h = 12.5f) { return juce::Font (juce::FontOptions (h, juce::Font::bold)); }
    inline juce::Font monoFont  (float h = 12.0f) { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::plain)); }
    inline float textWidth (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }

    inline juce::String shorten (const juce::String& s, int maxChars = 52)
    {
        return s.length() > maxChars ? s.substring (0, maxChars - 3) + "..." : s;
    }

    // Valori dal vivo sui cavi (come il mouse sopra un cavo in Max): stato
    // condiviso dalle viste Flow e Detail.
    struct LiveCables
    {
        std::map<juce::String, juce::String> text;   // variabile -> "0.73" / "-12 dB"
        std::map<juce::String, float> level;         // variabili audio: livello 0..1
        bool all = false;                            // interruttore Live: tutti i cavi
        juce::String hover, selected;                // cavo sotto il mouse / toccato
        bool shows (const juce::String& var) const   { return all || var == hover || var == selected; }
    };

    // il cavo piu' vicino al punto (entro 7 px col mouse, 18 col dito su
    // iOS / Android: un cavo e' sottile), "" se nessuno
    inline juce::String cableAt (const std::vector<std::pair<juce::Path, juce::String>>& hits, juce::Point<float> pos)
    {
        juce::String best;
       #if JUCE_IOS || JUCE_ANDROID
        float bestD = 18.0f;
       #else
        float bestD = 7.0f;
       #endif
        for (const auto& [path, var] : hits)
        {
            juce::Point<float> nearest;
            path.getNearestPoint (pos, nearest);
            const float d = nearest.getDistanceFrom (pos);
            if (d < bestD) { bestD = d; best = var; }
        }
        return best;
    }

    // Etichette dei valori dal vivo, disegnate DOPO le scatole (sempre sopra)
    // e senza sovrapporsi: per ogni cavo si prova a meta', poi in altri punti
    // del cavo, poi sopra / sotto; si evitano le scatole e le etichette gia'
    // messe. Prima i cavi evidenziati (mouse, tocco), che hanno il posto
    // migliore. Se proprio non c'e' posto: a meta' cavo comunque.
    struct LiveLabel { juce::Path path; juce::String var, value; juce::Colour colour; bool hot = false; };

    inline void drawLiveLabels (juce::Graphics& g, std::vector<LiveLabel> labels, const std::vector<juce::Rectangle<float>>& obstacles, bool skipIfNoRoom = false)
    {
        std::stable_sort (labels.begin(), labels.end(), [] (const LiveLabel& a, const LiveLabel& b) { return a.hot && ! b.hot; });
        const auto f = boldFont (11.0f);
        std::vector<juce::Rectangle<float>> placed;
        auto freeAt = [&] (juce::Rectangle<float> r)
        {
            for (const auto& p : placed)    if (p.expanded (3.0f).intersects (r)) return false;
            for (const auto& o : obstacles) if (o.intersects (r)) return false;
            return true;
        };
        static const float fractions[] = { 0.5f, 0.35f, 0.65f, 0.22f, 0.78f, 0.12f, 0.88f };
        for (const auto& l : labels)
        {
            const auto t = l.var + "  " + l.value;
            const float w = textWidth (f, t) + 14.0f, h = 18.0f;
            const float len = l.path.getLength();
            juce::Rectangle<float> pill;
            bool found = false;
            for (float dy : { 0.0f, -14.0f, 14.0f })
            {
                for (float fr : fractions)
                {
                    const auto at = l.path.getPointAlongPath (len * fr);
                    const juce::Rectangle<float> r (at.x - w * 0.5f, at.y - h * 0.5f + dy, w, h);
                    if (freeAt (r)) { pill = r; found = true; break; }
                }
                if (found) break;
            }
            if (! found && skipIfNoRoom && ! l.hot) continue;      // niente etichette una sopra l'altra
            if (! found)
            {
                const auto at = l.path.getPointAlongPath (len * 0.5f);
                pill = { at.x - w * 0.5f, at.y - h * 0.5f, w, h };
            }
            placed.push_back (pill);
            g.setColour (kBg.withAlpha (0.94f));
            g.fillRoundedRectangle (pill, 9.0f);
            g.setColour (l.hot ? kAccent : l.colour);
            g.drawRoundedRectangle (pill, 9.0f, l.hot ? 1.6f : 1.0f);
            g.setColour (kText);
            g.setFont (f);
            g.drawText (t, pill, juce::Justification::centred, false);
        }
    }

    // etichetta del valore a meta' cavo
    inline void drawLiveLabel (juce::Graphics& g, const juce::Path& cable, const juce::String& var, const juce::String& value, juce::Colour col, bool highlight)
    {
        const auto at = cable.getPointAlongPath (cable.getLength() * 0.5f);
        const auto t = var + "  " + value;
        const auto f = boldFont (11.0f);
        const float w = textWidth (f, t) + 14.0f;
        const juce::Rectangle<float> pill (at.x - w * 0.5f, at.y - 9.0f, w, 18.0f);
        g.setColour (kBg.withAlpha (0.92f));
        g.fillRoundedRectangle (pill, 9.0f);
        g.setColour (highlight ? kAccent : col);
        g.drawRoundedRectangle (pill, 9.0f, highlight ? 1.6f : 1.0f);
        g.setColour (kText);
        g.setFont (f);
        g.drawText (t, pill, juce::Justification::centred, false);
    }

//==============================================================================
// Viewport della vista Algorithm: rotellina e trackpad scorrono come nel
// code editor (stessa taratura: 14 * 16 px per unita', in pixel, senza
// arrotondare; Shift scambia verticale e orizzontale). Cmd/Ctrl/Alt
// restano al comportamento di JUCE.
class ScrollViewport : public juce::Viewport
{
public:
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isAltDown() || getViewedComponent() == nullptr)
        {
            juce::Viewport::mouseWheelMove (e, wheel);
            return;
        }
        constexpr double pixelsPerUnit = 14.0 * 16.0;
        double dx = -wheel.deltaX * pixelsPerUnit;
        double dy = -wheel.deltaY * pixelsPerUnit;
        if (e.mods.isShiftDown() && juce::approximatelyEqual (dx, 0.0))
            std::swap (dx, dy);

        const auto* c = getViewedComponent();
        const double maxX = juce::jmax (0, c->getWidth()  - getMaximumVisibleWidth());
        const double maxY = juce::jmax (0, c->getHeight() - getMaximumVisibleHeight());
        if (maxY <= 0.0 && maxX > 0.0 && juce::approximatelyEqual (dx, 0.0))
            std::swap (dx, dy);   // diagramma solo largo: la rotellina scorre in orizzontale
        if ((maxX <= 0.0 || juce::approximatelyEqual (dx, 0.0)) && (maxY <= 0.0 || juce::approximatelyEqual (dy, 0.0)))
        {
            juce::Component::mouseWheelMove (e, wheel);   // niente da scorrere: al parent
            return;
        }
        posX = juce::jlimit (0.0, maxX, (std::abs (posX - getViewPositionX()) < 1.0 ? posX : (double) getViewPositionX()) + dx);
        posY = juce::jlimit (0.0, maxY, (std::abs (posY - getViewPositionY()) < 1.0 ? posY : (double) getViewPositionY()) + dy);
        setViewPosition ((int) std::round (posX), (int) std::round (posY));
    }

private:
    double posX = 0.0, posY = 0.0;   // posizione frazionaria (trackpad a passi piccoli)
};

// Le tele (Flow, Explain, Detail) girano la rotellina al loro ScrollViewport.
inline void forwardWheel (juce::Component& from, const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (auto* vp = from.findParentComponentOfClass<ScrollViewport>())
        vp->mouseWheelMove (e.getEventRelativeTo (vp), wheel);
}
}
