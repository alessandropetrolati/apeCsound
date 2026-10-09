// [EDIT-CALLOUT] vedi CsoundEditCallout.h
#include "CsoundEditCallout.h"
#include <cmath>

namespace
{
    const juce::Colour kCalloutBg      (0xff161b21); // stesso scuro della toolbar
    const juce::Colour kCalloutText    (0xfff2f5f7);
    const juce::Colour kCalloutMuted   (0xff6f7a85);
    const juce::Colour kCalloutDivider (0xff2c343d);
    const juce::Colour kCalloutAccent  (0xff4aa3b8);

    constexpr int kItemPadding  = 14;
    constexpr int kNavWidth     = 30;
    constexpr int kAnchorGap    = 10;  // spazio fra bolla e selezione (lascia posto alle maniglie)
    constexpr float kCorner     = 9.0f;
}

CsoundEditCallout::CsoundEditCallout()
{
    setOpaque (false);
    setAlwaysOnTop (true);
    setInterceptsMouseClicks (true, false);
    setVisible (false);
}

int CsoundEditCallout::itemWidth (const Item& item) const
{
    // Larghezza misurata + margine: la misura di GlyphArrangement e il
    // rendering (font bold, hinting) possono differire di 1-2 px e
    // drawText tronca l'ultima lettera ("Cu" invece di "Cut").
    const int iconSpace = item.icon != CsoundActionSheetIcon::none ? iconSize + 6 : 0;
    const int textWidth = (int) std::ceil (juce::GlyphArrangement::getStringWidth (itemFont, item.text)) + 4;
    return textWidth + kItemPadding * 2 + iconSpace;
}

void CsoundEditCallout::show (juce::Rectangle<int> anchorArea, std::vector<Item> newItems)
{
    items = std::move (newItems);
    anchor = anchorArea;
    currentPage = 0;
    pressedSlot = hoverSlot = -1;

    auto* parent = getParentComponent();
    const int maxBarWidth = parent != nullptr ? juce::jmax (120, parent->getWidth() - 16) : 400;

    layoutPages (maxBarWidth);
    layoutCurrentPage();
    placeAroundAnchor();

    toFront (false);
    setVisible (true);
    repaint();
}

void CsoundEditCallout::hide()
{
    if (isVisible())
        setVisible (false);
}

void CsoundEditCallout::layoutPages (int maxBarWidth)
{
    // Righe riempite in sequenza (al massimo kMaxRows per pagina): una
    // riga se le voci entrano, altrimenti due; oltre, altre pagine con i
    // bottoni "‹ ›" in coda all'ultima riga di ogni pagina.
    pages.clear();

    if (items.empty())
        return;

    const int n = (int) items.size();
    int i = 0;

    while (i < n)
    {
        Page page;

        while (i < n && (int) page.size() < kMaxRows)
        {
            std::vector<int> row;
            int width = 0;

            while (i < n)
            {
                const int w = itemWidth (items[(size_t) i]);

                if (! row.empty() && width + w > maxBarWidth)
                    break;

                row.push_back (i);
                width += w;
                ++i;
            }

            page.push_back (row);
        }

        pages.push_back (page);
    }

    if (pages.size() > 1)
    {
        // Spazio per i bottoni di navigazione nell'ultima riga di ogni
        // pagina: se non c'e', l'ultima voce passa alla pagina successiva.
        for (size_t p = 0; p < pages.size(); ++p)
        {
            auto& lastRow = pages[p].back();
            int width = kNavWidth * 2;

            for (int index : lastRow)
                width += itemWidth (items[(size_t) index]);

            while (width > maxBarWidth && lastRow.size() > 1)
            {
                const int moved = lastRow.back();
                lastRow.pop_back();
                width -= itemWidth (items[(size_t) moved]);

                if (p + 1 < pages.size())
                    pages[p + 1].front().insert (pages[p + 1].front().begin(), moved);
                else
                    pages.push_back ({ { moved } });
            }
        }
    }
}

