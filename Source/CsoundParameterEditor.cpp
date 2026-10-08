#include "CsoundParameterEditor.h"
#include "NativeAlertMac.h"

namespace
{
    // Azione di Undo minimale basata su due lambda (vedi il commento su
    // CsoundParameterMappingPanel::undo()/redo() in CsoundParameterEditor.h
    // per il quadro completo) - usata per i metadata in modalita' Edit
    // (nome canale, min/max/default/skew/step, default bool, opzioni/
    // default choice) e per l'aggiunta/rimozione di un parametro, cioe'
    // tutto cio' che NON passa per un juce::RangedAudioParameter apvts (per
    // quello, vedi invece GenericParamRow: usa il supporto NATIVO di JUCE
    // passando lo stesso undoManager agli attachment). perform() e undo()
    // si limitano a richiamare le due lambda fornite dal chiamante - MAI
    // devono catturare il `this` di una riga (ParamRow/ecc.), che puo'
    // essere distrutta e ricostruita da rebuildUnifiedRows() tra un
    // perform() e l'undo() corrispondente.
    struct LambdaUndoableAction final : public juce::UndoableAction
    {
        LambdaUndoableAction (std::function<void()> doItIn, std::function<void()> undoItIn)
            : doIt (std::move (doItIn)), undoIt (std::move (undoItIn)) {}

        std::function<void()> doIt;
        std::function<void()> undoIt;

        bool perform() override { if (doIt)   doIt();   return true; }
        bool undo()    override { if (undoIt) undoIt(); return true; }
    };

    // Tema SCURO dedicato a questo pannello (stesso spirito della console
    // di log in PluginEditor, che e' gia' scura): il resto dell'app usa un
    // tema chiaro (CsoundLookAndFeel) sotto cui il testo nero di default di
    // TextEditor/ComboBox risultava illeggibile su alcuni sfondi/stati -
    // qui i colori vengono impostati ESPLICITAMENTE su ogni componente,
    // cosi' il contrasto e' garantito a prescindere dal tema globale.
    const juce::Colour kPanelBg      { 0xff10181f }; // come logConsole in PluginEditor
    const juce::Colour kCardBg       { 0xff19232c }; // sfondo di una card in modalita' UI, leggermente piu' chiaro del pannello
    const juce::Colour kCardBgEditing { 0xff232d39 }; // sfondo di una card in modalita' Edit - piu' chiaro di kCardBg, cosi' il cambio di modalita' si vede subito anche senza leggere i campi
    const juce::Colour kFieldBg      { 0xff202a33 };
    const juce::Colour kFieldOutline { 0xff3a4550 };
    const juce::Colour kAccent       { 0xff17a2b8 }; // stesso accento teal del resto dell'app
    const juce::Colour kDanger       { 0xffb33a3a }; // bottone di rimozione riga
    const juce::Colour kText         { 0xffe8eef1 };
    const juce::Colour kTextMuted    { 0xff8a9aa5 };
    const juce::Colour kPlaceholder  { 0xff5b6b74 };

    // Colore della maniglia/etichetta tipo, uno per ciascuno dei 4 tipi di
    // card Edit (vedi mockup: barra verticale a sinistra colorata) - "Slider
    // Float" per Float, "Slider Int" per Int, "Toggle" per Bool, "Menu" per Choice.
    const juce::Colour kSliderAccent { 0xff4a90e2 }; // blu
    const juce::Colour kKnobAccent   { 0xff9370db }; // viola
    const juce::Colour kToggleAccent { 0xff2ecc9a }; // verde/teal
    const juce::Colour kMenuAccent   { 0xffe8954a }; // arancione

    void applyDarkFieldColours (juce::TextEditor& field)
    {
        field.setColour (juce::TextEditor::backgroundColourId,     kFieldBg);
        field.setColour (juce::TextEditor::textColourId,           kText);
        field.setColour (juce::TextEditor::outlineColourId,        kFieldOutline);
        field.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
        field.setColour (juce::TextEditor::highlightColourId,      kAccent.withAlpha (0.35f));
        field.setColour (juce::TextEditor::highlightedTextColourId, kText);
    }

    // Etichetta piccola, grigia, MAIUSCOLA - usata sopra ogni campo numerico
    // nelle card Edit (MIN/MAX/INIT/EXP/STEP/DEFAULT/OPZIONI...).
    void setupFieldCaption (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, kTextMuted);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setMinimumHorizontalScale (1.0f);
    }

    // Etichetta tipo in testa alla card ("SLIDER · CANALE" ecc.) - colorata
    // come l'accento del tipo.
    void setupTypeLabel (juce::Label& label, const juce::String& text, juce::Colour accent)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, accent);
        label.setJustificationType (juce::Justification::centredLeft);
    }

    // Formattazione di un numero reale per il commento di configurazione
    // (vedi sotto): arrotonda a 6 decimali e toglie gli zero finali inutili,
    // cosi' 10.0f diventa "10" e 0.001f resta "0.001" invece di stampare
    // artefatti tipo "0.0010000000475" dovuti alla precisione float.
    juce::String formatConfigNumber (double value)
    {
        auto text = juce::String (value, 6);
        if (text.containsChar ('.'))
        {
            while (text.endsWithChar ('0'))
                text = text.dropLastCharacters (1);
            if (text.endsWithChar ('.'))
                text = text.dropLastCharacters (1);
        }
        return text;
    }

    // Contenuto del commento "TIPO: chiave=valore; ..." che accompagna ogni
    // chnget trascinato o copiato (richiesta esplicita) - SENZA il prefisso
    // ";" del commento Csound classico, aggiunto da makeConfigCommentLine()
    // sotto. Una funzione per tipo, usando esattamente i nomi/etichette gia'
    // visti nelle card Edit (MIN/MAX/EXP/STEP per Float, MIN/MAX per Int,
    // INIT per Bool, OPTIONS/DEFAULT per Choice) cosi' il commento si legge
    // come le didascalie del pannello.
    juce::String makeFloatConfigComment (const CsoundAudioProcessor::ChannelParamSlot& slot)
    {
        return "SLIDER FLOAT: Min=" + formatConfigNumber (slot.minValue)
             + "; Max=" + formatConfigNumber (slot.maxValue)
             + "; Skew=" + formatConfigNumber (slot.skew)
             + "; Step=" + formatConfigNumber (slot.increment);
    }

    juce::String makeIntConfigComment (const CsoundAudioProcessor::IntParamSlot& slot)
    {
        return "SLIDER INT: Min=" + juce::String (slot.minValue) + "; Max=" + juce::String (slot.maxValue);
    }

    juce::String makeBoolConfigComment (const CsoundAudioProcessor::BoolParamSlot& slot)
    {
        return juce::String ("TOGGLE: Init=") + (slot.defaultValue ? "On" : "Off");
    }

    juce::String makeChoiceConfigComment (const CsoundAudioProcessor::ChoiceParamSlot& slot)
    {
        auto options = slot.optionLabels;
        options.removeEmptyStrings();
        const auto defaultLabel = CsoundAudioProcessor::getChoiceOptionLabel (slot, slot.defaultIndex);
        return "MENU: Options=" + options.joinIntoString (",") + "; Default=" + defaultLabel;
    }

    // Avvolge il contenuto di cui sopra in un commento Csound CLASSICO
    // (";...", non "/* */" - richiesto esplicitamente) da solo su una riga
    // PRIMA del chnget, non in coda alla stessa riga - stringa vuota se non
    // c'e' nessun commento (channelName vuoto, slot non ancora assegnato).
    juce::String makeConfigCommentLine (const juce::String& configComment)
    {
        return configComment.isNotEmpty() ? (";" + configComment + "\n") : juce::String();
    }

    // Stesso formato ESATTO che CsoundCodeEditor::itemDropped inserisce
    // trascinando la maniglia sull'editor di codice (vedi li'), MENO
    // l'indentazione - usato da copyButton (vedi *UnifiedRow) per copiare
    // negli appunti senza dover trascinare fisicamente.
    juce::String makeChngetClipboardText (const juce::String& channelName, const juce::String& configComment)
    {
        const auto varName = "k" + channelName.removeCharacters (" \t");
        return makeConfigCommentLine (configComment) + varName + " chnget \"" + channelName + "\"\n";
    }

    // Copia diretta negli appunti, richiamata da copyButton.onClick (vedi
    // *UnifiedRow) - niente piu' un sottomenu con una sola voce "Copy" (era
    // il modo in cui il tasto destro sulla maniglia mostrava l'azione,
    // rimosso: un bottone dedicato e' gia' di per se' un'azione esplicita,
    // non serve altra conferma). channelName vuoto -> no-op silenzioso
    // (riga ancora senza nome). configComment e' il contenuto SENZA
    // braces/delimitatori (vedi makeFloatConfigComment ecc. sopra) - puo'
    // essere vuoto (nessun commento aggiunto). notifyResult riceve il
    // messaggio da mostrare in consolle.
    void copyChngetToClipboard (const juce::String& channelName, const juce::String& configComment,
                                 const std::function<void (const juce::String&)>& notifyResult)
    {
        if (channelName.isEmpty())
            return;

        const auto clip = makeChngetClipboardText (channelName, configComment);
        juce::SystemClipboard::copyTextToClipboard (clip);

        if (notifyResult)
            notifyResult ("--- Copied to clipboard: " + clip.trim() + " ---");
    }

    // Icona edit propria di ogni riga (vedi editIconButton in ciascuna
    // delle 4 *UnifiedRow) - metafora "occhio/occhio chiuso" (Material
    // Design "visibility"/"visibility_off", viewBox 24x24), terzo tentativo
    // dopo l'icona "tune" e la coppia matita/spunta, entrambe scartate:
    // occhio APERTO in modalita' UI ("stai guardando i controlli, tocca per
    // modificare"), occhio CHIUSO/barrato in modalita' Edit ("stai
    // modificando, tocca per tornare ai controlli") - mostrata SOLO in
    // modalita' UI, vedi drawButtonText.
    juce::Path makeEyeIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zm0 "
            "12.5c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 "
            "3-3-1.34-3-3-3z");
    }

    // Occhio chiuso/barrato, mostrata SOLO mentre la riga e' in modalita'
    // Edit - insieme al cerchio pieno colorato disegnato da
    // drawButtonBackground per lo stesso stato (vedi li'), la coppia
    // icona-diversa + sfondo-pieno rende lo stato ATTIVO inequivocabile,
    // invece di affidarsi al solo colore del tratto a quella scala
    // (richiesta esplicita: "ci vogliono due icone oppure una selezione
    // piu' seria" - qui entrambe).
    juce::Path makeEyeOffIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M12 7c2.76 0 5 2.24 5 5 0 .65-.13 1.26-.36 1.83l2.92 2.92c1.51-1.26 2.7-2.89 3.43-4.75-1.73-4.39-6-7.5-"
            "11-7.5-1.4 0-2.74.25-3.98.7l2.16 2.16C10.74 7.13 11.35 7 12 7zM2 4.27l2.28 2.28.46.46C3.08 8.3 1.78 "
            "10.02 1 12c1.73 4.39 6 7.5 11 7.5 1.55 0 3.03-.3 4.38-.84l.42.42L19.73 22 21 20.73 3.27 3 2 4.27zM7.53 "
            "9.8l1.55 1.55c-.05.21-.08.43-.08.65 0 1.66 1.34 3 3 3 .22 0 .44-.03.65-.08l1.55 1.55c-.67.33-1.41.53-"
            "2.2.53-2.76 0-5-2.24-5-5 0-.79.2-1.53.53-2.2zm4.31-.78l3.15 3.15.02-.16c0-1.66-1.34-3-3-3l-.17.01z");
    }

    // Icona copyButton (Material Design "content_copy", viewBox 24x24) -
    // bottone dedicato per copiare il chnget negli appunti, al posto del
    // vecchio tasto destro sulla maniglia (rimosso: non esiste un "tasto
    // destro" su iOS, richiesta esplicita) - posizionato a sinistra di
    // editIconButton in ciascuna *UnifiedRow, vedi layoutRowIconButtons().
    juce::Path makeCopyIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M16 1H4c-1.1 0-2 .9-2 2v14h2V3h12V1zm3 4H8c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h11c1.1 0 2-.9 2-2V7c0-1.1-."
            "9-2-2-2zm0 16H8V7h11v14z");
    }
}

//==============================================================================
// CsoundParameterPanelLookAndFeel - vedi il commento in CsoundParameterEditor.h.
CsoundParameterPanelLookAndFeel::CsoundParameterPanelLookAndFeel()
{
    setColour (juce::ScrollBar::thumbColourId, kAccent.withAlpha (0.55f));
    setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId,            kFieldBg);
    setColour (juce::PopupMenu::textColourId,                  kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

    setColour (juce::Label::textColourId, kText);

    setColour (juce::ToggleButton::textColourId,         kText);
    setColour (juce::ToggleButton::tickColourId,         kAccent);
    setColour (juce::ToggleButton::tickDisabledColourId, kFieldOutline);

    setColour (juce::ComboBox::backgroundColourId, kFieldBg);
    setColour (juce::ComboBox::textColourId,       kText);
    setColour (juce::ComboBox::outlineColourId,    kFieldOutline);
    setColour (juce::ComboBox::arrowColourId,      kTextMuted);

    setColour (juce::Slider::backgroundColourId,        kFieldBg);
    setColour (juce::Slider::trackColourId,             kAccent);
    setColour (juce::Slider::thumbColourId,             kAccent);
    setColour (juce::Slider::textBoxTextColourId,        kText);
    setColour (juce::Slider::textBoxBackgroundColourId,  kFieldBg);
    setColour (juce::Slider::textBoxOutlineColourId,     kFieldOutline);

    setColour (juce::TextButton::buttonColourId,   kFieldBg);
    setColour (juce::TextButton::textColourOffId,  kText);
    setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
}

void CsoundParameterPanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    // Icona edit di ogni riga (vedi drawButtonText piu' sotto): la
    // distinzione tra i due stati NON puo' affidarsi solo al colore
    // dell'icona (poco leggibile a quella scala - "non si capisce quando e'
    // in edit o no") - qui disegniamo un cerchio PIENO (stesso trattamento
    // "serio" di "+"/rimozione) SOLO quando la riga e' in modalita' Edit
    // (button.getToggleState(), aggiornato da setEditMode()); in modalita'
    // UI resta piatta, senza alcun sfondo.
    if (button.getName() == "editToggle")
    {
        if (! button.getToggleState())
            return;

        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto colour = kAccent;
        if (shouldDrawButtonAsDown)
            colour = colour.darker (0.25f);
        else if (shouldDrawButtonAsHighlighted)
            colour = colour.brighter (0.12f);

        g.setColour (colour);
        g.fillEllipse (bounds);
        return;
    }

    // copyButton: nessuno stato "attivo" da segnalare (e' un'azione
    // singola, non un toggle come editToggle sopra) - resta piatto a
    // riposo, un cerchio semitrasparente solo in hover/pressione, per dare
    // un riscontro visivo al tocco senza un bordo/sfondo permanente che
    // competerebbe con editIconButton/removeButton accanto.
    if (button.getName() == "copyChnget")
    {
        if (! shouldDrawButtonAsHighlighted && ! shouldDrawButtonAsDown)
            return;

        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (kAccent.withAlpha (shouldDrawButtonAsDown ? 0.35f : 0.18f));
        g.fillEllipse (bounds);
        return;
    }

    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

    auto colour = backgroundColour;
    if (shouldDrawButtonAsDown)
        colour = colour.darker (0.25f);
    else if (shouldDrawButtonAsHighlighted)
        colour = colour.brighter (0.12f);

    g.setColour (colour);

    // Proprieta' dinamica "circular" (stessa convenzione usata da
    // CsoundLookAndFeel per il bottone Clear Console nella toolbar
    // principale) - il bottone "+" di questo pannello la usa per un aspetto
    // piu' da "azione primaria", invece di un TextButton piatto uguale a
    // tutti gli altri (richiesto esplicitamente: "il bottone + non e' bello").
    if (button.getProperties().getWithDefault ("circular", false))
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
    else
        g.fillRect (bounds);
}

void CsoundParameterPanelLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRect (0, 0, width, height);
}

void CsoundParameterPanelLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    if (! editor.isEnabled())
        return;

    auto bounds = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);

    if (editor.hasKeyboardFocus (true))
    {
        g.setColour (kAccent.withAlpha (0.25f));
        g.drawRect (bounds.expanded (1.5f), 2.5f);
        g.setColour (kAccent);
        g.drawRect (bounds, 1.4f);
    }
    else
    {
        g.setColour (kFieldOutline);
        g.drawRect (bounds, 1.0f);
    }
}

juce::Font CsoundParameterPanelLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (13.0f));
}

void CsoundParameterPanelLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                                      int buttonX, int buttonY, int buttonW, int buttonH,
                                                      juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
    const auto bg = box.findColour (juce::ComboBox::backgroundColourId);

    g.setColour (isButtonDown ? bg.brighter (0.1f) : bg);
    g.fillRect (bounds);

    g.setColour (box.hasKeyboardFocus (true) ? kAccent : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRect (bounds, 1.2f);

    constexpr float arrowWidth  = 8.0f;
    constexpr float arrowHeight = 4.5f;
    const auto arrowCentre = juce::Rectangle<int> (buttonX, buttonY, buttonW, buttonH).toFloat().getCentre();

    juce::Path arrow;
    arrow.addTriangle (arrowCentre.x - arrowWidth * 0.5f, arrowCentre.y - arrowHeight * 0.5f,
                        arrowCentre.x + arrowWidth * 0.5f, arrowCentre.y - arrowHeight * 0.5f,
                        arrowCentre.x,                     arrowCentre.y + arrowHeight * 0.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void CsoundParameterPanelLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.setColour (findColour (juce::PopupMenu::backgroundColourId));
    g.fillAll();
    g.setColour (kFieldOutline);
    g.drawRect (0, 0, width, height, 1);
}

void CsoundParameterPanelLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                                                   int standardMenuItemHeight, int& idealWidth, int& idealHeight)
{
    juce::LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);

    if (! isSeparator)
        idealHeight = juce::jmax (idealHeight, 26);
}

void CsoundParameterPanelLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                           bool isSeparator, bool isActive, bool isHighlighted,
                                                           bool isTicked, bool hasSubMenu, const juce::String& text,
                                                           const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                                           const juce::Colour* textColour)
{
    juce::ignoreUnused (icon);

    if (isSeparator)
    {
        auto line = area.reduced (10, 0);
        g.setColour (kFieldOutline);
        g.drawLine ((float) line.getX(), (float) line.getCentreY(), (float) line.getRight(), (float) line.getCentreY(), 1.0f);
        return;
    }

    auto itemArea = area.reduced (4, 1);

    if (isHighlighted && isActive)
    {
        g.setColour (kAccent);
        g.fillRect (itemArea);
    }

    auto textArea = itemArea.reduced (10, 0);

    if (isTicked)
    {
        auto tickColumn = textArea.removeFromLeft (16);
        constexpr float tickSize = 9.0f;
        const auto tickBounds = juce::Rectangle<float> (tickSize, tickSize)
                                     .withCentre (tickColumn.toFloat().getCentre());

        juce::Path tick;
        tick.startNewSubPath (tickBounds.getX(), tickBounds.getCentreY());
        tick.lineTo (tickBounds.getX() + tickSize * 0.35f, tickBounds.getBottom());
        tick.lineTo (tickBounds.getRight(), tickBounds.getY());
        g.setColour (isHighlighted ? juce::Colours::white : kAccent);
        g.strokePath (tick, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (! isActive ? kTextMuted
                             : (textColour != nullptr ? *textColour : (isHighlighted ? juce::Colours::white : kText)));
    g.setFont (juce::Font (juce::FontOptions (13.5f)));
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);

    if (hasSubMenu || shortcutKeyText.isNotEmpty())
    {
        g.setColour (kTextMuted);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawFittedText (shortcutKeyText, textArea, juce::Justification::centredRight, 1);
    }
}

void CsoundParameterPanelLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
                                                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                                       bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused (scrollbar);

    if (thumbSize <= 0)
        return;

    auto thumbBounds = isScrollbarVertical
                            ? juce::Rectangle<int> (x + 2, thumbStartPosition, juce::jmax (2, width - 4), thumbSize)
                            : juce::Rectangle<int> (thumbStartPosition, y + 2, thumbSize, juce::jmax (2, height - 4));

    const auto alpha = isMouseDown ? 0.85f : (isMouseOver ? 0.65f : 0.45f);
    g.setColour (kAccent.withAlpha (alpha));
    g.fillRect (thumbBounds);
}

void CsoundParameterPanelLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                          float sliderPos, float minSliderPos, float maxSliderPos,
                                                          const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused (minSliderPos, maxSliderPos, style);

    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto centreY = bounds.getCentreY();

    // Track sottile (sfondo + riempito fino alla maniglia) - niente Slider::
    // TextBoxRight da dover far stare nella stessa riga, lo spazio e' tutto
    // per il track + la maniglia.
    const float trackThickness = juce::jmax (3.0f, bounds.getHeight() * 0.18f);
    auto trackBounds = juce::Rectangle<float> (bounds.getX(), centreY - trackThickness * 0.5f, bounds.getWidth(), trackThickness);

    g.setColour (slider.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (trackBounds, trackThickness * 0.5f);

    g.setColour (slider.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (trackBounds.withWidth (juce::jlimit (0.0f, bounds.getWidth(), sliderPos - bounds.getX())),
                             trackThickness * 0.5f);

    // Maniglia (thumb) MOLTO piu' grande del normale (richiesta esplicita) -
    // quasi tutta l'altezza disponibile del componente, indipendentemente
    // da quanto e' sottile il track sopra: e' questo, non la riga intera,
    // che deve risultare grande/cliccabile.
    const float thumbDiameter = juce::jmax (trackThickness, bounds.getHeight());
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (juce::Rectangle<float> (thumbDiameter, thumbDiameter).withCentre ({ sliderPos, centreY }));
}

void CsoundParameterPanelLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    if (button.getName() != "pillToggle")
    {
        LookAndFeel_V4::drawToggleButton (g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        return;
    }

    auto bounds = button.getLocalBounds().toFloat();
    const bool isOn = button.getToggleState();

    g.setColour (kFieldBg.brighter (shouldDrawButtonAsDown ? 0.08f : (shouldDrawButtonAsHighlighted ? 0.04f : 0.0f)));
    g.fillRect (bounds);
    g.setColour (kFieldOutline);
    g.drawRect (bounds, 1.0f);

    auto content = bounds.reduced (10.0f, 0.0f);
    const float dotSize = juce::jmin (10.0f, content.getHeight() * 0.4f);
    auto dotArea = content.removeFromLeft (dotSize).withSizeKeepingCentre (dotSize, dotSize);
    g.setColour (isOn ? kToggleAccent : kPlaceholder);
    g.fillEllipse (dotArea);

    content.removeFromLeft (8.0f);
    g.setColour (isOn ? kText : kTextMuted);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawFittedText (isOn ? "ON" : "OFF", content.toNearestInt(), juce::Justification::centredLeft, 1);
}

void CsoundParameterPanelLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    // Icona edit propria di ogni riga (riconosciuta dal nome "editToggle"):
    // due icone DIVERSE per i due stati (vedi button.getToggleState(),
    // aggiornato da setEditMode() - richiesto esplicitamente, un'icona
    // sola non rendeva chiaro lo stato) - occhio aperto (makeEyeIconPath,
    // "tocca per modificare") in modalita' UI, occhio chiuso/barrato
    // (makeEyeOffIconPath, bianco su cerchio pieno colorato - vedi
    // drawButtonBackground - "tocca per tornare ai controlli") in modalita'
    // Edit.
    if (button.getName() == "editToggle")
    {
        const bool editing = button.getToggleState();
        g.setColour (editing ? juce::Colours::white : kTextMuted);

        auto bounds = button.getLocalBounds().toFloat();
        auto icon = editing ? makeEyeOffIconPath() : makeEyeIconPath();

        const float iconSize = bounds.getHeight() * 0.56f; // niente piu' un tetto fisso a 14px: l'icona ora scala CON il bottone (ingrandito, richiesta esplicita)
        auto iconArea = bounds.withSizeKeepingCentre (iconSize, iconSize);
        icon.scaleToFit (iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true);
        g.fillPath (icon);
        return;
    }

    // copyButton: stesso trattamento "a riposo muto, colore in evidenza su
    // azione" dell'icona "occhio" sopra, ma SENZA i due stati (e' un'azione
    // singola, non un toggle).
    if (button.getName() == "copyChnget")
    {
        g.setColour (kTextMuted);

        auto bounds = button.getLocalBounds().toFloat();
        auto icon = makeCopyIconPath();

        const float iconSize = bounds.getHeight() * 0.5f;
        auto iconArea = bounds.withSizeKeepingCentre (iconSize, iconSize);
        icon.scaleToFit (iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true);
        g.fillPath (icon);
        return;
    }

    // Bottone di rimozione (riconosciuto dal nome "removeParam"): cerchio
    // rosso (kDanger, vedi la proprieta' "circular" impostata sul bottone -
    // stesso meccanismo generico di drawButtonBackground usato da "+") con
    // un "-" disegnato a mano, al posto del vecchio quadrato con la "x" -
    // richiesto esplicitamente. Controllato PRIMA del ramo "circular"
    // generico sotto (che altrimenti disegnerebbe un "+" anche qui, dato
    // che removeButton ha la stessa proprieta' impostata per lo sfondo
    // circolare).
    if (button.getName() == "removeParam")
    {
        auto bounds = button.getLocalBounds().toFloat();
        const float barSize = bounds.getHeight() * 0.42f;
        const float thickness = juce::jmax (2.0f, bounds.getHeight() * 0.12f);
        const auto centre = bounds.getCentre();

        juce::Path minus;
        minus.addRoundedRectangle (centre.x - barSize * 0.5f, centre.y - thickness * 0.5f, barSize, thickness, thickness * 0.3f);

        g.setColour (juce::Colours::white);
        g.fillPath (minus);
        return;
    }

    // Bottone menu (riconosciuto dal nome "burgerMenu"): tre barre
    // orizzontali ("hamburger"), disegnate a mano come i bottoni "+"/
    // rimozione qui sopra - apre Undo/Redo/Remove Parameters (vedi
    // showPanelMenu()).
    if (button.getName() == "burgerMenu")
    {
        auto bounds = button.getLocalBounds().toFloat();
        const float barWidth = bounds.getWidth() * 0.46f;
        const float thickness = juce::jmax (1.6f, bounds.getHeight() * 0.09f);
        const float gap = thickness * 1.8f;
        const auto centre = bounds.getCentre();

        juce::Path bars;
        for (int i = -1; i <= 1; ++i)
            bars.addRoundedRectangle (centre.x - barWidth * 0.5f, centre.y + (float) i * gap - thickness * 0.5f,
                                        barWidth, thickness, thickness * 0.3f);

        g.setColour (juce::Colours::white);
        g.fillPath (bars);
        return;
    }

    // Bottone multifunzione del pannello Parametri (riconosciuto dal nome
    // "paramsMenu", title bar - vedi CsoundParameterMappingPanel::
    // showAddMenu()): icona a "slider/equalizzatore" (tre barre orizzontali
    // di lunghezza DIVERSA, ciascuna con una maniglia circolare) invece
    // della "+" generica, usata finche' il bottone faceva solo "aggiungi
    // parametro" - richiesto esplicitamente ("cambia icona... dal momento
    // che ormai e' multifunzionale": apre anche Open/Close Config, Remove,
    // Reset). Disegnata a mano come gli altri bottoni qui sopra, non un
    // glifo di font.
    if (button.getName() == "paramsMenu")
    {
        auto bounds = button.getLocalBounds().toFloat();
        const float maxBarWidth = bounds.getWidth() * 0.5f;
        const float thickness = juce::jmax (1.4f, bounds.getHeight() * 0.07f);
        const float gap = bounds.getHeight() * 0.19f;
        const float knobRadius = thickness * 1.1f;
        const auto centre = bounds.getCentre();
        const float left = centre.x - maxBarWidth * 0.5f;

        // Lunghezze decrescenti (100%/70%/40%) e maniglia a una posizione
        // diversa su ciascuna barra - la "firma visiva" classica di un
        // pannello di controllo parametri, distinta sia dalle 3 barre
        // UGUALI dell'hamburger sia dalla croce della "+".
        static constexpr float widths[3]    = { 1.0f, 0.7f, 0.42f };
        static constexpr float knobPos[3]   = { 0.78f, 0.42f, 0.62f };

        juce::Path p;
        for (int i = 0; i < 3; ++i)
        {
            const float y = centre.y + ((float) i - 1.0f) * gap;
            const float barWidth = maxBarWidth * widths[i];
            p.addRoundedRectangle (left, y - thickness * 0.5f, barWidth, thickness, thickness * 0.4f);
            p.addEllipse (left + barWidth * knobPos[i] - knobRadius, y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        }

        g.setColour (juce::Colours::white);
        g.fillPath (p);
        return;
    }

    // Bottone "+"/Add (proprieta' dinamica "circular"): icona "+" disegnata
    // a mano, non il carattere di testo "+" - a quella scala un glifo di
    // font risultava sottile/poco centrato ("il bottone + non e' bello").
    if (button.getProperties().getWithDefault ("circular", false))
    {
        auto bounds = button.getLocalBounds().toFloat();
        const float plusSize = bounds.getHeight() * 0.42f;
        const float thickness = juce::jmax (2.0f, bounds.getHeight() * 0.12f);
        const auto centre = bounds.getCentre();

        juce::Path plus;
        plus.addRoundedRectangle (centre.x - plusSize * 0.5f, centre.y - thickness * 0.5f, plusSize, thickness, thickness * 0.3f);
        plus.addRoundedRectangle (centre.x - thickness * 0.5f, centre.y - plusSize * 0.5f, thickness, plusSize, thickness * 0.3f);

        g.setColour (juce::Colours::white);
        g.fillPath (plus);
        return;
    }

    // Tutti gli altri bottoni: rendering di testo standard di LookAndFeel_V4.
    juce::LookAndFeel_V4::drawButtonText (g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

//==============================================================================
// Scheletro comune delle card Edit - vedi il commento in testa alla
// dichiarazione in CsoundParameterEditor.h.
void CsoundParameterMappingPanel::layoutCardSkeleton (juce::Rectangle<int> full,
                                                        juce::Rectangle<int>& handleStrip,
                                                        juce::Rectangle<int>& typeLabelArea,
                                                        juce::Rectangle<int>& removeArea,
                                                        juce::Rectangle<int>& nameArea,
                                                        juce::Rectangle<int>& fieldsArea)
{
    handleStrip = full.removeFromLeft (handleStripWidth);

    auto content = full.reduced (cardPaddingH, cardPaddingV);

    auto header = content.removeFromTop (cardHeaderHeight);
    // Remove button a DESTRA, adiacente a copyButton (a sua volta adiacente
    // a editIconButton - richiesta esplicita: i bottoni locali devono stare
    // vicini) - copyButton ED editIconButton sono disegnati SOPRA da
    // layoutRowIconButtons (nel wrapper *UnifiedRow, stessa geometria
    // cardHeaderHeight/rowEditIconSize/rowEditIconMargin/rowIconButtonGap),
    // quindi qui si riserva prima il loro spazio senza piazzarci nulla, poi
    // si mette removeArea subito alla loro sinistra. Ordine da destra a
    // sinistra nell'header: editIconButton, copyButton, removeArea,
    // typeLabelArea.
    header.removeFromRight (rowEditIconMargin);
    const int editIconSize = juce::jmin (cardHeaderHeight, rowEditIconSize);
    header.removeFromRight (editIconSize);           // spazio di editIconButton
    header.removeFromRight (rowIconButtonGap);
    header.removeFromRight (editIconSize);           // spazio di copyButton
    header.removeFromRight (rowIconButtonGap);
    removeArea = header.removeFromRight (editIconSize).withSizeKeepingCentre (editIconSize, editIconSize);
    header.removeFromRight (rowIconButtonGap);
    typeLabelArea = header;

    content.removeFromTop (cardHeaderGap);
    nameArea = content.removeFromTop (cardNameHeight);
    content.removeFromTop (cardNameGap);

    fieldsArea = content;
}

// Scheletro compatto delle card UI - vedi il commento in testa alla
// dichiarazione in CsoundParameterEditor.h. Stessa logica di
// layoutCardSkeleton() sopra ma con la geometria uiCard* (piu' bassa) e
// senza bottone di rimozione (GenericParamRow non ne ha uno).
void CsoundParameterMappingPanel::layoutUiCardSkeleton (juce::Rectangle<int> full,
                                                          juce::Rectangle<int>& handleStrip,
                                                          juce::Rectangle<int>& typeLabelArea,
                                                          juce::Rectangle<int>& nameArea,
                                                          juce::Rectangle<int>& fieldsArea)
{
    handleStrip = full.removeFromLeft (handleStripWidth);

    auto content = full.reduced (cardPaddingH, uiCardPaddingV);

    typeLabelArea = content.removeFromTop (uiCardHeaderHeight);
    content.removeFromTop (uiCardHeaderGap);

    nameArea = content.removeFromTop (uiCardNameHeight);
    content.removeFromTop (uiCardNameGap);

    fieldsArea = content;
}

// Icona "edit" propria di ogni riga (vedi editIconButton in ciascuna delle
// 4 *UnifiedRow) - solo il nome ("editToggle", riconosciuto da
// drawButtonBackground/drawButtonText per disegnare il cerchio pieno SOLO
// in modalita' Edit e la matita/spunta a seconda dello stato, vedi li'):
// lo stato stesso (toggle state, colori, icona) e' tutto gestito da
// setEditMode() ad ogni cambio, non qui alla creazione.
void CsoundParameterMappingPanel::setupRowEditIconButton (juce::TextButton& button)
{
    button.setName ("editToggle");
}

// Icona "copy" propria di ogni riga (vedi copyButton in ciascuna delle 4
// *UnifiedRow) - solo il nome ("copyChnget", riconosciuto da
// drawButtonBackground/drawButtonText per disegnare l'icona a mano) - al
// posto del vecchio tasto destro sulla maniglia (rimosso, richiesta
// esplicita: non esiste su iOS). onClick e' impostato dal chiamante (ogni
// *UnifiedRow conosce il proprio slot/kind, serve per costruire la riga
// chnget giusta - vedi i 4 costruttori).
void CsoundParameterMappingPanel::setupRowCopyButton (juce::TextButton& button)
{
    button.setName ("copyChnget");
    button.setTooltip ("Copy chnget line to clipboard");
}

// Posiziona copyButton ED editIconButton insieme nell'angolo in alto a
// DESTRA della card, SEMPRE (richiesta esplicita) - sia in modalita' UI
// (layoutUiCardSkeleton, header piu' basso, niente bottone di rimozione)
// sia in modalita' Edit (layoutCardSkeleton, dove il bottone di rimozione
// e' ora adiacente a copyButton, subito alla sua sinistra - vedi li').
// copyButton e' SEMPRE immediatamente a sinistra di editIconButton
// (richiesta esplicita: "un bottone sulla sx di edit per il copy").
void CsoundParameterMappingPanel::layoutRowIconButtons (juce::Rectangle<int> fullBounds, bool rowEditMode,
                                                          juce::Component& copyButton, juce::Component& editButton)
{
    const int padV   = rowEditMode ? cardPaddingV     : uiCardPaddingV;
    const int headerH = rowEditMode ? cardHeaderHeight : uiCardHeaderHeight;

    auto header = fullBounds.reduced (cardPaddingH, padV).removeFromTop (headerH);

    header.removeFromRight (rowEditIconMargin);
    const int iconSize = juce::jmin (headerH, rowEditIconSize);
    editButton.setBounds (header.removeFromRight (iconSize).withSizeKeepingCentre (iconSize, iconSize));
    header.removeFromRight (rowIconButtonGap);
    copyButton.setBounds (header.removeFromRight (iconSize).withSizeKeepingCentre (iconSize, iconSize));
}

void CsoundParameterMappingPanel::layoutCaptionedField (juce::Rectangle<int> cell, juce::Label& caption, juce::Component& field)
{
    caption.setBounds (cell.removeFromTop (cardFieldCaptionHeight));
    cell.removeFromTop (cardFieldCaptionGap);
    field.setBounds (cell.removeFromTop (cardFieldBoxHeight));
}

void CsoundParameterMappingPanel::paintCardChrome (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour accent,
                                                     juce::Rectangle<int> handleStrip, bool showHandleDots, bool handleHovered,
                                                     bool isEditingCard)
{
    // kCardBgEditing (piu' chiaro) SOLO per le card Edit (ParamRow/
    // IntParamRow/BoolParamRow/ChoiceParamRow) - GenericParamRow (UI) passa
    // sempre false: il colore di sfondo diverso e' l'indicazione visiva
    // immediata di "sei in modalita' Edit", richiesta esplicitamente.
    g.setColour (isEditingCard ? kCardBgEditing : kCardBg);
    g.fillRect (bounds);

    g.setColour (accent);
    g.fillRect (bounds.getX(), bounds.getY(), 5, bounds.getHeight());

    if (! showHandleDots)
        return;

    g.setColour (handleHovered ? juce::Colours::white : juce::Colours::white.withAlpha (0.6f));
    const auto centre = handleStrip.toFloat().getCentre();
    constexpr float dotSize = 3.4f;
    constexpr float dotSpacingX = 7.0f;
    constexpr float dotSpacingY = 7.0f;
    for (int row = -1; row <= 1; ++row)
    {
        for (int col = 0; col <= 1; ++col)
        {
            const float dx = (col == 0 ? -1.0f : 1.0f) * (dotSpacingX / 2.0f);
            const float dy = (float) row * dotSpacingY;
            g.fillEllipse (centre.x + dx - dotSize / 2.0f, centre.y + dy - dotSize / 2.0f, dotSize, dotSize);
        }
    }
}

int CsoundParameterMappingPanel::fieldsAvailableWidth (int fullCardWidth)
{
    return juce::jmax (0, fullCardWidth - handleStripWidth - cardPaddingH * 2);
}

int CsoundParameterMappingPanel::computeWrappedFieldRows (int availableWidth, std::initializer_list<int> fieldWidths, int gap)
{
    int x = 0;
    int rows = 1;

    for (int w : fieldWidths)
    {
        if (x != 0 && x + gap + w > availableWidth)
        {
            ++rows;
            x = w;
        }
        else
        {
            x += (x == 0 ? 0 : gap) + w;
        }
    }

    return rows;
}

void CsoundParameterMappingPanel::layoutWrappedFields (juce::Rectangle<int> fieldsArea, std::initializer_list<int> fieldWidths, int gap,
                                                         const std::function<void (int, juce::Rectangle<int>)>& onPlaceField)
{
    // STESSO algoritmo di computeWrappedFieldRows() (vedi il commento li'):
    // qui pero' si piazzano davvero le celle invece di contarle soltanto -
    // deve restare IDENTICO, altrimenti il numero di righe previsto da
    // getPreferredHeight() e quello usato davvero da resized() potrebbero
    // andare fuori sincrono.
    const int rowHeightPx = cardFieldCaptionHeight + cardFieldCaptionGap + cardFieldBoxHeight;
    const int availableWidth = fieldsArea.getWidth();

    int x = 0;
    int row = 0;
    int index = 0;

    for (int w : fieldWidths)
    {
        if (x != 0 && x + gap + w > availableWidth)
        {
            ++row;
            x = 0;
        }

        auto cell = juce::Rectangle<int> (fieldsArea.getX() + x,
                                            fieldsArea.getY() + row * (rowHeightPx + cardFieldRowGap),
                                            w, rowHeightPx);
        onPlaceField (index, cell);

        x += w + gap;
        ++index;
    }
}

//==============================================================================
// ParamRow ("SLIDER" nel mockup, slot Float) - vedi il commento in testa
// alla dichiarazione in CsoundParameterEditor.h.
CsoundParameterMappingPanel::ParamRow::ParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex)
    : processor (processorToEdit),
      index (slotIndex)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);

    // Vedi il commento in CsoundParameterEditor.h: senza questo il click
    // sulla maniglia porterebbe il focus su channelNameEditor.
    setMouseClickGrabsKeyboardFocus (false);

    setupTypeLabel (typeLabel, "SLIDER FLOAT " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + " CHANNEL", kSliderAccent);
    addAndMakeVisible (typeLabel);

    channelNameEditor.setTextToShowWhenEmpty ("(no channel)", kPlaceholder);
    channelNameEditor.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    setupFieldCaption (minCaption, "MIN");
    addAndMakeVisible (minCaption);
    minEditor.setInputRestrictions (0, "0123456789.,-eE");
    minEditor.setJustification (juce::Justification::centredLeft);
    minEditor.addListener (this);
    applyDarkFieldColours (minEditor);
    addAndMakeVisible (minEditor);

    setupFieldCaption (maxCaption, "MAX");
    addAndMakeVisible (maxCaption);
    maxEditor.setInputRestrictions (0, "0123456789.,-eE");
    maxEditor.setJustification (juce::Justification::centredLeft);
    maxEditor.addListener (this);
    applyDarkFieldColours (maxEditor);
    addAndMakeVisible (maxEditor);

    setupFieldCaption (initCaption, "INIT");
    addAndMakeVisible (initCaption);
    defaultEditor.setInputRestrictions (0, "0123456789.,-eE");
    defaultEditor.setJustification (juce::Justification::centredLeft);
    defaultEditor.addListener (this);
    applyDarkFieldColours (defaultEditor);
    addAndMakeVisible (defaultEditor);

    setupFieldCaption (expCaption, "SKEW");
    addAndMakeVisible (expCaption);
    skewEditor.setInputRestrictions (0, "0123456789.,-eE");
    skewEditor.setJustification (juce::Justification::centredLeft);
    skewEditor.addListener (this);
    applyDarkFieldColours (skewEditor);
    addAndMakeVisible (skewEditor);

    setupFieldCaption (stepCaption, "STEP");
    addAndMakeVisible (stepCaption);
    incrementEditor.setInputRestrictions (0, "0123456789.,-eE");
    incrementEditor.setJustification (juce::Justification::centredLeft);
    incrementEditor.addListener (this);
    applyDarkFieldColours (incrementEditor);
    addAndMakeVisible (incrementEditor);

    // Cerchio rosso con un "-" disegnato a mano (vedi drawButtonText,
    // branch "removeParam") invece del vecchio quadrato con la "x" - stesso
    // meccanismo della proprieta' dinamica "circular" gia' usata da "+".
    removeButton.setName ("removeParam");
    removeButton.getProperties().set ("circular", true);
    removeButton.setColour (juce::TextButton::buttonColourId, kDanger);
    removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    removeButton.setTooltip ("Remove this parameter");
    removeButton.onClick = [this]
    {
        // Il nome e lo slot COMPLETO vanno letti QUI, prima di svuotare:
        // dopo commitFromFields() sono gia' vuoti - "before" per Undo (vedi
        // pushUndo in CsoundParameterEditor.h).
        const auto removedName = channelNameEditor.getText().trim();
        const auto beforeSlot = processor.getChannelParamSlot (index);
        channelNameEditor.setText ({}, false);
        commitFromFields();
        const auto afterSlot = processor.getChannelParamSlot (index);

        if (onCopiedToClipboard && removedName.isNotEmpty())
            onCopiedToClipboard ("--- Removed parameter: " + removedName + " ---");

        // &proc/idx catturati esplicitamente (NON tramite `this`): queste
        // due lambda possono essere eseguite molto piu' tardi, quando
        // QUESTA riga potrebbe essere stata gia' distrutta da un
        // rebuildUnifiedRows() per un'altra azione - vedi il commento su
        // pushUndo in CsoundParameterEditor.h.
        if (pushUndo)
        {
            auto& proc = processor;
            const int idx = index;
            pushUndo ([&proc, idx, afterSlot]  { proc.setChannelParamSlot (idx, afterSlot); },
                       [&proc, idx, beforeSlot] { proc.setChannelParamSlot (idx, beforeSlot); });
        }

        if (onRemoveRequested)
            onRemoveRequested();
    };
    addAndMakeVisible (removeButton);

    refreshFromProcessor();
}

