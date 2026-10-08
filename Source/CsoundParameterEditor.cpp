#include "CsoundParameterEditor.h"

namespace
{
    // Tema SCURO dedicato a questo pannello (stesso spirito della console
    // di log in PluginEditor, che e' gia' scura): il resto dell'app usa un
    // tema chiaro (CsoundLookAndFeel) sotto cui il testo nero di default di
    // TextEditor/ComboBox risultava illeggibile su alcuni sfondi/stati -
    // qui i colori vengono impostati ESPLICITAMENTE su ogni componente,
    // cosi' il contrasto e' garantito a prescindere dal tema globale.
    const juce::Colour kPanelBg      { 0xff10181f }; // come logConsole in PluginEditor
    const juce::Colour kCardBg       { 0xff19232c }; // sfondo di ogni "card" Edit, leggermente piu' chiaro del pannello
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
    // l'indentazione - usato dal tasto destro sulla maniglia di ciascuna
    // riga per copiare negli appunti senza dover trascinare fisicamente.
    juce::String makeChngetClipboardText (const juce::String& channelName, const juce::String& configComment)
    {
        const auto varName = "k" + channelName.removeCharacters (" \t");
        return makeConfigCommentLine (configComment) + varName + " chnget \"" + channelName + "\"\n";
    }

    // Menu contestuale "Copy" mostrato dal tasto destro sulla maniglia -
    // NON copia automaticamente al solo right-click: deve comparire un vero
    // menu con una voce "Copy" da selezionare. channelName vuoto -> voce
    // disabilitata. notifyResult riceve il messaggio da mostrare in
    // consolle SOLO se l'utente seleziona davvero "Copy". configComment e'
    // il contenuto SENZA braces/delimitatori (vedi makeFloatConfigComment
    // ecc. sopra) - puo' essere vuoto (nessun commento aggiunto).
    void showCopyChngetMenu (const juce::String& channelName, const juce::String& configComment,
                              std::function<void (const juce::String&)> notifyResult)
    {
        juce::PopupMenu menu;
        constexpr int copyItemId = 1;
        menu.addItem (copyItemId, "Copy", channelName.isNotEmpty());

        menu.showMenuAsync (juce::PopupMenu::Options(),
            [channelName, configComment, notifyResult] (int result)
            {
                if (result != copyItemId)
                    return; // menu chiuso senza scegliere "Copy"

                const auto clip = makeChngetClipboardText (channelName, configComment);
                juce::SystemClipboard::copyTextToClipboard (clip);

                if (notifyResult)
                    notifyResult ("--- Copied to clipboard: " + clip.trim() + " ---");
            });
    }

    // Stesso identico path SVG di CsoundLookAndFeel's makeTuneIconPath (il
    // bottone "Parameters" nella toolbar principale) - usata per il
    // bottone "Edit" di questo pannello.
    juce::Path makeTuneIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M3,17V19H9V17H3M3,5V7H13V5H3M13,21V19H21V17H13V15H11V21H13M7,9V11H3V13H7V15H9V9H7M21,"
            "13V11H11V13H21M15,9H17V7H21V5H17V3H15V9Z");
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

    // Bottone "Edit" (riconosciuto dal nome "editToggle"): SOLO icona
    // (tune), nessun testo, centrata - circolare come "+" (richiesta
    // esplicita), controllato PRIMA del ramo "circular" qui sotto perche'
    // anche "Edit" ha quella proprieta' impostata (stesso aspetto rotondo
    // di "+"), ma deve disegnare l'icona "tune", non il simbolo "+".
    if (button.getName() == "editToggle")
    {
        auto textColour = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                                       : juce::TextButton::textColourOffId);
        g.setColour (textColour);

        auto bounds = button.getLocalBounds().toFloat();
        auto icon = makeTuneIconPath();

        const float iconSize = juce::jmin (bounds.getHeight() * 0.42f, 16.0f);
        auto iconArea = bounds.withSizeKeepingCentre (iconSize, iconSize);
        icon.scaleToFit (iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true);
        g.fillPath (icon);
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
    removeArea = header.removeFromRight (cardHeaderHeight).reduced (1);
    header.removeFromRight (6);
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