void CsoundEditCallout::layoutCurrentPage()
{
    slots.clear();
    barWidth = 0;
    barHeightPx = rowHeight;

    if (pages.empty())
        return;

    currentPage = juce::jlimit (0, (int) pages.size() - 1, currentPage);
    const bool multiPage = pages.size() > 1;
    const auto& page = pages[(size_t) currentPage];

    // Larghezza = la riga piu' larga (con la navigazione sull'ultima).
    std::vector<int> rowWidths;

    for (size_t r = 0; r < page.size(); ++r)
    {
        int w = 0;
        for (int index : page[r])
            w += itemWidth (items[(size_t) index]);

        if (multiPage && r + 1 == page.size())
            w += kNavWidth * 2;

        rowWidths.push_back (w);
        barWidth = juce::jmax (barWidth, w);
    }

    barHeightPx = rowHeight * (int) page.size();

    // Le voci di ogni riga si allargano in proporzione per riempire barWidth.
    for (size_t r = 0; r < page.size(); ++r)
    {
        const int y = rowHeight * (int) r;
        const bool navRow = multiPage && r + 1 == page.size();
        const int itemsWidth = rowWidths[r] - (navRow ? kNavWidth * 2 : 0);
        const int available = barWidth - (navRow ? kNavWidth * 2 : 0);
        int x = 0;

        if (navRow)
        {
            slots.push_back ({ kPrevPage, { x, y, kNavWidth, rowHeight } });
            x += kNavWidth;
        }

        for (size_t k = 0; k < page[r].size(); ++k)
        {
            const int index = page[r][k];
            const int natural = itemWidth (items[(size_t) index]);
            const int w = (k + 1 == page[r].size()) ? (available - (x - (navRow ? kNavWidth : 0)))
                                                     : juce::roundToInt ((double) natural * available / juce::jmax (1, itemsWidth));
            slots.push_back ({ index, { x, y, w, rowHeight } });
            x += w;
        }

        if (navRow)
            slots.push_back ({ kNextPage, { x, y, kNavWidth, rowHeight } });
    }
}

void CsoundEditCallout::placeAroundAnchor()
{
    auto* parent = getParentComponent();
    const int parentW = parent != nullptr ? parent->getWidth()  : 1000;
    const int parentH = parent != nullptr ? parent->getHeight() : 1000;

    const int totalHeight = barHeightPx + arrowSize;

    // Sopra la selezione se c'e' spazio, altrimenti sotto.
    arrowPointsDown = anchor.getY() - kAnchorGap - totalHeight >= 0;

    const int y = arrowPointsDown ? anchor.getY() - kAnchorGap - totalHeight
                                  : juce::jmin (parentH - totalHeight, anchor.getBottom() + kAnchorGap);

    int x = anchor.getCentreX() - barWidth / 2;
    x = juce::jlimit (0, juce::jmax (0, parentW - barWidth), x);

    // Punta della freccia sotto il centro della selezione, ma mai sugli
    // angoli arrotondati della bolla.
    const int minArrowX = juce::roundToInt (kCorner) + arrowSize;
    const int maxArrowX = juce::jmax (minArrowX, barWidth - juce::roundToInt (kCorner) - arrowSize);
    arrowX = juce::jlimit (minArrowX, maxArrowX, anchor.getCentreX() - x);

    setBounds (x, y, barWidth, totalHeight);
}

int CsoundEditCallout::slotAt (juce::Point<int> p) const
{
    const int barY = arrowPointsDown ? 0 : arrowSize;

    for (int i = 0; i < (int) slots.size(); ++i)
        if (slots[(size_t) i].bounds.translated (0, barY).contains (p))
            return i;

    return -1;
}