void CsoundParameterMappingPanel::ParamRow::focusChannelNameField()
{
    channelNameEditor.grabKeyboardFocus();
    channelNameEditor.selectAll();
}

void CsoundParameterMappingPanel::ParamRow::refreshFromProcessor()
{
    const auto slot = processor.getChannelParamSlot (index);

    channelNameEditor.setText (slot.channelName, false);
    minEditor.setText (juce::String (slot.minValue), false);
    maxEditor.setText (juce::String (slot.maxValue), false);
    defaultEditor.setText (juce::String (slot.defaultValue), false);
    skewEditor.setText (juce::String (slot.skew), false);
    incrementEditor.setText (juce::String (slot.increment), false);
    removeButton.setVisible (slot.channelName.isNotEmpty());
}

void CsoundParameterMappingPanel::ParamRow::commitFromFields()
{
    CsoundAudioProcessor::ChannelParamSlot slot;

    slot.channelName = channelNameEditor.getText().trim();

    const auto minText = minEditor.getText().trim();
    const auto maxText = maxEditor.getText().trim();
    const auto defaultText = defaultEditor.getText().trim();
    slot.minValue = minText.isNotEmpty() ? minText.getFloatValue() : 0.0f;
    slot.maxValue = maxText.isNotEmpty() ? maxText.getFloatValue() : 1.0f;
    slot.defaultValue = defaultText.isNotEmpty() ? defaultText.getFloatValue() : slot.minValue;

    const auto skewText = skewEditor.getText().trim();
    const auto incrementText = incrementEditor.getText().trim();
    slot.skew = skewText.isNotEmpty() ? skewText.getFloatValue() : 1.0f;
    slot.increment = incrementText.isNotEmpty() ? incrementText.getFloatValue() : 0.001f;

    processor.setChannelParamSlot (index, slot);

    removeButton.setVisible (slot.channelName.isNotEmpty());
    repaint();
}

void CsoundParameterMappingPanel::ParamRow::notifyRemovedIfEmpty()
{
    if (processor.getChannelParamSlot (index).channelName.isEmpty() && onRemoveRequested)
        onRemoveRequested();
}

void CsoundParameterMappingPanel::ParamRow::notifyCommittedIfNonEmpty()
{
    if (processor.getChannelParamSlot (index).channelName.isNotEmpty() && onCommittedNonEmpty)
        onCommittedNonEmpty();
}

void CsoundParameterMappingPanel::ParamRow::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    commitFromFields();
    pushPendingUndoIfAny();
    editor.giveAwayKeyboardFocus();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::ParamRow::textEditorEscapeKeyPressed (juce::TextEditor& editor)
{
    textEditorReturnKeyPressed (editor);
}

void CsoundParameterMappingPanel::ParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
    pushPendingUndoIfAny();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::ParamRow::textEditorTextChanged (juce::TextEditor&)
{
    // Commit ad ogni carattere (non solo a Return/focus perso): cosi' la
    // maniglia di trascinamento compare/scompare SUBITO - ma le notifiche
    // di rimozione/promozione restano SOLO su Return/focus perso (vedi
    // sopra): farle scattare a META' di una digitazione distruggerebbe la
    // riga sotto le dita dell'utente.
    // Primo carattere modificato dopo l'ultimo commit: cattura lo slot PRIMA
    // che venga sovrascritto, cosi' pushPendingUndoIfAny() (chiamata su
    // Return/focus perso/Esc) sa da dove ripartire con Undo.
    if (! hasBeforeEditSlot)
    {
        beforeEditSlot = processor.getChannelParamSlot (index);
        hasBeforeEditSlot = true;
    }
    commitFromFields();
}

void CsoundParameterMappingPanel::ParamRow::pushPendingUndoIfAny()
{
    if (! hasBeforeEditSlot)
        return;

    const auto before = beforeEditSlot;
    const auto after = processor.getChannelParamSlot (index);
    hasBeforeEditSlot = false;

    if (pushUndo)
    {
        auto& proc = processor;
        const int idx = index;
        pushUndo ([&proc, idx, after]  { proc.setChannelParamSlot (idx, after); },
                   [&proc, idx, before] { proc.setChannelParamSlot (idx, before); });
    }
}

void CsoundParameterMappingPanel::ParamRow::paint (juce::Graphics& g)
{
    const bool hasName = processor.getChannelParamSlot (index).channelName.isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), kSliderAccent, handleBounds, hasName, handleHovered, true);
}

void CsoundParameterMappingPanel::ParamRow::resized()
{
    juce::Rectangle<int> handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea;
    CsoundParameterMappingPanel::layoutCardSkeleton (getLocalBounds(), handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea);

    handleBounds = handleStrip;
    typeLabel.setBounds (typeLabelArea);
    removeButton.setBounds (removeArea);
    channelNameEditor.setBounds (nameArea);

    // Min/Max/Init/Exp/Step vanno a capo (invece di far comparire una
    // scrollbar orizzontale) quando il pannello e' troppo stretto per
    // contenerli tutti su una riga sola - STESSO algoritmo usato da
    // FloatUnifiedRow::getPreferredHeight() per calcolare l'altezza, vedi
    // layoutWrappedFields()/computeWrappedFieldRows().
    juce::Label* const captions[] = { &minCaption, &maxCaption, &initCaption, &expCaption, &stepCaption };
    juce::TextEditor* const editors[] = { &minEditor, &maxEditor, &defaultEditor, &skewEditor, &incrementEditor };

    CsoundParameterMappingPanel::layoutWrappedFields (fieldsArea,
        { cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldStepWidth },
        cardFieldGap,
        [&] (int index, juce::Rectangle<int> cell)
        {
            CsoundParameterMappingPanel::layoutCaptionedField (cell, *captions[index], *editors[index]);
        });
}

void CsoundParameterMappingPanel::ParamRow::mouseDown (const juce::MouseEvent& event)
{
    // Il tasto destro sulla maniglia copiava il chnget negli appunti
    // (showCopyChngetMenu) - rimosso (richiesta esplicita, l'azione non e'
    // raggiungibile su iOS, dove non esiste un "tasto destro"): la stessa
    // copia e' ora un bottone dedicato, copyButton, a sinistra di
    // editIconButton (vedi *UnifiedRow).
    draggingFromHandle = handleBounds.contains (event.getPosition());
}

void CsoundParameterMappingPanel::ParamRow::mouseMove (const juce::MouseEvent& event)
{
    updateHandleHover (event.getPosition());
}

void CsoundParameterMappingPanel::ParamRow::mouseExit (const juce::MouseEvent&)
{
    if (handleHovered)
    {
        handleHovered = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CsoundParameterMappingPanel::ParamRow::updateHandleHover (juce::Point<int> position)
{
    const bool nowHovered = handleBounds.contains (position)
                             && processor.getChannelParamSlot (index).channelName.isNotEmpty();

    if (nowHovered != handleHovered)
    {
        handleHovered = nowHovered;
        setMouseCursor (handleHovered ? juce::MouseCursor::DraggingHandCursor
                                       : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CsoundParameterMappingPanel::ParamRow::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingFromHandle)
        return;

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr || container->isDragAndDropActive())
        return;

    const auto slot = processor.getChannelParamSlot (index);
    if (slot.channelName.isEmpty())
        return;

    container->startDragging ("csoundChannel:" + slot.channelName + "\x01" + makeFloatConfigComment (slot), this);
}

//==============================================================================
// IntParamRow ("KNOB" nel mockup, slot Int).
CsoundParameterMappingPanel::IntParamRow::IntParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex)
    : processor (processorToEdit),
      index (slotIndex)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    setMouseClickGrabsKeyboardFocus (false);

    setupTypeLabel (typeLabel, "SLIDER INT " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + " CHANNEL", kKnobAccent);
    addAndMakeVisible (typeLabel);

    channelNameEditor.setTextToShowWhenEmpty ("(no channel)", kPlaceholder);
    channelNameEditor.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    setupFieldCaption (minCaption, "MIN");
    addAndMakeVisible (minCaption);
    minEditor.setInputRestrictions (0, "0123456789-");
    minEditor.setJustification (juce::Justification::centredLeft);
    minEditor.addListener (this);
    applyDarkFieldColours (minEditor);
    addAndMakeVisible (minEditor);

    setupFieldCaption (maxCaption, "MAX");
    addAndMakeVisible (maxCaption);
    maxEditor.setInputRestrictions (0, "0123456789-");
    maxEditor.setJustification (juce::Justification::centredLeft);
    maxEditor.addListener (this);
    applyDarkFieldColours (maxEditor);
    addAndMakeVisible (maxEditor);

    setupFieldCaption (initCaption, "INIT");
    addAndMakeVisible (initCaption);
    defaultEditor.setInputRestrictions (0, "0123456789-");
    defaultEditor.setJustification (juce::Justification::centredLeft);
    defaultEditor.addListener (this);
    applyDarkFieldColours (defaultEditor);
    addAndMakeVisible (defaultEditor);

    // Cerchio rosso con un "-" disegnato a mano (vedi drawButtonText,
    // branch "removeParam") invece del vecchio quadrato con la "x" - stesso
    // meccanismo della proprieta' dinamica "circular" gia' usata da "+".
    removeButton.setName ("removeParam");
    removeButton.getProperties().set ("circular", true);
    removeButton.setColour (juce::TextButton::buttonColourId, kDanger);
    removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    removeButton.setTooltip ("Remove this parameter");
    removeButton.onClick = [this]
    {
        // Vedi il commento identico su ParamRow::removeButton.onClick sopra.
        const auto removedName = channelNameEditor.getText().trim();
        const auto beforeSlot = processor.getIntParamSlot (index);
        channelNameEditor.setText ({}, false);
        commitFromFields();
        const auto afterSlot = processor.getIntParamSlot (index);

        if (onCopiedToClipboard && removedName.isNotEmpty())
            onCopiedToClipboard ("--- Removed parameter: " + removedName + " ---");

        if (pushUndo)
        {
            auto& proc = processor;
            const int idx = index;
            pushUndo ([&proc, idx, afterSlot]  { proc.setIntParamSlot (idx, afterSlot); },
                       [&proc, idx, beforeSlot] { proc.setIntParamSlot (idx, beforeSlot); });
        }

        if (onRemoveRequested)
            onRemoveRequested();
    };
    addAndMakeVisible (removeButton);

    refreshFromProcessor();
}

void CsoundParameterMappingPanel::IntParamRow::focusChannelNameField()
{
    channelNameEditor.grabKeyboardFocus();
    channelNameEditor.selectAll();
}

