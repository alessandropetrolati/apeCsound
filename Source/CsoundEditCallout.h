#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>
#include "CsoundActionSheet.h" // icone condivise

/**
    [EDIT-CALLOUT] Menu di editing NON modale per il touch, stile "callout"
    di iOS: una bolla con freccia ancorata sopra (o sotto) la selezione, con
    le voci in fila (Cut · Copy · Paste · ...), su due righe se non
    entrano in una (massimizza lo spazio orizzontale su iPhone) e con le
    pagine "‹ ›" solo oltre le due righe. Non ha uno sfondo che cattura i tocchi: intercetta solo
    il proprio rettangolo, quindi le maniglie della selezione e il resto
    dell'editor restano subito usabili (a differenza di CsoundActionSheet,
    che e' modale e "spreca" il primo tocco per chiudersi).

    Vive come figlio del componente che lo mostra (CsoundCodeEditor): chi lo
    usa lo nasconde quando la selezione cambia, si scorre, si digita o si
    tocca altrove.

    RIMOZIONE: questa funzionalita' e' isolata. Per toglierla: cancellare
    CsoundEditCallout.h/.cpp (e le due righe nel .jucer) e rimuovere i
    blocchi marcati "[EDIT-CALLOUT]" in CsoundCodeEditor.h/.cpp e
    CsoundCodeView.h/.cpp (gli hook virtuali showTouchEditMenu/
    hideTouchEditMenu di CodeView tornano a chiamare showContextMenu).
*/
class CsoundEditCallout final : public juce::Component
{
public:
    struct Item
    {
        int id = 0;
        juce::String text;
        bool enabled = true;
        CsoundActionSheetIcon icon = CsoundActionSheetIcon::none; // a sinistra del testo
    };

    CsoundEditCallout();

    /** Mostra la bolla ancorata ad anchorArea (coordinate del genitore):
        sopra se c'e' spazio, altrimenti sotto. Riparte dalla prima pagina. */
    void show (juce::Rectangle<int> anchorArea, std::vector<Item> newItems);
    void hide();

    /** Voce scelta (id): la bolla si e' gia' nascosta. */
    std::function<void (int)> onItemSelected;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    static constexpr int rowHeight   = 40;
    static constexpr int kMaxRows    = 2;   // una riga se le voci entrano, altrimenti due; oltre: pagine
    static constexpr int iconSize    = 16;
    static constexpr int arrowSize   = 8;

private:
    struct Slot
    {
        int itemIndex = -1;          // >= 0: voce; kPrevPage / kNextPage: navigazione
        juce::Rectangle<int> bounds; // relative alla bolla
    };

    static constexpr int kPrevPage = -2, kNextPage = -3;

    void layoutPages (int maxBarWidth);
    void layoutCurrentPage();
    void placeAroundAnchor();
    int slotAt (juce::Point<int> p) const;
    int itemWidth (const Item& item) const;

    juce::Font itemFont { juce::FontOptions (14.0f, juce::Font::bold) };
    std::vector<Item> items;
    using Row  = std::vector<int>;       // indici in items
    using Page = std::vector<Row>;       // 1..kMaxRows righe
    std::vector<Page> pages;
    int currentPage = 0;
    std::vector<Slot> slots;
    int barWidth = 0, barHeightPx = rowHeight;
    bool arrowPointsDown = true;         // bolla SOPRA la selezione: freccia verso il basso
    int arrowX = 0;                      // x della punta, relativa alla bolla
    juce::Rectangle<int> anchor;         // in coordinate del genitore
    int pressedSlot = -1, hoverSlot = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundEditCallout)
};