void CsoundParameterMappingPanel::layoutCaptionedField (juce::Rectangle<int> cell, juce::Label& caption, juce::Component& field)
{
    caption.setBounds (cell.removeFromTop (cardFieldCaptionHeight));
    cell.removeFromTop (cardFieldCaptionGap);
    field.setBounds (cell.removeFromTop (cardFieldBoxHeight));
}

void CsoundParameterMappingPanel::paintCardChrome (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour accent,
                                                     juce::Rectangle<int> handleStrip, bool showHandleDots, bool handleHovered)
{
    g.setColour (kCardBg);
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

    setupFieldCaption (expCaption, "EXP");
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

    removeButton.setColour (juce::TextButton::buttonColourId, kDanger);
    removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    removeButton.setTooltip ("Remove this parameter");
    removeButton.onClick = [this]
    {
        // Il nome va letto QUI, prima di svuotarlo: dopo commitFromFields()
        // lo slot (e quindi channelNameEditor) sono gia' vuoti.
        const auto removedName = channelNameEditor.getText().trim();
        channelNameEditor.setText ({}, false);
        commitFromFields();
        if (onCopiedToClipboard && removedName.isNotEmpty())
            onCopiedToClipboard ("--- Removed parameter: " + removedName + " ---");
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
    editor.giveAwayKeyboardFocus();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::ParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
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
    commitFromFields();
}

void CsoundParameterMappingPanel::ParamRow::paint (juce::Graphics& g)
{
    const bool hasName = processor.getChannelParamSlot (index).channelName.isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), kSliderAccent, handleBounds, hasName, handleHovered);
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
    if (event.mods.isPopupMenu() && handleBounds.contains (event.getPosition()))
    {
        const auto slot = processor.getChannelParamSlot (index);
        showCopyChngetMenu (slot.channelName, makeFloatConfigComment (slot), onCopiedToClipboard);
        return;
    }

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

    removeButton.setColour (juce::TextButton::buttonColourId, kDanger);
    removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    removeButton.setTooltip ("Remove this parameter");
    removeButton.onClick = [this]
    {
        // Vedi il commento identico su ParamRow::removeButton.onClick sopra.
        const auto removedName = channelNameEditor.getText().trim();
        channelNameEditor.setText ({}, false);
        commitFromFields();
        if (onCopiedToClipboard && removedName.isNotEmpty())
            onCopiedToClipboard ("--- Removed parameter: " + removedName + " ---");
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
    editor.giveAwayKeyboardFocus();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::IntParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::IntParamRow::textEditorTextChanged (juce::TextEditor&)
{
    commitFromFields();
}

void CsoundParameterMappingPanel::IntParamRow::paint (juce::Graphics& g)
{
    const bool hasName = processor.getIntParamSlot (index).channelName.isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), kKnobAccent, handleBounds, hasName, handleHovered);
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
    if (event.mods.isPopupMenu() && handleBounds.contains (event.getPosition()))
    {
        const auto slot = processor.getIntParamSlot (index);
        showCopyChngetMenu (slot.channelName, makeIntConfigComment (slot), onCopiedToClipboard);
        return;
    }

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
    defaultToggle.onClick = [this] { commitFromFields(); };
    addAndMakeVisible (defaultToggle);

    removeButton.setColour (juce::TextButton::buttonColourId, kDanger);
    removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    removeButton.setTooltip ("Remove this parameter");
    removeButton.onClick = [this]
    {
        const auto removedName = channelNameEditor.getText().trim();
        channelNameEditor.setText ({}, false);
        commitFromFields();
        if (onCopiedToClipboard && removedName.isNotEmpty())
            onCopiedToClipboard ("--- Removed parameter: " + removedName + " ---");
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
    editor.giveAwayKeyboardFocus();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::BoolParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::BoolParamRow::textEditorTextChanged (juce::TextEditor&)
{
    commitFromFields();
}

void CsoundParameterMappingPanel::BoolParamRow::paint (juce::Graphics& g)
{
    const bool hasName = processor.getBoolParamSlot (index).channelName.isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), kToggleAccent, handleBounds, hasName, handleHovered);
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
    if (event.mods.isPopupMenu() && handleBounds.contains (event.getPosition()))
    {
        const auto slot = processor.getBoolParamSlot (index);
        showCopyChngetMenu (slot.channelName, makeBoolConfigComment (slot), onCopiedToClipboard);
        return;
    }

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

    setupFieldCaption (defaultCaption, "DEFAULT");
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
    defaultIndexCombo.onChange = [this] { commitFromFields(); };
    addAndMakeVisible (defaultIndexCombo);

    removeButton.setColour (juce::TextButton::buttonColourId, kDanger);
    removeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    removeButton.setTooltip ("Remove this parameter");
    removeButton.onClick = [this]
    {
        const auto removedName = channelNameEditor.getText().trim();
        channelNameEditor.setText ({}, false);
        commitFromFields();
        if (onCopiedToClipboard && removedName.isNotEmpty())
            onCopiedToClipboard ("--- Removed parameter: " + removedName + " ---");
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
    const auto idToSelect = juce::jlimit (0, defaultIndexCombo.getNumItems() - 1, slot.defaultIndex) + 1;
    if (defaultIndexCombo.getNumItems() > 0)
        defaultIndexCombo.setSelectedId (idToSelect, juce::dontSendNotification);
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
    editor.giveAwayKeyboardFocus();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::ChoiceParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
    notifyRemovedIfEmpty();
    notifyCommittedIfNonEmpty();
}