void CsoundParameterMappingPanel::IntParamRow::refreshFromProcessor()
{
    const auto slot = processor.getIntParamSlot (index);

    channelNameEditor.setText (slot.channelName, false);
    minEditor.setText (juce::String (slot.minValue), false);
    maxEditor.setText (juce::String (slot.maxValue), false);
    defaultEditor.setText (juce::String (slot.defaultValue), false);
    removeButton.setVisible (slot.channelName.isNotEmpty());
}

void CsoundParameterMappingPanel::IntParamRow::commitFromFields()
{
    CsoundAudioProcessor::IntParamSlot slot;

    slot.channelName = channelNameEditor.getText().trim();

    const auto minText = minEditor.getText().trim();
    const auto maxText = maxEditor.getText().trim();
    const auto defaultText = defaultEditor.getText().trim();
    slot.minValue = minText.isNotEmpty() ? minText.getIntValue() : 0;
    slot.maxValue = maxText.isNotEmpty() ? maxText.getIntValue() : 127;
    slot.defaultValue = defaultText.isNotEmpty() ? defaultText.getIntValue() : slot.minValue;

    processor.setIntParamSlot (index, slot);

    removeButton.setVisible (slot.channelName.isNotEmpty());
    repaint();
}

void CsoundParameterMappingPanel::IntParamRow::notifyRemovedIfEmpty()
{
    if (processor.getIntParamSlot (index).channelName.isEmpty() && onRemoveRequested)
        onRemoveRequested();
}

void CsoundParameterMappingPanel::IntParamRow::notifyCommittedIfNonEmpty()
{
    if (processor.getIntParamSlot (index).channelName.isNotEmpty() && onCommittedNonEmpty)
        onCommittedNonEmpty();
}

void CsoundParameterMappingPanel::IntParamRow::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    commitFromFields();
    pushPendingUndoIfAny();
    editor.giveAwayKeyboardFocus();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::IntParamRow::textEditorEscapeKeyPressed (juce::TextEditor& editor)
{
    textEditorReturnKeyPressed (editor);
}

void CsoundParameterMappingPanel::IntParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
    pushPendingUndoIfAny();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::IntParamRow::textEditorTextChanged (juce::TextEditor&)
{
    if (! hasBeforeEditSlot)
    {
        beforeEditSlot = processor.getIntParamSlot (index);
        hasBeforeEditSlot = true;
    }
    commitFromFields();
}

void CsoundParameterMappingPanel::IntParamRow::pushPendingUndoIfAny()
{
    if (! hasBeforeEditSlot)
        return;

    const auto before = beforeEditSlot;
    const auto after = processor.getIntParamSlot (index);
    hasBeforeEditSlot = false;

    if (pushUndo)
    {
        auto& proc = processor;
        const int idx = index;
        pushUndo ([&proc, idx, after]  { proc.setIntParamSlot (idx, after); },
                   [&proc, idx, before] { proc.setIntParamSlot (idx, before); });
    }
}

void CsoundParameterMappingPanel::IntParamRow::paint (juce::Graphics& g)
{
    const bool hasName = processor.getIntParamSlot (index).channelName.isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), kKnobAccent, handleBounds, hasName, handleHovered, true);
}

void CsoundParameterMappingPanel::IntParamRow::resized()
{
    juce::Rectangle<int> handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea;
    CsoundParameterMappingPanel::layoutCardSkeleton (getLocalBounds(), handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea);

    handleBounds = handleStrip;
    typeLabel.setBounds (typeLabelArea);
    removeButton.setBounds (removeArea);
    channelNameEditor.setBounds (nameArea);

    // Stesso meccanismo "a capo" di ParamRow::resized() - qui raramente
    // serve (3 campi stretti ci stanno quasi sempre), ma resta corretto
    // anche nel caso limite di un pannello strettissimo.
    juce::Label* const captions[] = { &minCaption, &maxCaption, &initCaption };
    juce::TextEditor* const editors[] = { &minEditor, &maxEditor, &defaultEditor };

    CsoundParameterMappingPanel::layoutWrappedFields (fieldsArea,
        { cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldNarrowWidth },
        cardFieldGap,
        [&] (int index, juce::Rectangle<int> cell)
        {
            CsoundParameterMappingPanel::layoutCaptionedField (cell, *captions[index], *editors[index]);
        });
}

void CsoundParameterMappingPanel::IntParamRow::mouseDown (const juce::MouseEvent& event)
{
    // Vedi il commento identico su ParamRow::mouseDown sopra.
    draggingFromHandle = handleBounds.contains (event.getPosition());
}

void CsoundParameterMappingPanel::IntParamRow::mouseMove (const juce::MouseEvent& event)
{
    updateHandleHover (event.getPosition());
}

void CsoundParameterMappingPanel::IntParamRow::mouseExit (const juce::MouseEvent&)
{
    if (handleHovered)
    {
        handleHovered = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CsoundParameterMappingPanel::IntParamRow::updateHandleHover (juce::Point<int> position)
{
    const bool nowHovered = handleBounds.contains (position)
                             && processor.getIntParamSlot (index).channelName.isNotEmpty();

    if (nowHovered != handleHovered)
    {
        handleHovered = nowHovered;
        setMouseCursor (handleHovered ? juce::MouseCursor::DraggingHandCursor
                                       : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CsoundParameterMappingPanel::IntParamRow::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingFromHandle)
        return;

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr || container->isDragAndDropActive())
        return;

    const auto slot = processor.getIntParamSlot (index);
    if (slot.channelName.isEmpty())
        return;

    container->startDragging ("csoundChannel:" + slot.channelName + "\x01" + makeIntConfigComment (slot), this);
}

//==============================================================================
// BoolParamRow ("TOGGLE" nel mockup, slot Bool).
CsoundParameterMappingPanel::BoolParamRow::BoolParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex)
    : processor (processorToEdit),
      index (slotIndex)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    setMouseClickGrabsKeyboardFocus (false);

    setupTypeLabel (typeLabel, "TOGGLE " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + " CHANNEL", kToggleAccent);
    addAndMakeVisible (typeLabel);

    channelNameEditor.setTextToShowWhenEmpty ("(no channel)", kPlaceholder);
    channelNameEditor.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    setupFieldCaption (initCaption, "INIT");
    addAndMakeVisible (initCaption);
    defaultToggle.setColour (juce::ToggleButton::textColourId, kText);
    defaultToggle.setColour (juce::ToggleButton::tickColourId, kToggleAccent);
    defaultToggle.setColour (juce::ToggleButton::tickDisabledColourId, kFieldOutline);
    defaultToggle.onClick = [this]
    {
        const auto beforeSlot = processor.getBoolParamSlot (index);
        commitFromFields();
        const auto afterSlot = processor.getBoolParamSlot (index);

        if (pushUndo)
        {
            auto& proc = processor;
            const int idx = index;
            pushUndo ([&proc, idx, afterSlot]  { proc.setBoolParamSlot (idx, afterSlot); },
                       [&proc, idx, beforeSlot] { proc.setBoolParamSlot (idx, beforeSlot); });
        }
    };
    addAndMakeVisible (defaultToggle);

    // Cerchio rosso con un "-" disegnato a mano (vedi drawButtonText,
    // branch "removeParam") invece del vecchio quadrato con la "x" - stesso
    // meccanismo della proprieta' dinamica "circular" gia' usata da "+".
    removeButton.setName ("removeParam");
    removeButton.getProperties().set ("circular", true);
    removeButton.setColour (juce::TextButton::buttonColourId, kDanger);
    removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    removeButton.setTooltip ("Remove this parameter");
    removeButton.onClick = [this]
    {
        const auto removedName = channelNameEditor.getText().trim();
        const auto beforeSlot = processor.getBoolParamSlot (index);
        channelNameEditor.setText ({}, false);
        commitFromFields();
        const auto afterSlot = processor.getBoolParamSlot (index);

        if (onCopiedToClipboard && removedName.isNotEmpty())
            onCopiedToClipboard ("--- Removed parameter: " + removedName + " ---");

        if (pushUndo)
        {
            auto& proc = processor;
            const int idx = index;
            pushUndo ([&proc, idx, afterSlot]  { proc.setBoolParamSlot (idx, afterSlot); },
                       [&proc, idx, beforeSlot] { proc.setBoolParamSlot (idx, beforeSlot); });
        }

        if (onRemoveRequested)
            onRemoveRequested();
    };
    addAndMakeVisible (removeButton);

    refreshFromProcessor();
}

void CsoundParameterMappingPanel::BoolParamRow::focusChannelNameField()
{
    channelNameEditor.grabKeyboardFocus();
    channelNameEditor.selectAll();
}

void CsoundParameterMappingPanel::BoolParamRow::refreshFromProcessor()
{
    const auto slot = processor.getBoolParamSlot (index);
    channelNameEditor.setText (slot.channelName, false);
    defaultToggle.setToggleState (slot.defaultValue, juce::dontSendNotification);
    removeButton.setVisible (slot.channelName.isNotEmpty());
}

void CsoundParameterMappingPanel::BoolParamRow::commitFromFields()
{
    CsoundAudioProcessor::BoolParamSlot slot;
    slot.channelName = channelNameEditor.getText().trim();
    slot.defaultValue = defaultToggle.getToggleState();

    processor.setBoolParamSlot (index, slot);
    removeButton.setVisible (slot.channelName.isNotEmpty());
    repaint();
}

void CsoundParameterMappingPanel::BoolParamRow::notifyRemovedIfEmpty()
{
    if (processor.getBoolParamSlot (index).channelName.isEmpty() && onRemoveRequested)
        onRemoveRequested();
}

void CsoundParameterMappingPanel::BoolParamRow::notifyCommittedIfNonEmpty()
{
    if (processor.getBoolParamSlot (index).channelName.isNotEmpty() && onCommittedNonEmpty)
        onCommittedNonEmpty();
}

void CsoundParameterMappingPanel::BoolParamRow::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    commitFromFields();
    pushPendingUndoIfAny();
    editor.giveAwayKeyboardFocus();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::BoolParamRow::textEditorEscapeKeyPressed (juce::TextEditor& editor)
{
    textEditorReturnKeyPressed (editor);
}

void CsoundParameterMappingPanel::BoolParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
    pushPendingUndoIfAny();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::BoolParamRow::textEditorTextChanged (juce::TextEditor&)
{
    if (! hasBeforeEditSlot)
    {
        beforeEditSlot = processor.getBoolParamSlot (index);
        hasBeforeEditSlot = true;
    }
    commitFromFields();
}

void CsoundParameterMappingPanel::BoolParamRow::pushPendingUndoIfAny()
{
    if (! hasBeforeEditSlot)
        return;

    const auto before = beforeEditSlot;
    const auto after = processor.getBoolParamSlot (index);
    hasBeforeEditSlot = false;

    if (pushUndo)
    {
        auto& proc = processor;
        const int idx = index;
        pushUndo ([&proc, idx, after]  { proc.setBoolParamSlot (idx, after); },
                   [&proc, idx, before] { proc.setBoolParamSlot (idx, before); });
    }
}

void CsoundParameterMappingPanel::BoolParamRow::paint (juce::Graphics& g)
{
    const bool hasName = processor.getBoolParamSlot (index).channelName.isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), kToggleAccent, handleBounds, hasName, handleHovered, true);
}

void CsoundParameterMappingPanel::BoolParamRow::resized()
{
    juce::Rectangle<int> handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea;
    CsoundParameterMappingPanel::layoutCardSkeleton (getLocalBounds(), handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea);

    handleBounds = handleStrip;
    typeLabel.setBounds (typeLabelArea);
    removeButton.setBounds (removeArea);
    channelNameEditor.setBounds (nameArea);

    CsoundParameterMappingPanel::layoutCaptionedField (fieldsArea.removeFromLeft (cardBoolFieldWidth), initCaption, defaultToggle);
}

void CsoundParameterMappingPanel::BoolParamRow::mouseDown (const juce::MouseEvent& event)
{
    // Vedi il commento identico su ParamRow::mouseDown sopra.
    draggingFromHandle = handleBounds.contains (event.getPosition());
}

void CsoundParameterMappingPanel::BoolParamRow::mouseMove (const juce::MouseEvent& event)
{
    updateHandleHover (event.getPosition());
}

void CsoundParameterMappingPanel::BoolParamRow::mouseExit (const juce::MouseEvent&)
{
    if (handleHovered)
    {
        handleHovered = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CsoundParameterMappingPanel::BoolParamRow::updateHandleHover (juce::Point<int> position)
{
    const bool nowHovered = handleBounds.contains (position)
                             && processor.getBoolParamSlot (index).channelName.isNotEmpty();

    if (nowHovered != handleHovered)
    {
        handleHovered = nowHovered;
        setMouseCursor (handleHovered ? juce::MouseCursor::DraggingHandCursor
                                       : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CsoundParameterMappingPanel::BoolParamRow::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingFromHandle)
        return;

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr || container->isDragAndDropActive())
        return;

    const auto slot = processor.getBoolParamSlot (index);
    if (slot.channelName.isEmpty())
        return;

    container->startDragging ("csoundChannel:" + slot.channelName + "\x01" + makeBoolConfigComment (slot), this);
}

//==============================================================================
// ChoiceParamRow ("MENU" nel mockup, slot Choice).
CsoundParameterMappingPanel::ChoiceParamRow::ChoiceParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex)
    : processor (processorToEdit),
      index (slotIndex)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    setMouseClickGrabsKeyboardFocus (false);

    setupTypeLabel (typeLabel, "MENU " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + " CHANNEL", kMenuAccent);
    addAndMakeVisible (typeLabel);

    channelNameEditor.setTextToShowWhenEmpty ("(no channel)", kPlaceholder);
    channelNameEditor.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    setupFieldCaption (optionsCaption, "OPTIONS (COMMA SEPARATED)");
    addAndMakeVisible (optionsCaption);
    optionsEditor.setTextToShowWhenEmpty ("item1, item2, item3...", kPlaceholder);
    optionsEditor.addListener (this);
    applyDarkFieldColours (optionsEditor);
    addAndMakeVisible (optionsEditor);

    setupFieldCaption (defaultCaption, "INIT");
    addAndMakeVisible (defaultCaption);
    defaultIndexCombo.setColour (juce::ComboBox::backgroundColourId, kFieldBg);
    defaultIndexCombo.setColour (juce::ComboBox::textColourId,       kText);
    defaultIndexCombo.setColour (juce::ComboBox::outlineColourId,    kFieldOutline);
    defaultIndexCombo.setColour (juce::PopupMenu::backgroundColourId,            kFieldBg);
    defaultIndexCombo.setColour (juce::PopupMenu::textColourId,                  kText);
    defaultIndexCombo.setColour (juce::PopupMenu::highlightedBackgroundColourId, kMenuAccent);
    defaultIndexCombo.setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    defaultIndexCombo.setTextWhenNoChoicesAvailable ("(no options yet)");
    defaultIndexCombo.setTextWhenNothingSelected ("(no options yet)");
    defaultIndexCombo.onChange = [this]
    {
        const auto beforeSlot = processor.getChoiceParamSlot (index);
        commitFromFields();
        const auto afterSlot = processor.getChoiceParamSlot (index);

        if (pushUndo)
        {
            auto& proc = processor;
            const int idx = index;
            pushUndo ([&proc, idx, afterSlot]  { proc.setChoiceParamSlot (idx, afterSlot); },
                       [&proc, idx, beforeSlot] { proc.setChoiceParamSlot (idx, beforeSlot); });
        }
    };
    addAndMakeVisible (defaultIndexCombo);

    // Cerchio rosso con un "-" disegnato a mano (vedi drawButtonText,
    // branch "removeParam") invece del vecchio quadrato con la "x" - stesso
    // meccanismo della proprieta' dinamica "circular" gia' usata da "+".
    removeButton.setName ("removeParam");
    removeButton.getProperties().set ("circular", true);
    removeButton.setColour (juce::TextButton::buttonColourId, kDanger);
    removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    removeButton.setTooltip ("Remove this parameter");
    removeButton.onClick = [this]
    {
        const auto removedName = channelNameEditor.getText().trim();
        const auto beforeSlot = processor.getChoiceParamSlot (index);
        channelNameEditor.setText ({}, false);
        commitFromFields();
        const auto afterSlot = processor.getChoiceParamSlot (index);

        if (onCopiedToClipboard && removedName.isNotEmpty())
            onCopiedToClipboard ("--- Removed parameter: " + removedName + " ---");

        if (pushUndo)
        {
            auto& proc = processor;
            const int idx = index;
            pushUndo ([&proc, idx, afterSlot]  { proc.setChoiceParamSlot (idx, afterSlot); },
                       [&proc, idx, beforeSlot] { proc.setChoiceParamSlot (idx, beforeSlot); });
        }

        if (onRemoveRequested)
            onRemoveRequested();
    };
    addAndMakeVisible (removeButton);

    refreshFromProcessor();
}

void CsoundParameterMappingPanel::ChoiceParamRow::focusChannelNameField()
{
    channelNameEditor.grabKeyboardFocus();
    channelNameEditor.selectAll();
}

void CsoundParameterMappingPanel::ChoiceParamRow::refreshFromProcessor()
{
    const auto slot = processor.getChoiceParamSlot (index);
    channelNameEditor.setText (slot.channelName, false);
    optionsEditor.setText (slot.optionLabels.joinIntoString (", "), false);
    refreshDefaultOptions();

    // jlimit(0, getNumItems()-1, ...) con ZERO opzioni (es. Menu appena
    // creato dal "+", OPZIONI ancora vuoto) chiama jlimit con upperLimit
    // pari a -1: jassert(lowerLimit <= upperLimit) fallisce (0 <= -1 e'
    // falso) e in debug manda in crash - da cui il crash segnalato aprendo
    // il menu Add. Il calcolo va quindi dentro il guard, non prima.
    if (defaultIndexCombo.getNumItems() > 0)
    {
        const auto idToSelect = juce::jlimit (0, defaultIndexCombo.getNumItems() - 1, slot.defaultIndex) + 1;
        defaultIndexCombo.setSelectedId (idToSelect, juce::dontSendNotification);
    }
    removeButton.setVisible (slot.channelName.isNotEmpty());
}

