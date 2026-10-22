#pragma once

#include <JuceHeader.h>
#include "CsoundActionSheet.h"

/**
    juce::ComboBox con il menu adatto alla piattaforma:

    - iOS / Android: al tocco apre il menu di apeCsound (CsoundActionSheet:
      righe alte, scorrimento con il dito, spunta sulla voce corrente), al
      posto del PopupMenu di JUCE, poco usabile con le dita.
    - macOS / Windows / Linux: il menu a tendina originale di JUCE, piu'
      rapido da usare con il mouse.

    Stesse API di un ComboBox normale: funziona con
    ComboBoxParameterAttachment, onChange, setSelectedId...
    Il titolo del menu mobile e' quello impostato con setPopupTitle()
    (es. il nome del parametro).
*/
class CsoundComboBox final : public juce::ComboBox
{
public:
    using juce::ComboBox::ComboBox;

    void setPopupTitle (const juce::String& title)  { popupTitle = title; }

   #if JUCE_IOS || JUCE_ANDROID
    void showPopup() override
    {
        if (! isEnabled() || getNumItems() == 0)
        {
            hidePopup();
            return;
        }

        std::vector<CsoundActionSheetItem> items;
        items.reserve ((size_t) getNumItems());

        const int selectedId = getSelectedId();

        for (int i = 0; i < getNumItems(); ++i)
        {
            CsoundActionSheetItem item;
            item.id      = getItemId (i);
            item.text    = getItemText (i);
            item.enabled = isItemEnabled (item.id);
            item.ticked  = (item.id == selectedId);
            items.push_back (std::move (item));
        }

        juce::Component::SafePointer<CsoundComboBox> safeThis (this);

        CsoundActionSheet::show (*this, popupTitle, std::move (items), [safeThis] (int result)
        {
            if (safeThis == nullptr)
                return;

            // ComboBox segna il menu come "attivo" prima di chiamare
            // showPopup(): va azzerato (hidePopup) anche se si annulla,
            // altrimenti il tocco successivo non riaprirebbe il menu.
            safeThis->hidePopup();

            if (result != 0)
                safeThis->setSelectedId (result, juce::sendNotificationSync);
        });
    }
   #endif

private:
    juce::String popupTitle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundComboBox)
};