void CsoundParameterMappingPanel::ChoiceParamRow::textEditorTextChanged (juce::TextEditor&)
{
    commitFromFields();
}

void CsoundParameterMappingPanel::ChoiceParamRow::paint (juce::Graphics& g)
{
    const bool hasName = processor.getChoiceParamSlot (index).channelName.isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), kMenuAccent, handleBounds, hasName, handleHovered);
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
    if (event.mods.isPopupMenu() && handleBounds.contains (event.getPosition()))
    {
        const auto slot = processor.getChoiceParamSlot (index);
        showCopyChngetMenu (slot.channelName, makeChoiceConfigComment (slot), onCopiedToClipboard);
        return;
    }

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
    std::function<juce::String()> getConfigCommentFn, bool treatAsInteger)
    : kind (rowKind), accentColour (accent), isIntegerLike (treatAsInteger),
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

            sliderAttachment = std::make_unique<juce::SliderParameterAttachment> (parameter, slider);
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
            minLabel.setText (juce::String ((int) std::round (slider.getMinimum())), juce::dontSendNotification);
            maxLabel.setText (juce::String ((int) std::round (slider.getMaximum())), juce::dontSendNotification);
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
            addAndMakeVisible (valueReadout);

            slider.onValueChange = [this] { updateValueReadout(); };
            updateValueReadout();
            break;
        }

        case Kind::toggle:
            toggle.setName ("pillToggle");
            addAndMakeVisible (toggle);
            buttonAttachment = std::make_unique<juce::ButtonParameterAttachment> (parameter, toggle);
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
            comboAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (parameter, comboBox);
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

    const auto value = slider.getValue();
    valueReadout.setText (isIntegerLike ? juce::String ((int) std::round (value))
                                         : juce::String (value, 3),
                          false);
}

void CsoundParameterMappingPanel::GenericParamRow::commitValueFromField()
{
    const auto text = valueReadout.getText().trim();
    if (text.isNotEmpty())
    {
        const auto typed = text.getDoubleValue();
        slider.setValue (juce::jlimit (slider.getMinimum(), slider.getMaximum(), typed), juce::sendNotificationSync);
    }

    // Rilegge comunque il valore (clampato/riformattato, o invariato se il
    // testo non era un numero valido) - cosi' il campo non resta mai con un
    // testo "sporco" dopo un Return/focus perso.
    const auto value = slider.getValue();
    valueReadout.setText (isIntegerLike ? juce::String ((int) std::round (value))
                                         : juce::String (value, 3),
                          false);
}