void CsoundParameterMappingPanel::ChoiceParamRow::refreshDefaultOptions()
{
    juce::StringArray parsed;
    parsed.addTokens (optionsEditor.getText(), ",", "");
    for (auto& option : parsed)
        option = option.trim();
    parsed.removeEmptyStrings();
    while (parsed.size() > CsoundAudioProcessor::maxChoiceOptions)
        parsed.remove (parsed.size() - 1);

    const auto previousId = defaultIndexCombo.getSelectedId();

    defaultIndexCombo.clear (juce::dontSendNotification);
    for (int i = 0; i < parsed.size(); ++i)
        defaultIndexCombo.addItem (parsed[i], i + 1);

    if (parsed.isEmpty())
        return;

    const auto restoredId = juce::jlimit (1, parsed.size(), previousId > 0 ? previousId : 1);
    defaultIndexCombo.setSelectedId (restoredId, juce::dontSendNotification);
}

void CsoundParameterMappingPanel::ChoiceParamRow::commitFromFields()
{
    CsoundAudioProcessor::ChoiceParamSlot slot;
    slot.channelName = channelNameEditor.getText().trim();

    refreshDefaultOptions();

    juce::StringArray parsed;
    parsed.addTokens (optionsEditor.getText(), ",", "");
    for (auto& option : parsed)
        option = option.trim();
    parsed.removeEmptyStrings();
    while (parsed.size() > CsoundAudioProcessor::maxChoiceOptions)
        parsed.remove (parsed.size() - 1);
    slot.optionLabels = parsed;

    const auto selectedId = defaultIndexCombo.getSelectedId();
    slot.defaultIndex = selectedId > 0 ? juce::jlimit (0, CsoundAudioProcessor::maxChoiceOptions - 1, selectedId - 1) : 0;

    processor.setChoiceParamSlot (index, slot);
    removeButton.setVisible (slot.channelName.isNotEmpty());
    repaint();
}

void CsoundParameterMappingPanel::ChoiceParamRow::notifyRemovedIfEmpty()
{
    if (processor.getChoiceParamSlot (index).channelName.isEmpty() && onRemoveRequested)
        onRemoveRequested();
}

void CsoundParameterMappingPanel::ChoiceParamRow::notifyCommittedIfNonEmpty()
{
    if (processor.getChoiceParamSlot (index).channelName.isNotEmpty() && onCommittedNonEmpty)
        onCommittedNonEmpty();
}

void CsoundParameterMappingPanel::ChoiceParamRow::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    commitFromFields();
    pushPendingUndoIfAny();
    editor.giveAwayKeyboardFocus();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::ChoiceParamRow::textEditorEscapeKeyPressed (juce::TextEditor& editor)
{
    textEditorReturnKeyPressed (editor);
}

void CsoundParameterMappingPanel::ChoiceParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
    pushPendingUndoIfAny();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::ChoiceParamRow::textEditorTextChanged (juce::TextEditor&)
{
    if (! hasBeforeEditSlot)
    {
        beforeEditSlot = processor.getChoiceParamSlot (index);
        hasBeforeEditSlot = true;
    }
    commitFromFields();
}

void CsoundParameterMappingPanel::ChoiceParamRow::pushPendingUndoIfAny()
{
    if (! hasBeforeEditSlot)
        return;

    const auto before = beforeEditSlot;
    const auto after = processor.getChoiceParamSlot (index);
    hasBeforeEditSlot = false;

    if (pushUndo)
    {
        auto& proc = processor;
        const int idx = index;
        pushUndo ([&proc, idx, after]  { proc.setChoiceParamSlot (idx, after); },
                   [&proc, idx, before] { proc.setChoiceParamSlot (idx, before); });
    }
}

void CsoundParameterMappingPanel::ChoiceParamRow::paint (juce::Graphics& g)
{
    const bool hasName = processor.getChoiceParamSlot (index).channelName.isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), kMenuAccent, handleBounds, hasName, handleHovered, true);
}

void CsoundParameterMappingPanel::ChoiceParamRow::resized()
{
    juce::Rectangle<int> handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea;
    CsoundParameterMappingPanel::layoutCardSkeleton (getLocalBounds(), handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea);

    handleBounds = handleStrip;
    typeLabel.setBounds (typeLabelArea);
    removeButton.setBounds (removeArea);
    channelNameEditor.setBounds (nameArea);

    // Row 1: OPTIONS, full card width.
    auto row1 = fieldsArea.removeFromTop (cardFieldCaptionHeight + cardFieldCaptionGap + cardFieldBoxHeight);
    CsoundParameterMappingPanel::layoutCaptionedField (row1, optionsCaption, optionsEditor);

    // Row 2: DEFAULT (narrow, left-aligned combo showing the chosen label).
    fieldsArea.removeFromTop (cardFieldRowGap);
    CsoundParameterMappingPanel::layoutCaptionedField (fieldsArea.removeFromLeft (cardChoiceDefaultWidth), defaultCaption, defaultIndexCombo);
}

void CsoundParameterMappingPanel::ChoiceParamRow::mouseDown (const juce::MouseEvent& event)
{
    // Vedi il commento identico su ParamRow::mouseDown sopra.
    draggingFromHandle = handleBounds.contains (event.getPosition());
}

void CsoundParameterMappingPanel::ChoiceParamRow::mouseMove (const juce::MouseEvent& event)
{
    updateHandleHover (event.getPosition());
}

void CsoundParameterMappingPanel::ChoiceParamRow::mouseExit (const juce::MouseEvent&)
{
    if (handleHovered)
    {
        handleHovered = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CsoundParameterMappingPanel::ChoiceParamRow::updateHandleHover (juce::Point<int> position)
{
    const bool nowHovered = handleBounds.contains (position)
                             && processor.getChoiceParamSlot (index).channelName.isNotEmpty();

    if (nowHovered != handleHovered)
    {
        handleHovered = nowHovered;
        setMouseCursor (handleHovered ? juce::MouseCursor::DraggingHandCursor
                                       : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CsoundParameterMappingPanel::ChoiceParamRow::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingFromHandle)
        return;

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr || container->isDragAndDropActive())
        return;

    const auto slot = processor.getChoiceParamSlot (index);
    if (slot.channelName.isEmpty())
        return;

    container->startDragging ("csoundChannel:" + slot.channelName + "\x01" + makeChoiceConfigComment (slot), this);
}

//==============================================================================
// GenericParamRow - vedi il commento in testa alla dichiarazione in
// CsoundParameterEditor.h.
CsoundParameterMappingPanel::GenericParamRow::GenericParamRow (
    const juce::String& channelName, juce::RangedAudioParameter& parameter,
    Kind rowKind, const juce::StringArray& choiceLabels,
    juce::Colour accent, const juce::String& typeLabelText,
    std::function<juce::String()> getChannelNameFn,
    std::function<juce::String()> getConfigCommentFn,
    bool treatAsInteger,
    std::function<double (double)> sliderToRealFn,
    std::function<double (double)> realToSliderFn)
    : kind (rowKind), accentColour (accent), isIntegerLike (treatAsInteger),
      sliderToReal (std::move (sliderToRealFn)),
      realToSlider (std::move (realToSliderFn)),
      getChannelName (std::move (getChannelNameFn)),
      getConfigComment (std::move (getConfigCommentFn))
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    setMouseClickGrabsKeyboardFocus (false);

    setupTypeLabel (typeLabel, typeLabelText, accentColour);
    addAndMakeVisible (typeLabel);

    channelNameLabel.setText (channelName, juce::dontSendNotification);
    channelNameLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    channelNameLabel.setColour (juce::Label::textColourId, kText);
    channelNameLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (channelNameLabel);

    switch (kind)
    {
        case Kind::slider:
        {
            slider.setScrollWheelEnabled (false);
            slider.setColour (juce::Slider::trackColourId,      accentColour);
            slider.setColour (juce::Slider::thumbColourId,      accentColour);
            slider.setColour (juce::Slider::backgroundColourId, kFieldBg);
            addAndMakeVisible (slider);

            // nullptr al posto di un juce::UndoManager: i VALORI dei
            // parametri sono gestiti dalla DAW/host (automazione, stato di
            // sessione), quindi non devono finire nella cronologia Undo/
            // Redo del plugin - vedi il commento sul costruttore nel .h.
            sliderAttachment = std::make_unique<juce::SliderParameterAttachment> (parameter, slider, nullptr);
            slider.sendLookAndFeelChange();

            auto setupRangeLabel = [] (juce::Label& l)
            {
                l.setFont (juce::Font (juce::FontOptions (9.0f)));
                l.setColour (juce::Label::textColourId, kTextMuted);
                l.setMinimumHorizontalScale (1.0f);
            };
            setupRangeLabel (minLabel);
            setupRangeLabel (maxLabel);
            minLabel.setJustificationType (juce::Justification::centredLeft);
            maxLabel.setJustificationType (juce::Justification::centredRight);
            // Valori iniziali impostati sotto da refreshRangeDisplay() (fine
            // di questo blocco), non qui direttamente con slider.getMinimum/
            // Maximum() - vedi il commento sul costruttore nel .h sul perche'.
            addAndMakeVisible (minLabel);
            addAndMakeVisible (maxLabel);

            // Editabile da tastiera (richiesta esplicita) - solo cifre, '-'
            // e, per i Float, anche '.'; commit SOLO su Return/focus perso
            // (vedi commitValueFromField()), MAI ad ogni carattere. Niente
            // didascalia "VALUE" sopra (riga compatta, economia verticale) -
            // il contesto (etichetta tipo + slider accanto) e' gia' chiaro.
            valueReadout.setInputRestrictions (0, isIntegerLike ? "-0123456789" : "-0123456789.");
            valueReadout.setJustification (juce::Justification::centred);
            valueReadout.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
            applyDarkFieldColours (valueReadout);
            valueReadout.onReturnKey = [this]
            {
                commitValueFromField();
                valueReadout.giveAwayKeyboardFocus();
            };
            valueReadout.onFocusLost = [this] { commitValueFromField(); };
            // Esc deve comportarsi come Return (richiesto esplicitamente,
            // vedi lo stesso trattamento su ParamRow/IntParamRow/ecc.) -
            // juce::TextEditor consuma Esc per conto suo (vedi
            // TextEditor::keyPressed) e senza questo hook non notificherebbe
            // nulla.
            valueReadout.onEscapeKey = [this]
            {
                commitValueFromField();
                valueReadout.giveAwayKeyboardFocus();
            };
            addAndMakeVisible (valueReadout);

            slider.onValueChange = [this] { updateValueReadout(); };
            refreshRangeDisplay();
            break;
        }

        case Kind::toggle:
            toggle.setName ("pillToggle");
            addAndMakeVisible (toggle);
            buttonAttachment = std::make_unique<juce::ButtonParameterAttachment> (parameter, toggle, nullptr);
            break;

        case Kind::choice:
            for (int i = 0; i < choiceLabels.size(); ++i)
                comboBox.addItem (choiceLabels[i], i + 1);
            comboBox.setColour (juce::ComboBox::backgroundColourId, kFieldBg);
            comboBox.setColour (juce::ComboBox::textColourId,       kText);
            comboBox.setColour (juce::ComboBox::outlineColourId,    kFieldOutline);
            comboBox.setColour (juce::PopupMenu::backgroundColourId,            kFieldBg);
            comboBox.setColour (juce::PopupMenu::textColourId,                  kText);
            comboBox.setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour);
            comboBox.setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
            addAndMakeVisible (comboBox);
            comboAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (parameter, comboBox, nullptr);
            break;
    }
}

void CsoundParameterMappingPanel::GenericParamRow::updateValueReadout()
{
    // Mai mentre l'utente ci sta scrivendo dentro - altrimenti gli
    // sovrascriverebbe il testo (e il cursore) a meta' digitazione, ad
    // esempio ogni volta che l'host automatizza il parametro.
    if (valueReadout.hasKeyboardFocus (true))
        return;

    // slider.getValue() e' nel range NATIVO del parametro apvts (0..1 per i
    // Float, 0..intHostRangeMax per gli Int) - sliderToReal lo rimappa nel
    // range REALE configurato dall'utente (slot.minValue..maxValue), che e'
    // quello che il box VALUE deve mostrare (BUG corretto: prima mostrava
    // il valore nativo grezzo). Vedi il commento sul costruttore nel .h.
    const auto raw = slider.getValue();
    const auto value = sliderToReal ? sliderToReal (raw) : raw;
    valueReadout.setText (isIntegerLike ? juce::String ((int) std::round (value))
                                         : juce::String (value, 3),
                          false);
}

void CsoundParameterMappingPanel::GenericParamRow::commitValueFromField()
{
    const auto text = valueReadout.getText().trim();
    if (text.isNotEmpty())
    {
        const auto typedReal = text.getDoubleValue();
        const auto rawValue = realToSlider ? realToSlider (typedReal) : typedReal;
        slider.setValue (juce::jlimit (slider.getMinimum(), slider.getMaximum(), rawValue), juce::sendNotificationSync);
    }

    // Rilegge comunque il valore (clampato/riformattato, o invariato se il
    // testo non era un numero valido) - cosi' il campo non resta mai con un
    // testo "sporco" dopo un Return/focus perso.
    updateValueReadout();
}

void CsoundParameterMappingPanel::GenericParamRow::refreshRangeDisplay()
{
    if (kind != Kind::slider)
        return;

    // min/max mostrati sono SEMPRE il range REALE (slot.minValue/maxValue),
    // non il range nativo grezzo di slider.getMinimum()/getMaximum() - vedi
    // il commento sul costruttore nel .h. Rilette ad ogni chiamata (non
    // cacheate): chiamata da *UnifiedRow::setEditMode() ogni volta che si
    // torna da Edit alla vista UI, cosi' un Min/Max appena modificato si
    // vede SUBITO, invece di restare fermo al valore di quando questa riga
    // e' stata costruita (era il BUG segnalato: "dopo la modifica premendo
    // sul bottone occhio... la UI non si aggiorna").
    if (sliderToReal)
    {
        minLabel.setText (juce::String ((int) std::round (sliderToReal (slider.getMinimum()))), juce::dontSendNotification);
        maxLabel.setText (juce::String ((int) std::round (sliderToReal (slider.getMaximum()))), juce::dontSendNotification);
    }

    updateValueReadout();
}

void CsoundParameterMappingPanel::GenericParamRow::paint (juce::Graphics& g)
{
    // Stessa maniglia (con puntini di trascinamento) delle card Edit -
    // trascinabile anche qui, vedi mouseDown/mouseDrag sotto.
    const bool hasName = getChannelName && getChannelName().isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), accentColour, handleBounds, hasName, handleHovered, false);
}

void CsoundParameterMappingPanel::GenericParamRow::mouseDown (const juce::MouseEvent& event)
{
    // Vedi il commento identico su ParamRow::mouseDown piu' sopra.
    draggingFromHandle = handleBounds.contains (event.getPosition());
}

void CsoundParameterMappingPanel::GenericParamRow::mouseMove (const juce::MouseEvent& event)
{
    updateHandleHover (event.getPosition());
}

