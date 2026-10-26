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

    /** true se il cambio di selezione in corso viene da una scelta
        dell'utente nel menu (showPopup qui sotto imposta il flag subito
        prima di setSelectedId) e non da fuori (host, chnset, snapshot).
        Da chiamare dentro onChange. */
    bool wasLastChangeFromUser() const { return userPicking; }

    // Il menu: foglio di apeCsound su iOS/Android, PopupMenu di JUCE
    // altrove - in entrambi i casi la scelta dell'utente passa da QUI
    // (userPicking), cosi' onChange puo' distinguerla da un cambio esterno.
    void showPopup() override
    {
       #if ! (JUCE_IOS || JUCE_ANDROID)
        showDesktopPopup();
       #else
        showSheetPopup();
       #endif
    }

   #if JUCE_IOS || JUCE_ANDROID
    void showSheetPopup()
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
            {
                const juce::ScopedValueSetter<bool> picking (safeThis->userPicking, true);
                safeThis->setSelectedId (result, juce::sendNotificationSync);
            }
        });
    }
   #endif

private:
    // Stessa logica di juce::ComboBox::showPopup (spunta sulla voce
    // selezionata, opzioni dalla LookAndFeel), ma con userPicking attorno a
    // setSelectedId.
    void showDesktopPopup()
    {
        auto menu = *getRootMenu();

        if (menu.getNumItems() > 0)
        {
            const int selectedId = getSelectedId();
            for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
            {
                auto& item = it.getItem();
                if (item.itemID != 0)
                    item.isTicked = (item.itemID == selectedId);
            }
        }
        else
        {
            menu.addItem (1, "(no choices)", false, false);
        }

        juce::Component::SafePointer<CsoundComboBox> safeThis (this);
        auto& lf = getLookAndFeel();
        menu.setLookAndFeel (&lf);

        // Le opzioni della LookAndFeel vogliono la Label interna: la stessa
        // che JUCE usa, raggiunta come primo figlio Label.
        juce::Label* label = nullptr;
        for (auto* c : getChildren())
            if ((label = dynamic_cast<juce::Label*> (c)) != nullptr)
                break;

        const auto options = label != nullptr ? lf.getOptionsForComboBoxPopupMenu (*this, *label)
                                              : juce::PopupMenu::Options().withTargetComponent (this);

        menu.showMenuAsync (options, [safeThis] (int result)
        {
            if (safeThis == nullptr)
                return;

            safeThis->hidePopup();

            if (result != 0)
            {
                const juce::ScopedValueSetter<bool> picking (safeThis->userPicking, true);
                safeThis->setSelectedId (result, juce::sendNotificationSync);
            }
        });
    }

    juce::String popupTitle;
    bool userPicking = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundComboBox)
};