void CsoundParameterMappingPanel::GenericParamRow::paint (juce::Graphics& g)
{
    // Stessa maniglia (con puntini di trascinamento) delle card Edit -
    // trascinabile anche qui, vedi mouseDown/mouseDrag sotto.
    const bool hasName = getChannelName && getChannelName().isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), accentColour, handleBounds, hasName, handleHovered);
}

void CsoundParameterMappingPanel::GenericParamRow::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() && handleBounds.contains (event.getPosition()))
    {
        showCopyChngetMenu (getChannelName ? getChannelName() : juce::String(),
                             getConfigComment ? getConfigComment() : juce::String(),
                             onCopiedToClipboard);
        return;
    }

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
    std::function<void (const juce::String&)> onCopied, std::function<void()> onRemoved)
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
                                                     false);
        uiRow->onCopiedToClipboard = onCopied;
        addChildComponent (*uiRow);
    }

    setEditMode (false);
}

void CsoundParameterMappingPanel::FloatUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (getLocalBounds());
}

void CsoundParameterMappingPanel::FloatUnifiedRow::setEditMode (bool edit)
{
    editRow->setVisible (edit);
    if (uiRow != nullptr)
        uiRow->setVisible (! edit);
    resized();
}

CsoundParameterMappingPanel::IntUnifiedRow::IntUnifiedRow (
    CsoundAudioProcessor& processorToEdit, int slotIndex,
    std::function<void (const juce::String&)> onCopied, std::function<void()> onRemoved)
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
                                                     true);
        uiRow->onCopiedToClipboard = onCopied;
        addChildComponent (*uiRow);
    }

    setEditMode (false);
}

void CsoundParameterMappingPanel::IntUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (getLocalBounds());
}

void CsoundParameterMappingPanel::IntUnifiedRow::setEditMode (bool edit)
{
    editRow->setVisible (edit);
    if (uiRow != nullptr)
        uiRow->setVisible (! edit);
    resized();
}

CsoundParameterMappingPanel::BoolUnifiedRow::BoolUnifiedRow (
    CsoundAudioProcessor& processorToEdit, int slotIndex,
    std::function<void (const juce::String&)> onCopied, std::function<void()> onRemoved)
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

    setEditMode (false);
}

void CsoundParameterMappingPanel::BoolUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (getLocalBounds());
}

void CsoundParameterMappingPanel::BoolUnifiedRow::setEditMode (bool edit)
{
    editRow->setVisible (edit);
    if (uiRow != nullptr)
        uiRow->setVisible (! edit);
    resized();
}

CsoundParameterMappingPanel::ChoiceUnifiedRow::ChoiceUnifiedRow (
    CsoundAudioProcessor& processorToEdit, int slotIndex,
    std::function<void (const juce::String&)> onCopied, std::function<void()> onRemoved)
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

    setEditMode (false);
}

void CsoundParameterMappingPanel::ChoiceUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (getLocalBounds());
}

void CsoundParameterMappingPanel::ChoiceUnifiedRow::setEditMode (bool edit)
{
    editRow->setVisible (edit);
    if (uiRow != nullptr)
        uiRow->setVisible (! edit);
    resized();
}