void CsoundParameterMappingPanel::GenericParamRow::mouseExit (const juce::MouseEvent&)
{
    if (handleHovered)
    {
        handleHovered = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CsoundParameterMappingPanel::GenericParamRow::updateHandleHover (juce::Point<int> position)
{
    const bool nowHovered = handleBounds.contains (position)
                             && getChannelName && getChannelName().isNotEmpty();

    if (nowHovered != handleHovered)
    {
        handleHovered = nowHovered;
        setMouseCursor (handleHovered ? juce::MouseCursor::DraggingHandCursor
                                       : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CsoundParameterMappingPanel::GenericParamRow::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingFromHandle)
        return;

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
    if (container == nullptr || container->isDragAndDropActive())
        return;

    const auto name = getChannelName ? getChannelName() : juce::String();
    if (name.isEmpty())
        return;

    const auto configComment = getConfigComment ? getConfigComment() : juce::String();
    container->startDragging ("csoundChannel:" + name + "\x01" + configComment, this);
}

void CsoundParameterMappingPanel::GenericParamRow::resized()
{
    juce::Rectangle<int> handleStrip, typeLabelArea, nameArea, fieldsArea;
    CsoundParameterMappingPanel::layoutUiCardSkeleton (getLocalBounds(), handleStrip, typeLabelArea, nameArea, fieldsArea);

    handleBounds = handleStrip;
    typeLabel.setBounds (typeLabelArea);
    channelNameLabel.setBounds (nameArea);

    // fieldsArea e' un'UNICA riga compatta (uiCardControlHeight, niente
    // didascalia sopra) - usata per intero da ciascun tipo di controllo.
    switch (kind)
    {
        case Kind::slider:
        {
            // Box VALUE a destra, larghezza fissa, ALTA QUANTO l'intera riga
            // (niente didascalia sopra) - tutto il resto (a sinistra) va
            // allo slider, per sfruttare il massimo spazio orizzontale
            // possibile.
            auto valueArea = fieldsArea.removeFromRight (uiValueBoxWidth);
            fieldsArea.removeFromRight (uiValueGap);
            valueReadout.setBounds (valueArea);

            // Sotto lo slider, le etichette min/max - lo slider stesso resta
            // COMPATTO (non e' l'altezza totale che deve crescere): la
            // maniglia e' disegnata molto piu' grande del normale
            // indipendentemente da quanto e' sottile questo rettangolo -
            // vedi CsoundParameterPanelLookAndFeel::drawLinearSlider nel .cpp.
            auto rangeLabelsArea = fieldsArea.removeFromBottom (uiRangeLabelHeight);
            minLabel.setBounds (rangeLabelsArea.removeFromLeft (rangeLabelsArea.getWidth() / 2));
            maxLabel.setBounds (rangeLabelsArea);

            fieldsArea.removeFromBottom (uiSliderTrackGap);
            slider.setBounds (fieldsArea);
            break;
        }

        case Kind::toggle:
            toggle.setBounds (fieldsArea.removeFromLeft (uiTogglePillWidth));
            break;

        case Kind::choice:
            comboBox.setBounds (fieldsArea);
            break;
    }
}

//==============================================================================
// FloatUnifiedRow/IntUnifiedRow/BoolUnifiedRow/ChoiceUnifiedRow - vedi il
// commento in testa alla dichiarazione in CsoundParameterEditor.h: editRow
// (metadata) e uiRow (controllo vero) sovrapposti sulle stesse bounds, uno
// dei due nascosto in base a setEditMode().
CsoundParameterMappingPanel::FloatUnifiedRow::FloatUnifiedRow (
    CsoundAudioProcessor& processorToEdit, int slotIndex,
    std::function<void (const juce::String&)> onCopied, std::function<void()> onRemoved,
    std::function<void()> onEditModeChangedIn)
    : onEditModeChanged (std::move (onEditModeChangedIn))
{
    editRow = std::make_unique<ParamRow> (processorToEdit, slotIndex);
    editRow->onCopiedToClipboard = onCopied;
    editRow->onRemoveRequested = onRemoved;
    addChildComponent (*editRow);

    const auto slot = processorToEdit.getChannelParamSlot (slotIndex);
    if (auto* param = processorToEdit.apvts.getParameter (CsoundAudioProcessor::getChannelParamID (slotIndex)))
    {
        uiRow = std::make_unique<GenericParamRow> (slot.channelName, *param, GenericParamRow::Kind::slider,
                                                     juce::StringArray(), kSliderAccent, "SLIDER FLOAT",
                                                     [&processorToEdit, slotIndex] { return processorToEdit.getChannelParamSlot (slotIndex).channelName; },
                                                     [&processorToEdit, slotIndex] { return makeFloatConfigComment (processorToEdit.getChannelParamSlot (slotIndex)); },
                                                     false,
                                                     // sliderToReal/realToSlider: rilegge lo SLOT al volo (non lo
                                                     // "slot" catturato qui sopra, che e' solo lo snapshot alla
                                                     // costruzione) - cosi' restano valide anche se l'utente
                                                     // cambia Min/Max/skew/increment in modalita' Edit DOPO che
                                                     // questa riga e' stata creata (BUG corretto, vedi il
                                                     // commento sul costruttore di GenericParamRow nel .h).
                                                     [&processorToEdit, slotIndex] (double normalized)
                                                     {
                                                         return processorToEdit.denormalizeChannelParam (
                                                             processorToEdit.getChannelParamSlot (slotIndex), (float) normalized);
                                                     },
                                                     [&processorToEdit, slotIndex] (double real)
                                                     {
                                                         return (double) processorToEdit.normalizeChannelParam (
                                                             processorToEdit.getChannelParamSlot (slotIndex), real);
                                                     });
        uiRow->onCopiedToClipboard = onCopied;
        addChildComponent (*uiRow);
    }

    setupRowEditIconButton (editIconButton);
    editIconButton.onClick = [this] { setEditMode (! rowEditMode); if (onEditModeChanged) onEditModeChanged(); };
    addAndMakeVisible (editIconButton); // dopo editRow/uiRow: deve restare sempre in primo piano

    setupRowCopyButton (copyButton);
    copyButton.onClick = [&processorToEdit, slotIndex, onCopied]
    {
        const auto slot = processorToEdit.getChannelParamSlot (slotIndex);
        copyChngetToClipboard (slot.channelName, makeFloatConfigComment (slot), onCopied);
    };
    addAndMakeVisible (copyButton);

    setEditMode (false);
}

void CsoundParameterMappingPanel::FloatUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (getLocalBounds());
    layoutRowIconButtons (getLocalBounds(), rowEditMode, copyButton, editIconButton);
}

void CsoundParameterMappingPanel::FloatUnifiedRow::setEditMode (bool edit)
{
    rowEditMode = edit;
    editRow->setVisible (edit);
    if (uiRow != nullptr)
        uiRow->setVisible (! edit);
    // setToggleState (non solo un cambio di colore): guida sia il cerchio
    // pieno colorato di drawButtonBackground sia la scelta tra le due
    // icone diverse in drawButtonText - vedi i commenti li'.
    editIconButton.setToggleState (edit, juce::dontSendNotification);
    editIconButton.setTooltip (edit ? "Back to controls" : "Edit this parameter");
    // Se si torna alla vista UI, rilegge SUBITO il range reale appena
    // configurato in modalita' Edit (BUG corretto: prima restava fermo al
    // valore della costruzione) - vedi GenericParamRow::refreshRangeDisplay().
    if (! edit && uiRow != nullptr)
        uiRow->refreshRangeDisplay();
    if (onEditModeToggled)
        onEditModeToggled (edit);
    resized();
}

CsoundParameterMappingPanel::IntUnifiedRow::IntUnifiedRow (
    CsoundAudioProcessor& processorToEdit, int slotIndex,
    std::function<void (const juce::String&)> onCopied, std::function<void()> onRemoved,
    std::function<void()> onEditModeChangedIn)
    : onEditModeChanged (std::move (onEditModeChangedIn))
{
    editRow = std::make_unique<IntParamRow> (processorToEdit, slotIndex);
    editRow->onCopiedToClipboard = onCopied;
    editRow->onRemoveRequested = onRemoved;
    addChildComponent (*editRow);

    const auto slot = processorToEdit.getIntParamSlot (slotIndex);
    if (auto* param = processorToEdit.apvts.getParameter (CsoundAudioProcessor::getIntParamID (slotIndex)))
    {
        uiRow = std::make_unique<GenericParamRow> (slot.channelName, *param, GenericParamRow::Kind::slider,
                                                     juce::StringArray(), kKnobAccent, "SLIDER INT",
                                                     [&processorToEdit, slotIndex] { return processorToEdit.getIntParamSlot (slotIndex).channelName; },
                                                     [&processorToEdit, slotIndex] { return makeIntConfigComment (processorToEdit.getIntParamSlot (slotIndex)); },
                                                     true,
                                                     // Il parametro apvts nativo va 0..intHostRangeMax (NON 0..1
                                                     // come i Float) - vedi il commento sul costruttore di
                                                     // GenericParamRow nel .h e su IntHostParameter in
                                                     // PluginProcessor.h. Stesso BUG/fix del caso Float sopra.
                                                     [&processorToEdit, slotIndex] (double rawHostValue)
                                                     {
                                                         const float normalized = (float) (rawHostValue / (double) CsoundAudioProcessor::intHostRangeMax);
                                                         return (double) processorToEdit.denormalizeIntParam (
                                                             processorToEdit.getIntParamSlot (slotIndex), normalized);
                                                     },
                                                     [&processorToEdit, slotIndex] (double real)
                                                     {
                                                         const float normalized = processorToEdit.normalizeIntParam (
                                                             processorToEdit.getIntParamSlot (slotIndex), real);
                                                         return (double) normalized * (double) CsoundAudioProcessor::intHostRangeMax;
                                                     });
        uiRow->onCopiedToClipboard = onCopied;
        addChildComponent (*uiRow);
    }

    setupRowEditIconButton (editIconButton);
    editIconButton.onClick = [this] { setEditMode (! rowEditMode); if (onEditModeChanged) onEditModeChanged(); };
    addAndMakeVisible (editIconButton);

    setupRowCopyButton (copyButton);
    copyButton.onClick = [&processorToEdit, slotIndex, onCopied]
    {
        const auto slot = processorToEdit.getIntParamSlot (slotIndex);
        copyChngetToClipboard (slot.channelName, makeIntConfigComment (slot), onCopied);
    };
    addAndMakeVisible (copyButton);

    setEditMode (false);
}

void CsoundParameterMappingPanel::IntUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (getLocalBounds());
    layoutRowIconButtons (getLocalBounds(), rowEditMode, copyButton, editIconButton);
}

void CsoundParameterMappingPanel::IntUnifiedRow::setEditMode (bool edit)
{
    rowEditMode = edit;
    editRow->setVisible (edit);
    if (uiRow != nullptr)
        uiRow->setVisible (! edit);
    // setToggleState (non solo un cambio di colore): guida sia il cerchio
    // pieno colorato di drawButtonBackground sia la scelta tra le due
    // icone diverse in drawButtonText - vedi i commenti li'.
    editIconButton.setToggleState (edit, juce::dontSendNotification);
    editIconButton.setTooltip (edit ? "Back to controls" : "Edit this parameter");
    // Vedi il commento identico su FloatUnifiedRow::setEditMode sopra.
    if (! edit && uiRow != nullptr)
        uiRow->refreshRangeDisplay();
    if (onEditModeToggled)
        onEditModeToggled (edit);
    resized();
}

CsoundParameterMappingPanel::BoolUnifiedRow::BoolUnifiedRow (
    CsoundAudioProcessor& processorToEdit, int slotIndex,
    std::function<void (const juce::String&)> onCopied, std::function<void()> onRemoved,
    std::function<void()> onEditModeChangedIn)
    : onEditModeChanged (std::move (onEditModeChangedIn))
{
    editRow = std::make_unique<BoolParamRow> (processorToEdit, slotIndex);
    editRow->onCopiedToClipboard = onCopied;
    editRow->onRemoveRequested = onRemoved;
    addChildComponent (*editRow);

    const auto slot = processorToEdit.getBoolParamSlot (slotIndex);
    if (auto* param = processorToEdit.apvts.getParameter (CsoundAudioProcessor::getBoolParamID (slotIndex)))
    {
        uiRow = std::make_unique<GenericParamRow> (slot.channelName, *param, GenericParamRow::Kind::toggle,
                                                     juce::StringArray(), kToggleAccent, "TOGGLE",
                                                     [&processorToEdit, slotIndex] { return processorToEdit.getBoolParamSlot (slotIndex).channelName; },
                                                     [&processorToEdit, slotIndex] { return makeBoolConfigComment (processorToEdit.getBoolParamSlot (slotIndex)); });
        uiRow->onCopiedToClipboard = onCopied;
        addChildComponent (*uiRow);
    }

    setupRowEditIconButton (editIconButton);
    editIconButton.onClick = [this] { setEditMode (! rowEditMode); if (onEditModeChanged) onEditModeChanged(); };
    addAndMakeVisible (editIconButton);

    setupRowCopyButton (copyButton);
    copyButton.onClick = [&processorToEdit, slotIndex, onCopied]
    {
        const auto slot = processorToEdit.getBoolParamSlot (slotIndex);
        copyChngetToClipboard (slot.channelName, makeBoolConfigComment (slot), onCopied);
    };
    addAndMakeVisible (copyButton);

    setEditMode (false);
}

void CsoundParameterMappingPanel::BoolUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (getLocalBounds());
    layoutRowIconButtons (getLocalBounds(), rowEditMode, copyButton, editIconButton);
}

void CsoundParameterMappingPanel::BoolUnifiedRow::setEditMode (bool edit)
{
    rowEditMode = edit;
    editRow->setVisible (edit);
    if (uiRow != nullptr)
        uiRow->setVisible (! edit);
    // setToggleState (non solo un cambio di colore): guida sia il cerchio
    // pieno colorato di drawButtonBackground sia la scelta tra le due
    // icone diverse in drawButtonText - vedi i commenti li'.
    editIconButton.setToggleState (edit, juce::dontSendNotification);
    editIconButton.setTooltip (edit ? "Back to controls" : "Edit this parameter");
    if (onEditModeToggled)
        onEditModeToggled (edit);
    resized();
}

CsoundParameterMappingPanel::ChoiceUnifiedRow::ChoiceUnifiedRow (
    CsoundAudioProcessor& processorToEdit, int slotIndex,
    std::function<void (const juce::String&)> onCopied, std::function<void()> onRemoved,
    std::function<void()> onEditModeChangedIn)
    : onEditModeChanged (std::move (onEditModeChangedIn))
{
    editRow = std::make_unique<ChoiceParamRow> (processorToEdit, slotIndex);
    editRow->onCopiedToClipboard = onCopied;
    editRow->onRemoveRequested = onRemoved;
    addChildComponent (*editRow);

    const auto slot = processorToEdit.getChoiceParamSlot (slotIndex);
    if (auto* param = processorToEdit.apvts.getParameter (CsoundAudioProcessor::getChoiceParamID (slotIndex)))
    {
        juce::StringArray labels;
        for (int opt = 0; opt < CsoundAudioProcessor::maxChoiceOptions; ++opt)
            labels.add (CsoundAudioProcessor::getChoiceOptionLabel (slot, opt));

        uiRow = std::make_unique<GenericParamRow> (slot.channelName, *param, GenericParamRow::Kind::choice,
                                                     labels, kMenuAccent, "MENU",
                                                     [&processorToEdit, slotIndex] { return processorToEdit.getChoiceParamSlot (slotIndex).channelName; },
                                                     [&processorToEdit, slotIndex] { return makeChoiceConfigComment (processorToEdit.getChoiceParamSlot (slotIndex)); });
        uiRow->onCopiedToClipboard = onCopied;
        addChildComponent (*uiRow);
    }

    setupRowEditIconButton (editIconButton);
    editIconButton.onClick = [this] { setEditMode (! rowEditMode); if (onEditModeChanged) onEditModeChanged(); };
    addAndMakeVisible (editIconButton);

    setupRowCopyButton (copyButton);
    copyButton.onClick = [&processorToEdit, slotIndex, onCopied]
    {
        const auto slot = processorToEdit.getChoiceParamSlot (slotIndex);
        copyChngetToClipboard (slot.channelName, makeChoiceConfigComment (slot), onCopied);
    };
    addAndMakeVisible (copyButton);

    setEditMode (false);
}

void CsoundParameterMappingPanel::ChoiceUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (getLocalBounds());
    layoutRowIconButtons (getLocalBounds(), rowEditMode, copyButton, editIconButton);
}

void CsoundParameterMappingPanel::ChoiceUnifiedRow::setEditMode (bool edit)
{
    rowEditMode = edit;
    editRow->setVisible (edit);
    if (uiRow != nullptr)
        uiRow->setVisible (! edit);
    // setToggleState (non solo un cambio di colore): guida sia il cerchio
    // pieno colorato di drawButtonBackground sia la scelta tra le due
    // icone diverse in drawButtonText - vedi i commenti li'.
    editIconButton.setToggleState (edit, juce::dontSendNotification);
    editIconButton.setTooltip (edit ? "Back to controls" : "Edit this parameter");
    if (onEditModeToggled)
        onEditModeToggled (edit);
    resized();
}

//==============================================================================
CsoundParameterMappingPanel::CsoundParameterMappingPanel (CsoundAudioProcessor& processorToEdit,
                                                            juce::UndoManager& sharedUndoManager)
    : processor (processorToEdit), undoManager (sharedUndoManager)
{
    setLookAndFeel (&lookAndFeel);

    // Bottone multifunzione: circolare e accentato (proprieta' dinamica
    // "circular", vedi CsoundParameterPanelLookAndFeel per lo sfondo),
    // icona dedicata disegnata a mano (nome "paramsMenu", NON piu' la "+"
    // generica - richiesto esplicitamente: "cambia icona al bottone +
    // dal momento che ormai e' multifunzionale") - apre Add Slider Float/
    // Int/Toggle/Menu, Open/Close Config, Remove/Reset Parameters (vedi
    // showAddMenu(), ora espanso con tutto il contenuto ex-submenu
    // "Parameters" del burger). Vive DENTRO questo pannello (title bar, a
    // sinistra - vedi resized()), quindi addAndMakeVisible() qui, a
    // differenza di menuButton sotto che resta riparentato da PluginEditor.
    addButton.setName ("paramsMenu");
    addButton.getProperties().set ("circular", true);
    addButton.setColour (juce::TextButton::buttonColourId, kAccent);
    addButton.setTooltip ("Add / configure parameters");
    addButton.onClick = [this] { showAddMenu(); };
    addAndMakeVisible (addButton);

    // Titolo centrale della title bar (richiesta esplicita).
    titleLabel.setText ("Parameters", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, kText);
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    // Bottone menu ("hamburger"): stesso trattamento circolare accentato
    // di addButton, icona a tre barre disegnata da drawButtonText (nome
    // "burgerMenu") - apre Undo/Redo/Save/Load/Show Parameters/Console.
    // A differenza di addButton sopra, questo resta riparentato nella
    // toolbar PRINCIPALE di PluginEditor (vedi getMenuButton() in
    // CsoundParameterEditor.h) - stile/nome/onClick restano impostati qui
    // perche' la LOGICA resta di questo pannello, solo il GENITORE nella
    // gerarchia dei componenti cambia. NON addAndMakeVisible() qui.
    menuButton.setName ("burgerMenu");
    menuButton.getProperties().set ("circular", true);
    menuButton.setColour (juce::TextButton::buttonColourId, kAccent);
    menuButton.setTooltip ("Undo / Redo / Save / Load / Show Parameters / Show Console");
    menuButton.onClick = [this] { showPanelMenu(); };

    viewport.setViewedComponent (&rowsContainer, false);
    // Solo verticale: il contenuto ora va sempre a capo per restare entro
    // la larghezza disponibile (vedi computeWrappedFieldRows()), quindi non
    // serve piu' scorrimento orizzontale.
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    emptyStateLabel.setText (
        "Add a Slider Float, Slider Int, Toggle, or Menu parameter.",
        juce::dontSendNotification);
    emptyStateLabel.setJustificationType (juce::Justification::centred);
    emptyStateLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    emptyStateLabel.setColour (juce::Label::textColourId, kTextMuted);
    emptyStateLabel.setMinimumHorizontalScale (1.0f);
    addChildComponent (emptyStateLabel);

    rebuildUnifiedRows();
}

CsoundParameterMappingPanel::~CsoundParameterMappingPanel()
{
    setLookAndFeel (nullptr);
}

bool CsoundParameterMappingPanel::undo()
{
    return undoManager.undo();
}

bool CsoundParameterMappingPanel::redo()
{
    return undoManager.redo();
}