void CsoundEditCallout::paint (juce::Graphics& g)
{
    const int barY = arrowPointsDown ? 0 : arrowSize;
    const juce::Rectangle<float> bar (0.0f, (float) barY, (float) barWidth, (float) barHeightPx);

    juce::Path shape;
    shape.addRoundedRectangle (bar, kCorner);

    // Freccia verso la selezione
    juce::Path arrow;
    if (arrowPointsDown)
        arrow.addTriangle ((float) arrowX - arrowSize, bar.getBottom(),
                           (float) arrowX + arrowSize, bar.getBottom(),
                           (float) arrowX,             bar.getBottom() + arrowSize);
    else
        arrow.addTriangle ((float) arrowX - arrowSize, bar.getY(),
                           (float) arrowX + arrowSize, bar.getY(),
                           (float) arrowX,             bar.getY() - arrowSize);
    shape.addPath (arrow);

    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillPath (shape, juce::AffineTransform::translation (0.0f, 1.5f));
    g.setColour (kCalloutBg);
    g.fillPath (shape);

    g.setFont (itemFont);

    for (int i = 0; i < (int) slots.size(); ++i)
    {
        const auto& slot = slots[(size_t) i];
        const auto r = slot.bounds.translated (0, barY);

        if (i == pressedSlot || i == hoverSlot)
        {
            g.setColour (kCalloutAccent.withAlpha (i == pressedSlot ? 0.55f : 0.25f));
            g.fillRoundedRectangle (r.reduced (3).toFloat(), 6.0f);
        }

        if (slot.bounds.getX() > 0)
        {
            g.setColour (kCalloutDivider);
            g.fillRect (r.getX(), r.getY() + 9, 1, r.getHeight() - 18);
        }

        if (slot.bounds.getY() > 0 && slot.bounds.getX() == 0)
        {
            g.setColour (kCalloutDivider);
            g.fillRect (6, r.getY(), barWidth - 12, 1);
        }

        if (slot.itemIndex == kPrevPage || slot.itemIndex == kNextPage)
        {
            const bool enabled = slot.itemIndex == kPrevPage ? currentPage > 0 : currentPage + 1 < (int) pages.size();
            g.setColour (enabled ? kCalloutText : kCalloutMuted);
            g.drawText (slot.itemIndex == kPrevPage ? juce::String::charToString (0x2039) : juce::String::charToString (0x203a),
                        r, juce::Justification::centred, false);
        }
        else
        {
            const auto& item = items[(size_t) slot.itemIndex];
            auto textArea = r.reduced (kItemPadding, 0);

            if (item.icon != CsoundActionSheetIcon::none)
            {
                auto path = CsoundActionSheet::getIconPath (item.icon);
                const auto iconArea = textArea.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize);
                textArea.removeFromLeft (6);
                path.scaleToFit ((float) iconArea.getX(), (float) iconArea.getY(),
                                 (float) iconArea.getWidth(), (float) iconArea.getHeight(), true);
                g.setColour (item.enabled ? kCalloutAccent : kCalloutMuted);
                g.fillPath (path);
            }

            g.setColour (item.enabled ? kCalloutText : kCalloutMuted);
            g.drawText (item.text, textArea, juce::Justification::centred, false);
        }
    }
}

void CsoundEditCallout::mouseDown (const juce::MouseEvent& e)
{
    pressedSlot = slotAt (e.getPosition());
    repaint();
}

void CsoundEditCallout::mouseDrag (const juce::MouseEvent& e)
{
    const int s = slotAt (e.getPosition());

    if (s != pressedSlot)
    {
        pressedSlot = s;
        repaint();
    }
}

void CsoundEditCallout::mouseExit (const juce::MouseEvent&)
{
    hoverSlot = -1;
    repaint();
}

void CsoundEditCallout::mouseUp (const juce::MouseEvent& e)
{
    const int s = slotAt (e.getPosition());
    pressedSlot = -1;
    repaint();

    if (s < 0 || s != slotAt (e.getMouseDownPosition()))
        return;

    const int index = slots[(size_t) s].itemIndex;

    if (index == kPrevPage || index == kNextPage)
    {
        const int newPage = currentPage + (index == kNextPage ? 1 : -1);

        if (newPage >= 0 && newPage < (int) pages.size())
        {
            currentPage = newPage;
            layoutCurrentPage();
            placeAroundAnchor();
            repaint();
        }

        return;
    }

    const auto& item = items[(size_t) index];

    if (! item.enabled)
        return;

    const int id = item.id;
    hide();

    if (onItemSelected)
        onItemSelected (id);
}