//==============================================================================
CsoundParameterMappingPanel::CsoundParameterMappingPanel (CsoundAudioProcessor& processorToEdit)
    : processor (processorToEdit)
{
    setLookAndFeel (&lookAndFeel);

    // Circolare come "+" (proprieta' dinamica "circular", vedi
    // CsoundParameterPanelLookAndFeel) - sola icona "tune" (nessun testo,
    // vedi drawButtonText), richiesto esplicitamente.
    editToggleButton.setName ("editToggle");
    editToggleButton.getProperties().set ("circular", true);
    editToggleButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a3540));
    editToggleButton.setColour (juce::TextButton::textColourOffId, kText);
    editToggleButton.setTooltip ("Toggle Edit mode");
    editToggleButton.onClick = [this] { toggleEditMode(); };
    addAndMakeVisible (editToggleButton);

    // Bottone "+"/Add: circolare e accentato (proprieta' dinamica
    // "circular", vedi CsoundParameterPanelLookAndFeel), icona "+" disegnata
    // a mano invece del testo - richiesto esplicitamente.
    addButton.getProperties().set ("circular", true);
    addButton.setColour (juce::TextButton::buttonColourId, kAccent);
    addButton.setTooltip ("Add a parameter");
    addButton.onClick = [this] { showAddMenu(); };
    addAndMakeVisible (addButton);

    viewport.setViewedComponent (&rowsContainer, false);
    // Solo verticale: il contenuto ora va sempre a capo per restare entro
    // la larghezza disponibile (vedi computeWrappedFieldRows()), quindi non
    // serve piu' scorrimento orizzontale.
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    emptyStateLabel.setText (
        "Use the + button to add a Slider Float, Slider Int, Toggle, or Menu parameter.",
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

void CsoundParameterMappingPanel::toggleEditMode()
{
    editMode = ! editMode;

    editToggleButton.setColour (juce::TextButton::buttonColourId, editMode ? kAccent : juce::Colour (0xff2a3540));
    editToggleButton.setColour (juce::TextButton::textColourOffId, editMode ? juce::Colours::white : kText);

    for (auto& row : unifiedRows)
        if (auto* iface = dynamic_cast<UnifiedRowInterface*> (row.get()))
            iface->setEditMode (editMode);

    // Le altezze preferite dipendono da editMode (le card Edit sono molto
    // piu' alte di una riga UI) - senza questo resized() le righe
    // cambierebbero contenuto ma non altezza fino al prossimo
    // ridimensionamento della finestra.
    resized();
}

void CsoundParameterMappingPanel::showAddMenu()
{
    // Etichette user-facing (Slider Float/Slider Int/Toggle/Menu);
    // internamente restano Float/Int/Bool/Choice (kind 0..3).
    juce::PopupMenu menu;
    menu.addItem (1, "Slider Float");
    menu.addItem (2, "Slider Int");
    menu.addItem (3, "Toggle");
    menu.addItem (4, "Menu");

    menu.showMenuAsync (juce::PopupMenu::Options(), [this] (int result)
    {
        if (result >= 1 && result <= 4)
            addNewParameter (result - 1);
    });
}

void CsoundParameterMappingPanel::addNewParameter (int kind)
{
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

    UnifiedRowInterface* iface = nullptr;

    switch (kind)
    {
        case 0:
        {
            auto row = std::make_unique<FloatUnifiedRow> (processor, slotIndex, onCopied, onDiscard);
            row->editRow->onCommittedNonEmpty = [this, slotIndex, onCopied]
            {
                juce::MessageManager::callAsync ([this, slotIndex, onCopied]
                {
                    const auto name = processor.getChannelParamSlot (slotIndex).channelName;
                    pendingRow = nullptr;
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
            auto row = std::make_unique<IntUnifiedRow> (processor, slotIndex, onCopied, onDiscard);
            row->editRow->onCommittedNonEmpty = [this, slotIndex, onCopied]
            {
                juce::MessageManager::callAsync ([this, slotIndex, onCopied]
                {
                    const auto name = processor.getIntParamSlot (slotIndex).channelName;
                    pendingRow = nullptr;
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
            auto row = std::make_unique<BoolUnifiedRow> (processor, slotIndex, onCopied, onDiscard);
            row->editRow->onCommittedNonEmpty = [this, slotIndex, onCopied]
            {
                juce::MessageManager::callAsync ([this, slotIndex, onCopied]
                {
                    const auto name = processor.getBoolParamSlot (slotIndex).channelName;
                    pendingRow = nullptr;
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
            auto row = std::make_unique<ChoiceUnifiedRow> (processor, slotIndex, onCopied, onDiscard);
            row->editRow->onCommittedNonEmpty = [this, slotIndex, onCopied]
            {
                juce::MessageManager::callAsync ([this, slotIndex, onCopied]
                {
                    const auto name = processor.getChoiceParamSlot (slotIndex).channelName;
                    pendingRow = nullptr;
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
    iface->setEditMode (true); // la riga sospesa mostra SEMPRE i campi, a prescindere da editMode globale

    viewport.setVisible (true);
    emptyStateLabel.setVisible (false);

    resized();

    viewport.setViewPosition (viewport.getViewPositionX(), juce::jmax (0, rowsContainer.getHeight() - viewport.getHeight()));
    iface->focusNameField();
}

void CsoundParameterMappingPanel::rebuildUnifiedRows()
{
    unifiedRows.clear();

    auto onCopied = [this] (const juce::String& msg)
    {
        if (onParameterCopiedToClipboard)
            onParameterCopiedToClipboard (msg);
    };

    // juce::MessageManager::callAsync: MAI distruggere/ricostruire la lista
    // SINCRONAMENTE dentro il callback che l'ha richiesta (es. il click del
    // removeButton di una riga che stiamo per distruggere).
    auto onRemoved = [this] { juce::MessageManager::callAsync ([this] { rebuildUnifiedRows(); }); };

    for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
    {
        if (processor.getChannelParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<FloatUnifiedRow> (processor, i, onCopied, onRemoved);
        row->setEditMode (editMode);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
    }

    for (int i = 0; i < CsoundAudioProcessor::numIntParams; ++i)
    {
        if (processor.getIntParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<IntUnifiedRow> (processor, i, onCopied, onRemoved);
        row->setEditMode (editMode);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
    }

    for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
    {
        if (processor.getBoolParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<BoolUnifiedRow> (processor, i, onCopied, onRemoved);
        row->setEditMode (editMode);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
    }

    for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
    {
        if (processor.getChoiceParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<ChoiceUnifiedRow> (processor, i, onCopied, onRemoved);
        row->setEditMode (editMode);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
    }

    const bool empty = unifiedRows.empty() && pendingRow == nullptr;
    viewport.setVisible (! empty);
    emptyStateLabel.setVisible (empty);

    resized();
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

    g.setColour (kAccent.withAlpha (0.6f));
    g.drawLine (0.5f, 0.0f, 0.5f, (float) getHeight(), 1.2f);
}

void CsoundParameterMappingPanel::resized()
{
    auto area = getLocalBounds().reduced (4);

    auto toolbar = area.removeFromTop (toolbarHeight);

    // "+" ed "Edit" - entrambi circolari, stesso diametro (toolbarHeight,
    // ingrandito - richiesta esplicita), centrati sulla larghezza TOTALE del
    // pannello (non della sola toolbar), ravvicinati con solo
    // toolbarButtonGap tra loro invece del margine di 10px di prima.
    const int buttonSize = toolbarHeight;
    const int centreX = getWidth() / 2;

    addButton.setBounds (centreX - buttonSize - toolbarButtonGap / 2, toolbar.getY(), buttonSize, buttonSize);
    editToggleButton.setBounds (centreX + toolbarButtonGap / 2, toolbar.getY(), buttonSize, buttonSize);

    area.removeFromTop (4);

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
        const int h = iface != nullptr ? iface->getPreferredHeight (editMode, contentWidth) : rowHeight;
        row->setBounds (0, y, contentWidth, h);
        y += h + gapBetweenRows;
    }

    if (pendingRow != nullptr)
    {
        auto* iface = dynamic_cast<UnifiedRowInterface*> (pendingRow.get());
        const int h = iface != nullptr ? iface->getPreferredHeight (true, contentWidth) : rowHeight;
        pendingRow->setBounds (0, y, contentWidth, h);
        y += h + gapBetweenRows;
    }

    rowsContainer.setSize (contentWidth, y);
}