void CsoundParameterMappingPanel::showPanelMenu()
{
    // Non e' piu' un juce::PopupMenu (richiesta esplicita: popup "stile
    // Numa Player", custom, pensati per il tocco - vedi CsoundActionSheet.h).
    // Il submenu "Parameters" (Add/Open-Close Config/Remove/Reset) e' stato
    // RIMOSSO da qui (richiesta esplicita: "rimuovilo quindi dal menu
    // principale") - si raggiunge ora direttamente dal bottone "+" nella
    // title bar del pannello Parametri, vedi showAddMenu(). Gli ID di primo
    // livello restano 101+ (non piu' indispensabile visto che lo spazio
    // 1..4 di buildAddParameterItems() non e' piu' condiviso con questo
    // foglio, ma invariato per non rinumerare tutto).
    std::vector<CsoundActionSheetItem> items;

    // Sezione "View", in testa al menu (richiesta esplicita) - Show
    // Parameters/Show Console spuntate quando il rispettivo elemento e'
    // visibile (isParametersPanelVisible/isConsoleVisible, impostate da
    // PluginEditor). I vecchi bottoni dedicati nella toolbar principale
    // sono stati rimossi: questa e' ora l'UNICA via per queste due azioni.
    const bool parametersVisible = isParametersPanelVisible && isParametersPanelVisible();
    const bool consoleVisible    = isConsoleVisible && isConsoleVisible();

    {
        CsoundActionSheetItem item;
        item.id = 101; item.text = "Show Parameters";
        item.enabled = onToggleParametersRequested != nullptr; item.ticked = parametersVisible;
        item.icon = CsoundActionSheetIcon::sidebarPanel;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 102; item.text = "Show Console";
        item.enabled = onToggleConsoleRequested != nullptr; item.ticked = consoleVisible;
        item.icon = CsoundActionSheetIcon::console;
        items.push_back (item);
    }

    // Load/Save CSD: richiamano le stesse funzioni dei vecchi bottoni
    // "Load..."/"Save as..." nella toolbar PRINCIPALE di PluginEditor, ora
    // rimossi (richiesta esplicita) - PluginEditor imposta queste due
    // callback nel proprio costruttore (vedi onSaveSessionRequested/
    // onLoadSessionRequested in CsoundParameterEditor.h). Separatore PRIMA
    // di "Load..." (richiesta esplicita, BUG corretto: prima era dopo),
    // a separare la sezione "View" sopra da quella file qui sotto.
    items.push_back (CsoundActionSheetItem::separator());
    
    // "Save": sovrascrive il file collegato (vedi onSaveLinkedRequested nel
    // .h) - prima di "Save as...", che invece chiede sempre il path.
    {
        CsoundActionSheetItem item;
        item.id = 112; item.text = "Save"; item.enabled = onSaveLinkedRequested != nullptr;
        item.icon = CsoundActionSheetIcon::save;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 105; item.text = "Save as..."; item.enabled = onSaveSessionRequested != nullptr;
        item.icon = CsoundActionSheetIcon::save;
        items.push_back (item);
    }

    {
        CsoundActionSheetItem item;
        item.id = 106; item.text = "Load..."; item.enabled = onLoadSessionRequested != nullptr;
        item.icon = CsoundActionSheetIcon::load;
        items.push_back (item);
    }
    
    // "Initialize Session" (richiesta esplicita: "pulisce tutto e carica
    // il CSD hard coded") - stesso trattamento di Save/Load CSD sopra,
    // PluginEditor imposta onInitializeSessionRequested nel proprio
    // costruttore (vedi promptInitializeSession()/
    // performInitializeSession() in PluginEditor.h/.cpp, che a loro volta
    // richiamano CsoundAudioProcessor::initializeSession()).
    {
        CsoundActionSheetItem item;
        item.id = 111; item.text = "Initialize Session"; item.enabled = onInitializeSessionRequested != nullptr;
        item.icon = CsoundActionSheetIcon::newDocument;
        items.push_back (item);
    }

    // Undo/Redo: stessa cronologia condivisa usata da Cmd+Z/Cmd+Shift+Z da
    // tastiera - qui pero' invocati da un CLICK, non da una scorciatoia:
    // su iOS o in una DAW che non lascia passare gli shortcut da tastiera
    // al plugin, questo e' l'UNICO modo per l'utente di annullare/
    // ripetere. onUndoRequested/onRedoRequested (impostate da PluginEditor
    // con performUndo()/performRedo(), vedi PluginEditor.h) instradano
    // ANCHE verso l'undo testuale dell'editor di codice quando e' lui ad
    // avere il focus - undo()/redo() diretti (fallback se non impostate)
    // agirebbero SEMPRE e SOLO su questo pannello.
    items.push_back (CsoundActionSheetItem::separator());
    {
        CsoundActionSheetItem item;
        item.id = 103; item.text = "Undo"; item.enabled = undoManager.canUndo();
        item.icon = CsoundActionSheetIcon::undo;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 104; item.text = "Redo"; item.enabled = undoManager.canRedo();
        item.icon = CsoundActionSheetIcon::redo;
        items.push_back (item);
    }

    // SafePointer, non [this]: CsoundActionSheet::show() richiama onSelected
    // in modo ASINCRONO (juce::MessageManager::callAsync dentro dismiss(),
    // vedi CsoundActionSheet.cpp) - a differenza di un juce::PopupMenu
    // (che JUCE gestisce con le sue proprie garanzie interne), qui siamo noi
    // a dover garantire che questo pannello sia ANCORA vivo quando la
    // callback arriva (es. l'utente ha chiuso la finestra del plugin mentre
    // il foglio era ancora aperto/in animazione) - altrimenti this->processor
    // e' un riferimento pendente e una qualunque chiamata su di esso
    // (es. getChannelParamSlot -> ScopedLock sul suo CriticalSection) crasha
    // con un indirizzo spazzatura.
    juce::Component::SafePointer<CsoundParameterMappingPanel> safeThis (this);

    CsoundActionSheet::show (*getTopLevelComponent(), "", std::move (items), [safeThis] (int result)
    {
        if (safeThis == nullptr)
            return;

        switch (result)
        {
            case 101: if (safeThis->onToggleParametersRequested) safeThis->onToggleParametersRequested(); break;
            case 102: if (safeThis->onToggleConsoleRequested)    safeThis->onToggleConsoleRequested();    break;
            case 103: if (safeThis->onUndoRequested) safeThis->onUndoRequested(); else safeThis->undo(); break;
            case 104: if (safeThis->onRedoRequested) safeThis->onRedoRequested(); else safeThis->redo(); break;
            case 105: if (safeThis->onSaveSessionRequested) safeThis->onSaveSessionRequested(); break;
            case 106: if (safeThis->onLoadSessionRequested) safeThis->onLoadSessionRequested(); break;
            case 111: if (safeThis->onInitializeSessionRequested) safeThis->onInitializeSessionRequested(); break;
            case 112: if (safeThis->onSaveLinkedRequested) safeThis->onSaveLinkedRequested(); break;
            default: break;
        }
    });
}

void CsoundParameterMappingPanel::removeAllParameters()
{
    // Dialogo di conferma DAVVERO nativo (NSAlert, vedi NativeAlertMac.h/
    // .mm) - richiesto esplicitamente per un'azione distruttiva come
    // questa. "Cancel" e' il bottone di default (primo aggiunto, risponde
    // a Invio): un'azione cosi' distruttiva non deve MAI essere quella che
    // scatta per errore premendo Invio o la barra spaziatrice.
    const int choice = showNativeTwoButtonAlert (
        "Remove all parameters?",
        "This will remove every parameter currently mapped in this plugin. "
        "You can undo this after confirming.",
        "Cancel", "Remove All");

    if (choice != 2)
        return;

    // Snapshot di TUTTI gli slot dei 4 tipi, per poter ripristinare tutto
    // con un solo Undo (azione singola e atomica, come le altre operazioni
    // del pannello - vedi il commento su undo()/redo()).
    std::array<CsoundAudioProcessor::ChannelParamSlot, CsoundAudioProcessor::numChannelParams> beforeFloat;
    std::array<CsoundAudioProcessor::IntParamSlot,     CsoundAudioProcessor::numIntParams>     beforeInt;
    std::array<CsoundAudioProcessor::BoolParamSlot,    CsoundAudioProcessor::numBoolParams>    beforeBool;
    std::array<CsoundAudioProcessor::ChoiceParamSlot,  CsoundAudioProcessor::numChoiceParams>  beforeChoice;

    for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i) beforeFloat[i]  = processor.getChannelParamSlot (i);
    for (int i = 0; i < CsoundAudioProcessor::numIntParams;     ++i) beforeInt[i]    = processor.getIntParamSlot (i);
    for (int i = 0; i < CsoundAudioProcessor::numBoolParams;    ++i) beforeBool[i]   = processor.getBoolParamSlot (i);
    for (int i = 0; i < CsoundAudioProcessor::numChoiceParams;  ++i) beforeChoice[i] = processor.getChoiceParamSlot (i);

    pendingRow = nullptr;

    undoManager.beginNewTransaction();
    undoManager.perform (new LambdaUndoableAction
    {
        [this]
        {
            for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
                processor.setChannelParamSlot (i, CsoundAudioProcessor::ChannelParamSlot {});
            for (int i = 0; i < CsoundAudioProcessor::numIntParams; ++i)
                processor.setIntParamSlot (i, CsoundAudioProcessor::IntParamSlot {});
            for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
                processor.setBoolParamSlot (i, CsoundAudioProcessor::BoolParamSlot {});
            for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
                processor.setChoiceParamSlot (i, CsoundAudioProcessor::ChoiceParamSlot {});

            rowsInEditMode.clear();
            rebuildUnifiedRows();
        },
        [this, beforeFloat, beforeInt, beforeBool, beforeChoice]
        {
            for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i) processor.setChannelParamSlot (i, beforeFloat[i]);
            for (int i = 0; i < CsoundAudioProcessor::numIntParams;     ++i) processor.setIntParamSlot (i, beforeInt[i]);
            for (int i = 0; i < CsoundAudioProcessor::numBoolParams;    ++i) processor.setBoolParamSlot (i, beforeBool[i]);
            for (int i = 0; i < CsoundAudioProcessor::numChoiceParams;  ++i) processor.setChoiceParamSlot (i, beforeChoice[i]);

            rebuildUnifiedRows();
        }
    });

    if (onParameterCopiedToClipboard)
        onParameterCopiedToClipboard ("--- Removed all parameters ---");
}

void CsoundParameterMappingPanel::setAllRowsEditMode (bool edit)
{
    // dynamic_cast: unifiedRows e' un vettore di juce::Component (serve per
    // poterci mettere insieme Float/Int/Bool/Choice), ma ogni elemento
    // implementa ANCHE UnifiedRowInterface (vedi la dichiarazione delle 4
    // *UnifiedRow nel .h) - setEditMode() su ciascuna aggiorna gia' da sola
    // rowsInEditMode (tramite onEditModeToggled, vedi wireEditModeTracking
    // in rebuildUnifiedRows()).
    //
    // BUG corretto qui ("Open/Close Config dal menu: gli slider restano
    // giganteschi, l'altezza non si ridimensiona"): *UnifiedRow::
    // setEditMode() alla fine chiama SOLO il resized() della RIGA stessa
    // (per riposizionare editRow/uiRow/i bottoni DENTRO le sue bounds
    // correnti) - NON quello del pannello, che e' invece agganciato al
    // callback onEditModeChanged passato a ciascuna riga ([this]{resized();}
    // in rebuildUnifiedRows()) e invocato SOLO da editIconButton.onClick.
    // Il bottone occhio di ogni riga quindi funzionava (click -> onClick ->
    // setEditMode() + onEditModeChanged() -> resized() del PANNELLO, che
    // tramite getPreferredHeight() ricalcola l'altezza giusta per la nuova
    // modalita' e riposiziona tutte le righe nel viewport) - ma chiamando
    // setEditMode() direttamente, come qui, quel secondo passaggio non
    // scattava mai: ogni riga restava DENTRO le bounds (quindi l'altezza)
    // della modalita' PRECEDENTE, con lo slider/i campi stirati o
    // schiacciati per starci dentro. Un resized() del pannello alla fine
    // (una volta sola, non per riga) risolve.
    for (auto& row : unifiedRows)
        if (auto* iface = dynamic_cast<UnifiedRowInterface*> (row.get()))
            iface->setEditMode (edit);

    resized();
}

void CsoundParameterMappingPanel::resetAllParametersToInit()
{
    // NESSUN Undo qui (vedi il commento sulla dichiarazione nel .h): i
    // VALORI dei parametri sono gestiti dalla DAW/host, quindi questa lista
    // raccoglie solo (parametro, normalizzato target) - non serve piu' il
    // valore "before" per un undoIt che non esiste.
    struct Entry
    {
        juce::RangedAudioParameter* param;
        float defaultNormalized; // valore normalizzato 0..1 del default/init configurato
    };

    std::vector<Entry> entries;

    auto addEntry = [&entries] (juce::RangedAudioParameter* param, float defaultNormalized)
    {
        if (param != nullptr)
            entries.push_back ({ param, defaultNormalized });
    };

    // Float: il parametro apvts ha gia' range nativo 0..1 (vedi
    // ChannelHostParameter) - normalizeChannelParam va benissimo cosi' com'e'.
    for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
    {
        const auto slot = processor.getChannelParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;

        addEntry (processor.apvts.getParameter (CsoundAudioProcessor::getChannelParamID (i)),
                  CsoundAudioProcessor::normalizeChannelParam (slot, (double) slot.defaultValue));
    }

    // Int: normalizeIntParam gia' restituisce 0..1 (NON il range nativo
    // 0..intHostRangeMax del parametro - quello serve solo a getRawParameterValue()/
    // allo slider, vedi i commenti su IntHostParameter/pushChannelParametersToCsound) -
    // setValueNotifyingHost vuole SEMPRE 0..1, qui e' gia' nella forma giusta.
    for (int i = 0; i < CsoundAudioProcessor::numIntParams; ++i)
    {
        const auto slot = processor.getIntParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;

        addEntry (processor.apvts.getParameter (CsoundAudioProcessor::getIntParamID (i)),
                  CsoundAudioProcessor::normalizeIntParam (slot, (double) slot.defaultValue));
    }

    // Bool: nessuna denormalizzazione, 0.0/1.0 diretto (stessa convenzione
    // di pushChannelParametersToCsound per i Bool).
    for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
    {
        const auto slot = processor.getBoolParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;

        addEntry (processor.apvts.getParameter (CsoundAudioProcessor::getBoolParamID (i)),
                  slot.defaultValue ? 1.0f : 0.0f);
    }

    // Choice: AudioParameterChoice ha un NormalisableRange nativo
    // (0, maxChoiceOptions - 1, 1) - normalizzato 0..1 = indice / (N - 1),
    // stessa logica usata per interpretare getRawParameterValue() altrove
    // ma invertita (qui si normalizza un indice, non si denormalizza).
    for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
    {
        const auto slot = processor.getChoiceParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;

        const float normalized = CsoundAudioProcessor::maxChoiceOptions > 1
            ? (float) slot.defaultIndex / (float) (CsoundAudioProcessor::maxChoiceOptions - 1)
            : 0.0f;

        addEntry (processor.apvts.getParameter (CsoundAudioProcessor::getChoiceParamID (i)), normalized);
    }

    if (entries.empty())
        return;

    for (auto& e : entries)
        e.param->setValueNotifyingHost (e.defaultNormalized);

    if (onParameterCopiedToClipboard)
        onParameterCopiedToClipboard ("--- Reset all parameters to init values ---");
}

std::vector<CsoundActionSheetItem> CsoundParameterMappingPanel::buildAddParameterItems()
{
    // Etichette user-facing (Slider Float/Slider Int/Toggle/Menu);
    // internamente restano Float/Int/Bool/Choice (kind 0..3). ID 1..4: ogni
    // pagina del foglio (CsoundActionSheet) ha un proprio spazio di ID
    // indipendente, quindi non serve piu' l'offset 10..13 di quando questo
    // era un juce::PopupMenu annidato con addSubMenu() dentro lo STESSO
    // menu di showPanelMenu() - vedi CsoundActionSheet.h.
    struct Entry { const char* label; CsoundActionSheetIcon icon; };
    static const Entry entries[] = {
        { "Add Slider Float", CsoundActionSheetIcon::sliderFloat },
        { "Add Slider Int",   CsoundActionSheetIcon::sliderInt },
        { "Add Toggle",       CsoundActionSheetIcon::toggleSwitch },
        { "Add Menu",         CsoundActionSheetIcon::comboMenu },
    };

    std::vector<CsoundActionSheetItem> items;
    for (auto& entry : entries)
    {
        CsoundActionSheetItem item;
        item.id = (int) items.size() + 1;
        item.text = entry.label;
        item.icon = entry.icon;
        items.push_back (item);
    }
    return items;
}

void CsoundParameterMappingPanel::showAddMenu()
{
    // Contenuto ESPANSO (richiesta esplicita: il bottone "+" "ormai e'
    // multifunzionale") - prima erano le 4 sole voci Slider Float/Int/
    // Toggle/Menu di buildAddParameterItems(); ora questo e' anche il
    // posto in cui viveva il submenu "Parameters" del burger (vedi lo
    // storico commento, ora rimosso, su showPanelMenu()): Open/Close
    // Config, Remove Parameters, Reset to INIT Values. ID 1..4 = le 4 voci
    // Add (vedi buildAddParameterItems(), condivise - stesso elenco,
    // stesso mapping addNewParameter(result - 1)); 108..110 per le altre -
    // spazio ID TUTTO di questo foglio (nessun'altra pagina con cui
    // potrebbe collidere, a differenza di quando queste voci erano
    // annidate dentro lo stesso show() di showPanelMenu()).
    std::vector<CsoundActionSheetItem> items = buildAddParameterItems();

    items.push_back (CsoundActionSheetItem::separator());
    {
        CsoundActionSheetItem item;
        item.id = 108; item.text = "Open Configs";
        item.enabled = ! unifiedRows.empty();
        item.icon = CsoundActionSheetIcon::eyeOpen;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 109; item.text = "Close Configs";
        item.enabled = ! unifiedRows.empty();
        item.icon = CsoundActionSheetIcon::eyeClosed;
        items.push_back (item);
    }
    items.push_back (CsoundActionSheetItem::separator());
    {
        CsoundActionSheetItem item;
        item.id = 107; item.text = "Remove Parameters";
        item.enabled = ! unifiedRows.empty() || pendingRow != nullptr;
        item.icon = CsoundActionSheetIcon::trash;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 110; item.text = "Reset to INIT Values";
        item.enabled = ! unifiedRows.empty();
        item.icon = CsoundActionSheetIcon::resetDefault;
        items.push_back (item);
    }

    // SafePointer: vedi il commento identico su showPanelMenu() sopra.
    juce::Component::SafePointer<CsoundParameterMappingPanel> safeThis (this);

    CsoundActionSheet::show (*getTopLevelComponent(), "", std::move (items), [safeThis] (int result)
    {
        if (safeThis == nullptr)
            return;

        if (result >= 1 && result <= 4)
        {
            safeThis->addNewParameter (result - 1);
            return;
        }

        switch (result)
        {
            case 107: safeThis->removeAllParameters(); break;
            case 108: safeThis->setAllRowsEditMode (true); break;
            case 109: safeThis->setAllRowsEditMode (false); break;
            case 110: safeThis->resetAllParametersToInit(); break;
            default: break;
        }
    });
}

void CsoundParameterMappingPanel::addNewParameter (int kind)
{
    // Se la sidebar Parametri e' nascosta, la riga "in sospeso" che stiamo
    // per creare apparirebbe in un pannello invisibile - l'utente non
    // vedrebbe nulla succedere (richiesta esplicita, vale sia dal bottone
    // "+" sia dalla voce "Add parameter" del menu hamburger, che passano
    // ENTRAMBI da qui). onEnsurePanelVisible (impostata da PluginEditor)
    // apre la sidebar SOLO se e' chiusa, senza toccarla se e' gia' aperta.
    if (onEnsurePanelVisible)
        onEnsurePanelVisible();

    // Trova il primo slot LIBERO (channelName vuoto) del tipo scelto - NON
    // scrive ancora nulla nel processor (vedi createPendingRow() e il
    // commento in testa alla classe sul perche': niente nome placeholder,
    // la riga compare vuota con il focus gia' sul campo nome).
    auto notify = [this] (const juce::String& msg)
    {
        if (onParameterCopiedToClipboard)
            onParameterCopiedToClipboard (msg);
    };

    switch (kind)
    {
        case 0:
            for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
                if (processor.getChannelParamSlot (i).channelName.isEmpty())
                {
                    createPendingRow (0, i);
                    return;
                }
            notify ("--- No free Slider Float slots (limit " + juce::String (CsoundAudioProcessor::numChannelParams) + ") ---");
            return;

        case 1:
            for (int i = 0; i < CsoundAudioProcessor::numIntParams; ++i)
                if (processor.getIntParamSlot (i).channelName.isEmpty())
                {
                    createPendingRow (1, i);
                    return;
                }
            notify ("--- No free Slider Int slots (limit " + juce::String (CsoundAudioProcessor::numIntParams) + ") ---");
            return;

        case 2:
            for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
                if (processor.getBoolParamSlot (i).channelName.isEmpty())
                {
                    createPendingRow (2, i);
                    return;
                }
            notify ("--- No free Toggle slots (limit " + juce::String (CsoundAudioProcessor::numBoolParams) + ") ---");
            return;

        case 3:
            for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
                if (processor.getChoiceParamSlot (i).channelName.isEmpty())
                {
                    createPendingRow (3, i);
                    return;
                }
            notify ("--- No free Menu slots (limit " + juce::String (CsoundAudioProcessor::numChoiceParams) + ") ---");
            return;

        default:
            return;
    }
}

void CsoundParameterMappingPanel::createPendingRow (int kind, int slotIndex)
{
    // Un solo pending alla volta - se ce n'era gia' uno (non ancora
    // promosso/scartato) viene distrutto subito: il suo eventuale
    // callAsync in volo (vedi onDiscard/onPromote sotto) trovera' comunque
    // pendingRow nullptr o rimpiazzato e non fara' nulla di pericoloso.
    pendingRow.reset();

    auto onCopied = [this] (const juce::String& msg)
    {
        if (onParameterCopiedToClipboard)
            onParameterCopiedToClipboard (msg);
    };

    // Invio/focus perso con il nome ANCORA vuoto: scarta la riga sospesa,
    // nessuno slot resta allocato (ParamRow::commitFromFields scrive
    // comunque un ChannelParamSlot{} vuoto, identico a prima - nessun
    // residuo). MAI sincrono nel callback che lo richiama - vedi il
    // commento su onRemoveRequested in CsoundParameterEditor.h.
    auto onDiscard = [this]
    {
        juce::MessageManager::callAsync ([this]
        {
            pendingRow = nullptr;
            resized();
        });
    };

    // Richiamata dalla editIconButton di QUALUNQUE riga (non solo quella
    // sospesa) per far rifluire l'intera lista quando una riga apre/chiude
    // la sua vista Edit - l'altezza della riga cambia ma tutte le righe
    // sotto devono spostarsi di conseguenza, non solo quella toccata.
    auto onEditModeChanged = [this] { resized(); };

    UnifiedRowInterface* iface = nullptr;

    switch (kind)
    {
        case 0:
        {
            auto row = std::make_unique<FloatUnifiedRow> (processor, slotIndex, onCopied, onDiscard, onEditModeChanged);
            row->editRow->onCommittedNonEmpty = [this, slotIndex, onCopied]
            {
                juce::MessageManager::callAsync ([this, slotIndex, onCopied]
                {
                    const auto name = processor.getChannelParamSlot (slotIndex).channelName;
                    pendingRow = nullptr;
                    // Resta in modalita' Edit dopo la prima promozione da
                    // "+" (richiesta esplicita: l'UNICO modo per passare
                    // alla vista UI deve essere il bottone Edit) - segnato
                    // PRIMA del rebuild cosi' wireEditModeTracking lo trova
                    // gia' marcato e riapre la riga in Edit.
                    rowsInEditMode.insert ({ 0, slotIndex });
                    rebuildUnifiedRows();
                    onCopied ("--- Added Slider Float parameter: " + name + " ---");
                });
            };
            iface = row.get();
            pendingRow = std::move (row);
            break;
        }
        case 1:
        {
            auto row = std::make_unique<IntUnifiedRow> (processor, slotIndex, onCopied, onDiscard, onEditModeChanged);
            row->editRow->onCommittedNonEmpty = [this, slotIndex, onCopied]
            {
                juce::MessageManager::callAsync ([this, slotIndex, onCopied]
                {
                    const auto name = processor.getIntParamSlot (slotIndex).channelName;
                    pendingRow = nullptr;
                    rowsInEditMode.insert ({ 1, slotIndex });
                    rebuildUnifiedRows();
                    onCopied ("--- Added Slider Int parameter: " + name + " ---");
                });
            };
            iface = row.get();
            pendingRow = std::move (row);
            break;
        }
        case 2:
        {
            auto row = std::make_unique<BoolUnifiedRow> (processor, slotIndex, onCopied, onDiscard, onEditModeChanged);
            row->editRow->onCommittedNonEmpty = [this, slotIndex, onCopied]
            {
                juce::MessageManager::callAsync ([this, slotIndex, onCopied]
                {
                    const auto name = processor.getBoolParamSlot (slotIndex).channelName;
                    pendingRow = nullptr;
                    rowsInEditMode.insert ({ 2, slotIndex });
                    rebuildUnifiedRows();
                    onCopied ("--- Added Toggle parameter: " + name + " ---");
                });
            };
            iface = row.get();
            pendingRow = std::move (row);
            break;
        }
        case 3:
        {
            auto row = std::make_unique<ChoiceUnifiedRow> (processor, slotIndex, onCopied, onDiscard, onEditModeChanged);
            row->editRow->onCommittedNonEmpty = [this, slotIndex, onCopied]
            {
                juce::MessageManager::callAsync ([this, slotIndex, onCopied]
                {
                    const auto name = processor.getChoiceParamSlot (slotIndex).channelName;
                    pendingRow = nullptr;
                    rowsInEditMode.insert ({ 3, slotIndex });
                    rebuildUnifiedRows();
                    onCopied ("--- Added Menu parameter: " + name + " ---");
                });
            };
            iface = row.get();
            pendingRow = std::move (row);
            break;
        }
        default:
            return;
    }

    rowsContainer.addAndMakeVisible (*pendingRow);
    iface->setEditMode (true); // la riga sospesa mostra SEMPRE i campi fin da subito (focus sul nome)

    viewport.setVisible (true);
    emptyStateLabel.setVisible (false);

    resized();

    viewport.setViewPosition (viewport.getViewPositionX(), juce::jmax (0, rowsContainer.getHeight() - viewport.getHeight()));
    iface->focusNameField();
}

void CsoundParameterMappingPanel::rebuildUnifiedRows()
{
    unifiedRows.clear();

    // Scarta dal tracking gli slot ormai vuoti (rimossi, o un Undo che li
    // ha appena svuotati) - altrimenti un futuro "+" che riusa lo stesso
    // slotIndex per un parametro DIVERSO lo troverebbe gia' segnato come
    // "era in modalita' Edit" e si apirebbe in Edit senza che l'utente
    // l'abbia chiesto.
    for (auto it = rowsInEditMode.begin(); it != rowsInEditMode.end(); )
    {
        const int kind = it->first;
        const int idx  = it->second;
        bool stillPresent = false;

        switch (kind)
        {
            case 0: stillPresent = processor.getChannelParamSlot (idx).channelName.isNotEmpty(); break;
            case 1: stillPresent = processor.getIntParamSlot (idx).channelName.isNotEmpty();     break;
            case 2: stillPresent = processor.getBoolParamSlot (idx).channelName.isNotEmpty();    break;
            case 3: stillPresent = processor.getChoiceParamSlot (idx).channelName.isNotEmpty();  break;
            default: break;
        }

        if (stillPresent)
            ++it;
        else
            it = rowsInEditMode.erase (it);
    }

    auto onCopied = [this] (const juce::String& msg)
    {
        if (onParameterCopiedToClipboard)
            onParameterCopiedToClipboard (msg);
    };

    // juce::MessageManager::callAsync: MAI distruggere/ricostruire la lista
    // SINCRONAMENTE dentro il callback che l'ha richiesta (es. il click del
    // removeButton di una riga che stiamo per distruggere).
    auto onRemoved = [this] { juce::MessageManager::callAsync ([this] { rebuildUnifiedRows(); }); };

    // Stessa identica callback di createPendingRow(): la editIconButton di
    // QUALUNQUE riga ricostruita qui deve poter far rifluire l'intera lista
    // quando apre/chiude la sua vista Edit (vedi il commento li').
    auto onEditModeChanged = [this] { resized(); };

    // Collega pushUndo di una riga Edit all'undoManager condiviso - vedi il
    // commento su pushUndo in CsoundParameterEditor.h: le due lambda
    // ricevute (doIt/undoIt, costruite da commitFromFields()/removeButton
    // e dintorni, che NON catturano mai `this` della riga) vengono avvolte
    // qui in un rebuildUnifiedRows() DOPO l'esecuzione - necessario perche'
    // una rimozione/ripristino fa apparire/sparire righe, e perche' NON
    // possiamo sapere qui se la riga toccata da un dato Undo esiste ancora
    // (rebuildUnifiedRows() distrugge e ricrea TUTTE le righe, comprese
    // quelle non toccate da questa specifica azione).
    auto wirePushUndo = [this] (std::function<void (std::function<void()>, std::function<void()>)>& pushUndo)
    {
        pushUndo = [this] (std::function<void()> doIt, std::function<void()> undoIt)
        {
            // Senza beginNewTransaction() JUCE accoda questa azione alla
            // transazione "corrente" (quella apertasi con la primissima
            // perform() mai chiamata su questo undoManager), cosicche' un
            // singolo Cmd+Z disfarebbe TUTTE le modifiche fatte finora in
            // un colpo solo. Aprendo qui una transazione nuova ad ogni
            // singola operazione (ogni rimozione/modifica di metadata e'
            // gia' di per se' un'unita' atomica discreta), Cmd+Z risale la
            // pila un passo alla volta, come un Undo normale.
            undoManager.beginNewTransaction();
            undoManager.perform (new LambdaUndoableAction
            {
                [this, doIt]   { doIt();   rebuildUnifiedRows(); },
                [this, undoIt] { undoIt(); rebuildUnifiedRows(); }
            });
        };
    };

    // Riaggancia il callback onEditModeToggled di una riga appena creata al
    // tracking persistente rowsInEditMode (vedi il commento sulla sua
    // dichiarazione in CsoundParameterEditor.h) e, se questo slot era
    // segnato come "era in modalita' Edit" prima di questo rebuild, la
    // riapre immediatamente - cosi' un commit di un singolo campo durante
    // l'editing (che causa un rebuild via pushUndo sopra) lascia la riga
    // esattamente dov'era, invece di farla ripiombare sulla vista UI.
    auto wireEditModeTracking = [this] (UnifiedRowInterface& iface, std::function<void (bool)>& onEditModeToggled, int kind, int idx)
    {
        onEditModeToggled = [this, kind, idx] (bool edit)
        {
            const std::pair<int, int> key { kind, idx };
            if (edit)
                rowsInEditMode.insert (key);
            else
                rowsInEditMode.erase (key);
        };

        if (rowsInEditMode.count ({ kind, idx }) > 0)
            iface.setEditMode (true);
    };

    for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
    {
        if (processor.getChannelParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<FloatUnifiedRow> (processor, i, onCopied, onRemoved, onEditModeChanged);
        wirePushUndo (row->editRow->pushUndo);
        wireEditModeTracking (*row, row->onEditModeToggled, 0, i);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
    }

    for (int i = 0; i < CsoundAudioProcessor::numIntParams; ++i)
    {
        if (processor.getIntParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<IntUnifiedRow> (processor, i, onCopied, onRemoved, onEditModeChanged);
        wirePushUndo (row->editRow->pushUndo);
        wireEditModeTracking (*row, row->onEditModeToggled, 1, i);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
    }

    for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
    {
        if (processor.getBoolParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<BoolUnifiedRow> (processor, i, onCopied, onRemoved, onEditModeChanged);
        wirePushUndo (row->editRow->pushUndo);
        wireEditModeTracking (*row, row->onEditModeToggled, 2, i);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
    }

    for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
    {
        if (processor.getChoiceParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<ChoiceUnifiedRow> (processor, i, onCopied, onRemoved, onEditModeChanged);
        wirePushUndo (row->editRow->pushUndo);
        wireEditModeTracking (*row, row->onEditModeToggled, 3, i);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
    }

    const bool empty = unifiedRows.empty() && pendingRow == nullptr;
    viewport.setVisible (! empty);
    emptyStateLabel.setVisible (empty);

    resized();

    // Ogni modifica alla STRUTTURA (aggiunta/rimozione/metadata, undo/redo
    // compresi, e i refresh dopo Load/ripristino) passa da qui: e' il punto
    // unico da cui PluginEditor puo' ricontrollare se la sessione differisce
    // dal file su disco - vedi onMappingChanged nel .h.
    if (onMappingChanged)
        onMappingChanged();
}

void CsoundParameterMappingPanel::refreshAllFromProcessor()
{
    // Ricostruire da zero (invece di rileggere riga per riga) e' la via
    // piu' semplice e sicura per riflettere un Load Session: non solo i
    // VALORI dei metadata possono essere cambiati, ma anche QUALI slot sono
    // allocati - una lista che "appare solo se allocato" deve comunque far
    // apparire/sparire righe di conseguenza. Una eventuale pendingRow in
    // sospeso viene scartata (il nuovo stato caricato la rende comunque
    // priva di senso).
    pendingRow = nullptr;
    rebuildUnifiedRows();
}

void CsoundParameterMappingPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (kPanelBg);
    g.fillRect (bounds);

    // Title bar (richiesta esplicita: "Riabilita la Title bar in
    // Parameters") - stesso sfondo/separatore della toolbar PRINCIPALE di
    // PluginEditor (vedi PluginEditor::paint(), 0xff10181f/0xff2a3a44), per
    // coerenza visiva tra le due barre superiori dell'editor. titleBarBounds
    // e' calcolato in resized() (stessa area usata per posizionare
    // addButton/titleLabel), non ricalcolato qui.
    g.setColour (juce::Colour (0xff10181f));
    g.fillRect (titleBarBounds);

    g.setColour (juce::Colour (0xff2a3a44));
    g.drawLine ((float) titleBarBounds.getX(),     (float) titleBarBounds.getBottom() - 0.5f,
                (float) titleBarBounds.getRight(), (float) titleBarBounds.getBottom() - 0.5f, 1.0f);

    g.setColour (kAccent.withAlpha (0.6f));
    g.drawLine (0.5f, 0.0f, 0.5f, (float) getHeight(), 1.2f);
}

void CsoundParameterMappingPanel::resized()
{
    auto full = getLocalBounds();

    // Title bar (richiesta esplicita, vedi il commento in testa alla
    // dichiarazione di titleLabel nel .h): addButton a sinistra (ora
    // multifunzionale, vedi il commento sul suo setName("paramsMenu") nel
    // costruttore), titleLabel centrata su TUTTA la larghezza della barra
    // (non solo lo spazio residuo) cosi' il titolo resta visivamente
    // centrato nel pannello. titleBarBounds salvato per paint() sopra.
    titleBarBounds = full.removeFromTop (panelTitleBarHeight);
    {
        auto bar = titleBarBounds.reduced (panelTitleBarPaddingH, 0);
        addButton.setBounds (bar.removeFromLeft (panelTitleBarButtonDiameter)
                                 .withSizeKeepingCentre (panelTitleBarButtonDiameter, panelTitleBarButtonDiameter));
        titleLabel.setBounds (titleBarBounds);
    }

    auto area = full.reduced (4);

    emptyStateLabel.setBounds (area.reduced (24));

    viewport.setBounds (area);

    // Non c'e' piu' un minimo forzato: se il pannello e' piu' stretto della
    // riga di campi piu' larga, i campi vanno a capo (vedi
    // computeWrappedFieldRows()/layoutWrappedFields()) invece di mostrare
    // una scrollbar orizzontale - richiesto esplicitamente.
    const int contentWidth = juce::jmax (120, viewport.getWidth() - scrollbarGutter);

    int y = 0;

    // Sia in modalita' Edit che UI le righe sono "card" (vedi
    // paintCardChrome()/GenericParamRow::paint()) separate da uno spazio
    // (cardGap) che lascia trasparire lo sfondo scuro del pannello
    // (kPanelBg) tra una card e la successiva.
    const int gapBetweenRows = cardGap;

    for (auto& row : unifiedRows)
    {
        auto* iface = dynamic_cast<UnifiedRowInterface*> (row.get());
        const int h = iface != nullptr ? iface->getPreferredHeight (contentWidth) : rowHeight;
        row->setBounds (0, y, contentWidth, h);
        y += h + gapBetweenRows;
    }

    if (pendingRow != nullptr)
    {
        auto* iface = dynamic_cast<UnifiedRowInterface*> (pendingRow.get());
        const int h = iface != nullptr ? iface->getPreferredHeight (contentWidth) : rowHeight;
        pendingRow->setBounds (0, y, contentWidth, h);
        y += h + gapBetweenRows;
    }

    rowsContainer.setSize (contentWidth, y);
}
