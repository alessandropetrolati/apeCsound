#include "CsoundParameterEditor.h"
#include "CsoundLayoutHelpers.h"
#include "CsoundShortcuts.h"
#include "NativeAlertMac.h"
#include "CsoundTextPrompt.h"

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
    const juce::Colour kAccent       { 0xff4aa3b8 }; // teal chiaro della toolbar (mockup R2)
    const juce::Colour kOnAccent     { 0xff0b1116 }; // icone/testo sui bottoni teal
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
    const juce::Colour kMonitorAccent { 0xff8a9aa5 }; // grigio-azzurro neutro: il Monitor (sola lettura), nessun tipo di parametro

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

    // Validazione inline (card Edit): campo con bordo rosso e didascalia
    // rossa con il messaggio al posto del nome, invece di correggere in
    // silenzio. Con ok = true la didascalia torna al testo normale.
    void setFieldValidity (juce::TextEditor& field, juce::Label* caption, const juce::String& captionText,
                           bool ok, const juce::String& message = {})
    {
        field.setColour (juce::TextEditor::outlineColourId,        ok ? kFieldOutline : kDanger);
        field.setColour (juce::TextEditor::focusedOutlineColourId, ok ? kAccent : kDanger);
        field.repaint();

        if (caption != nullptr)
        {
            // Le didascalie sono strette: messaggio BREVE (es. "MIN > MAX"),
            // il dettaglio nel tooltip del campo.
            caption->setText (ok ? captionText : message, juce::dontSendNotification);
            caption->setColour (juce::Label::textColourId, ok ? kTextMuted : kDanger.brighter (0.4f));
        }

        if (message.isNotEmpty())
            field.setTooltip (ok ? juce::String() : message);
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
    setColour (juce::PopupMenu::highlightedTextColourId,       kOnAccent);

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
    // Bottone "..." della card (nome "rowMenu"): niente sfondo a riposo, un
    // cerchio semitrasparente solo in hover/pressione.
    if (button.getName() == "rowMenu")
    {
        if (! shouldDrawButtonAsHighlighted && ! shouldDrawButtonAsDown)
            return;

        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (kAccent.withAlpha (shouldDrawButtonAsDown ? 0.35f : 0.18f));
        g.fillEllipse (bounds);
        return;
    }

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
    const float trackThickness = 4.0f;
    auto trackBounds = juce::Rectangle<float> (bounds.getX(), centreY - trackThickness * 0.5f, bounds.getWidth(), trackThickness);

    g.setColour (slider.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (trackBounds, trackThickness * 0.5f);

    g.setColour (slider.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (trackBounds.withWidth (juce::jlimit (0.0f, bounds.getWidth(), sliderPos - bounds.getX())),
                             trackThickness * 0.5f);

    // Maniglia grande (fino a 22 px) su un track sottile; la zona di presa
    // e' tutta l'altezza dello slider (28 px). Anello chiaro quando e'
    // trascinata, cosi' sotto il dito si vede che e' "agganciata".
    const float thumbDiameter = sliderThumbDiameter (bounds.getHeight());
    const auto thumb = juce::Rectangle<float> (thumbDiameter, thumbDiameter).withCentre ({ sliderPos, centreY });

    if (slider.isMouseButtonDown())
    {
        g.setColour (slider.findColour (juce::Slider::thumbColourId).withAlpha (0.25f));
        g.fillEllipse (thumb.expanded (sliderHaloMargin));
    }

    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (thumb);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawEllipse (thumb.reduced (1.0f), 1.5f);
}

int CsoundParameterPanelLookAndFeel::getSliderThumbRadius (juce::Slider& slider)
{
    if (slider.isHorizontal())
        // Rientro = raggio dell'alone: agli estremi anche l'alone resta intero.
        return juce::roundToInt (sliderThumbDiameter ((float) slider.getHeight()) * 0.5f + sliderHaloMargin);

    return LookAndFeel_V4::getSliderThumbRadius (slider);
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
    // matita ("edit") in modalita' UI, spunta ("apply", bianca su cerchio
    // pieno colorato - vedi drawButtonBackground) in modalita' Edit.
    // Bottone "..." della card: tre puntini grigi (teal in hover).
    if (button.getName() == "rowMenu")
    {
        const auto bounds = button.getLocalBounds().toFloat();
        const auto c = bounds.getCentre();

        const float r = juce::jmax (1.6f, bounds.getHeight() * 0.065f);
        const float gap = r * 3.2f;
        g.setColour (shouldDrawButtonAsHighlighted ? kAccent : kTextMuted);
        for (int i = -1; i <= 1; ++i)
            g.fillEllipse (c.x + (float) i * gap - r, c.y - r, r * 2.0f, r * 2.0f);
        return;
    }

    if (button.getName() == "editToggle")
    {
        const bool editing = button.getToggleState();
        // Matita ("edit") a riposo; spunta ("apply", chiude l'Edit) quando la
        // card e' in Edit - al posto del vecchio occhio.
        auto bounds = button.getLocalBounds().toFloat();
        const auto c = bounds.getCentre();
        const float sz = bounds.getHeight() * 0.46f;

        if (editing)
        {
            juce::Path tick;
            tick.startNewSubPath (c.x - sz * 0.45f, c.y + sz * 0.02f);
            tick.lineTo (c.x - sz * 0.12f, c.y + sz * 0.36f);
            tick.lineTo (c.x + sz * 0.48f, c.y - sz * 0.36f);
            g.setColour (kOnAccent);
            g.strokePath (tick, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }

        // Matita inclinata a 45 gradi: corpo, punta, piccola linea di base.
        juce::Path pencil;
        const float half = sz * 0.5f;
        const float w = sz * 0.22f;
        pencil.addRoundedRectangle (-w * 0.5f, -half, w, sz * 0.72f, w * 0.3f);          // corpo
        pencil.addTriangle (-w * 0.5f, -half + sz * 0.72f, w * 0.5f, -half + sz * 0.72f, 0.0f, half); // punta
        pencil.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::pi * 0.25f).translated (c.x - sz * 0.05f, c.y - sz * 0.05f));
        pencil.addRoundedRectangle (c.x - half * 0.95f, c.y + half * 0.78f, sz * 0.95f, 1.6f, 0.8f); // riga sotto

        g.setColour (shouldDrawButtonAsHighlighted ? kAccent : kTextMuted);
        g.fillPath (pencil);
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

        // Colore dell'icona dal bottone (PluginEditor lo imposta scuro sul
        // teal chiaro della toolbar), bianco se non impostato.
        g.setColour (button.isColourSpecified (juce::TextButton::textColourOffId)
                         ? button.findColour (juce::TextButton::textColourOffId) : kOnAccent);
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

        g.setColour (kOnAccent);
        g.fillPath (p);
        return;
    }

    // Bottone snapshot (nome "snapshotsMenu", title bar a destra): macchina
    // fotografica stilizzata - corpo arrotondato, "mirino" sopra, obiettivo
    // circolare - disegnata a mano come le altre icone qui sopra.
    if (button.getName() == "snapshotsMenu")
    {
        const auto bounds = button.getLocalBounds().toFloat();
        const float w = bounds.getWidth() * 0.56f;
        const float h = w * 0.72f;
        const auto body = juce::Rectangle<float> (w, h).withCentre (bounds.getCentre().translated (0.0f, h * 0.06f));
        const float r = h * 0.18f;

        juce::Path p;
        p.addRoundedRectangle (body, r);
        // Mirino: rettangolino sopra il corpo, a sinistra del centro.
        p.addRoundedRectangle (body.getX() + w * 0.22f, body.getY() - h * 0.16f, w * 0.26f, h * 0.2f, r * 0.5f);

        g.setColour (kOnAccent);
        g.fillPath (p);

        // Obiettivo: foro nel corpo (colore del bottone) + pupilla.
        const float lens = h * 0.5f;
        auto lensArea = juce::Rectangle<float> (lens, lens).withCentre (body.getCentre().translated (0.0f, h * 0.04f));
        g.setColour (button.findColour (juce::TextButton::buttonColourId));
        g.fillEllipse (lensArea);
        g.setColour (kOnAccent);
        g.fillEllipse (lensArea.reduced (lens * 0.28f));
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

        g.setColour (kOnAccent);
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
                                                        juce::Rectangle<int>& fieldsArea,
                                                        juce::Rectangle<int>* previewArea,
                                                        juce::Rectangle<int>* secondNameArea)
{
    handleStrip = full.removeFromLeft (handleStripWidth);

    auto content = full.reduced (cardPaddingH, cardPaddingV);

    auto header = content.removeFromTop (cardHeaderHeight);
    // In alto a destra stanno Edit e "..." della card, disegnati SOPRA da
    // layoutRowIconButtons (nel wrapper *UnifiedRow, stessa geometria):
    // qui si riserva il loro spazio senza piazzarci nulla. Il vecchio
    // bottone "x" di rimozione non c'e' piu' (Remove sta nel menu "...",
    // con conferma): removeArea resta vuota.
    header.removeFromRight (rowEditIconMargin);
    const int editIconSize = juce::jmin (cardHeaderHeight, rowEditIconSize);
    header.removeFromRight (editIconSize);           // spazio di editIconButton
    header.removeFromRight (rowIconButtonGap);
    header.removeFromRight (editIconSize);           // spazio di menuButton
    removeArea = {};
    header.removeFromRight (rowIconButtonGap);
    typeLabelArea = header;

    content.removeFromTop (cardHeaderGap);
    nameArea = content.removeFromTop (cardNameHeight);
    content.removeFromTop (cardNameGap);

    if (secondNameArea != nullptr)
    {
        *secondNameArea = content.removeFromTop (cardNameHeight);
        content.removeFromTop (cardNameGap);
    }

    if (previewArea != nullptr)
    {
        *previewArea = content.removeFromTop (cardPreviewHeight);
        content.removeFromTop (cardPreviewGap);
    }

    fieldsArea = content;
}

// Scheletro compatto delle card UI - vedi il commento in testa alla
// dichiarazione in CsoundParameterEditor.h. Stessa logica di
// layoutCardSkeleton() sopra ma con la geometria uiCard* (piu' bassa) e
// senza bottone di rimozione (GenericParamRow non ne ha uno).
bool CsoundParameterMappingPanel::compactCardFits (int cardWidth, bool isChoice)
{
    // Larghezza della riga di testa senza maniglia, padding e icone.
    const int iconsWidth = rowEditIconMargin + rowEditIconSize * 2 + rowIconButtonGap + 8;
    const int head = cardWidth - handleStripWidth - cardPaddingH * 2 - iconsWidth - uiExternalDotWidth;
    const int control = (isChoice ? 90 : uiTogglePillWidth) + uiValueGap;
    return head - control >= uiCompactMinNameWidth;
}

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

    // Nome del canale nella stessa riga di testa, a sinistra dei bottoni
    // "..." + Edit (vedi layoutRowIconButtons: 2 icone + spazio + margine).
    // A sinistra del nome resta lo spazio per il puntino "mosso da fuori"
    // (uiExternalDotWidth, vedi GenericParamRow::paint).
    const int iconsWidth = rowEditIconMargin + rowEditIconSize * 2 + rowIconButtonGap + 8;
    nameArea = typeLabelArea.withTrimmedRight (iconsWidth).withTrimmedLeft (uiExternalDotWidth);

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
    button.setTitle ("Edit parameter");
}

void CsoundParameterMappingPanel::setupRowMenuButton (juce::TextButton& button)
{
    button.setName ("rowMenu");
    button.setTooltip ("Parameter menu");
    button.setTitle ("Parameter menu");
}

// Edit a destra, menu "..." subito alla sua sinistra, nell'angolo in alto a
// destra della card, in ENTRAMBE le modalita'.
void CsoundParameterMappingPanel::layoutRowIconButtons (juce::Rectangle<int> fullBounds, bool rowEditMode,
                                                          juce::Component& menuButton, juce::Component& editButton)
{
    const int padV   = rowEditMode ? cardPaddingV     : uiCardPaddingV;
    const int headerH = rowEditMode ? cardHeaderHeight : uiCardHeaderHeight;

    auto header = fullBounds.reduced (cardPaddingH, padV).removeFromTop (headerH);

    header.removeFromRight (rowEditIconMargin);
    const int iconSize = juce::jmin (headerH, rowEditIconSize);
    editButton.setBounds (header.removeFromRight (iconSize).withSizeKeepingCentre (iconSize, iconSize));
    header.removeFromRight (rowIconButtonGap);
    menuButton.setBounds (header.removeFromRight (iconSize).withSizeKeepingCentre (iconSize, iconSize));
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

    // Card in Edit (una sola alla volta): oltre allo sfondo piu' chiaro,
    // un bordo pieno nel colore del tipo e un alone - deve risultare
    // evidente a colpo d'occhio quale card si sta configurando.
    if (isEditingCard)
    {
        const auto b = bounds.toFloat();
        g.setColour (accent.withAlpha (0.18f));
        g.fillRect (b.withTrimmedLeft (5.0f));
        g.setColour (accent);
        g.drawRect (b, 2.0f);
    }

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

// Griglia dei campi (Min/Max/Init/Exp/Step): tante colonne quante ne
// entrano con la larghezza MINIMA di un campo (il massimo di fieldWidths),
// poi ogni riga si allarga a riempire TUTTA availableWidth - cosi' l'ultimo
// campo finisce esattamente al bordo destro del nome/slider sopra, invece
// di lasciare un vuoto a destra. computeWrappedFieldRows() e
// layoutWrappedFields() passano entrambi da qui: stesso conteggio di
// colonne, quindi stesso numero di righe (prima i due algoritmi contavano
// il gap in modo diverso e, per certe larghezze, l'ultimo campo finiva in
// una riga che l'altezza della card non prevedeva: spariva).
// wrappedFieldColumns: vedi CsoundLayoutHelpers.h (testato in Tests/CsoundUnitTests.cpp).

int CsoundParameterMappingPanel::computeWrappedFieldRows (int availableWidth, std::initializer_list<int> fieldWidths, int gap)
{
    const int n = (int) fieldWidths.size();
    if (n == 0) return 1;
    const int cols = csoundLayout::wrappedFieldColumns (availableWidth, fieldWidths, gap);
    return (n + cols - 1) / cols;
}

void CsoundParameterMappingPanel::layoutWrappedFields (juce::Rectangle<int> fieldsArea, std::initializer_list<int> fieldWidths, int gap,
                                                         const std::function<void (int, juce::Rectangle<int>)>& onPlaceField)
{
    const int rowHeightPx = cardFieldCaptionHeight + cardFieldCaptionGap + cardFieldBoxHeight;
    const int availableWidth = fieldsArea.getWidth();
    const int n = (int) fieldWidths.size();
    if (n == 0) return;

    const int cols = csoundLayout::wrappedFieldColumns (availableWidth, fieldWidths, gap);
    const int cellW = juce::jmax (1, (availableWidth - gap * (cols - 1)) / cols);
    const int spare = juce::jmax (0, availableWidth - (cellW * cols + gap * (cols - 1))); // resto: all'ultima colonna

    for (int index = 0; index < n; ++index)
    {
        const int row = index / cols;
        const int col = index % cols;
        const int w = cellW + (col == cols - 1 ? spare : 0);

        auto cell = juce::Rectangle<int> (fieldsArea.getX() + col * (cellW + gap),
                                            fieldsArea.getY() + row * (rowHeightPx + cardFieldRowGap),
                                            w, rowHeightPx);
        onPlaceField (index, cell);
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
    channelNameEditor.setTitle ("Channel name");
    channelNameEditor.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    setupFieldCaption (minCaption, "MIN");
    addAndMakeVisible (minCaption);
    minEditor.setInputRestrictions (0, "0123456789.,-eE");
    minEditor.setTitle ("Minimum");
    minEditor.setJustification (juce::Justification::centredLeft);
    minEditor.addListener (this);
    applyDarkFieldColours (minEditor);
    addAndMakeVisible (minEditor);

    setupFieldCaption (maxCaption, "MAX");
    addAndMakeVisible (maxCaption);
    maxEditor.setInputRestrictions (0, "0123456789.,-eE");
    maxEditor.setTitle ("Maximum");
    maxEditor.setJustification (juce::Justification::centredLeft);
    maxEditor.addListener (this);
    applyDarkFieldColours (maxEditor);
    addAndMakeVisible (maxEditor);

    setupFieldCaption (initCaption, "INIT");
    addAndMakeVisible (initCaption);
    defaultEditor.setInputRestrictions (0, "0123456789.,-eE");
    defaultEditor.setTitle ("Initial value");
    defaultEditor.setJustification (juce::Justification::centredLeft);
    defaultEditor.addListener (this);
    applyDarkFieldColours (defaultEditor);
    addAndMakeVisible (defaultEditor);

    setupFieldCaption (expCaption, "SKEW");
    addAndMakeVisible (expCaption);
    skewEditor.setInputRestrictions (0, "0123456789.,-eE");
    skewEditor.setTitle ("Skew");
    skewEditor.setJustification (juce::Justification::centredLeft);
    skewEditor.addListener (this);
    applyDarkFieldColours (skewEditor);
    addAndMakeVisible (skewEditor);

    setupFieldCaption (stepCaption, "STEP");
    addAndMakeVisible (stepCaption);
    incrementEditor.setInputRestrictions (0, "0123456789.,-eE");
    incrementEditor.setTitle ("Step");
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
    addChildComponent (removeButton); // mai visibile: Remove sta nel menu "..." (requestRemove lo usa)

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

    // Validazione inline (vedi setFieldValidity): non blocca il commit, i
    // valori restano quelli scritti, ma il campo e la didascalia lo dicono.
    const bool nameTaken = slot.channelName.isNotEmpty() && processor.isChannelNameUsedElsewhere (slot.channelName, 0, index);
    setFieldValidity (channelNameEditor, nullptr, {}, ! nameTaken);
    channelNameEditor.setTooltip (nameTaken ? "Channel name already used by another parameter" : juce::String());

    const bool rangeOk   = slot.minValue < slot.maxValue;
    const bool defaultOk = ! rangeOk || (slot.defaultValue >= slot.minValue && slot.defaultValue <= slot.maxValue);
    const bool skewOk    = slot.skew > 0.0f;
    const bool stepOk    = slot.increment >= 0.0f && (! rangeOk || slot.increment <= slot.maxValue - slot.minValue);

    setFieldValidity (minEditor,       &minCaption,  "MIN",  rangeOk,   "MIN >= MAX");
    setFieldValidity (maxEditor,       &maxCaption,  "MAX",  rangeOk,   "MAX <= MIN");
    setFieldValidity (defaultEditor,   &initCaption, "INIT", defaultOk, "OUT OF RANGE");
    setFieldValidity (skewEditor,      &expCaption,  "SKEW", skewOk,    "SKEW <= 0");
    setFieldValidity (incrementEditor, &stepCaption, "STEP", stepOk,    "STEP > RANGE");

    if (onSlotChanged)
        onSlotChanged();
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

void CsoundParameterMappingPanel::ParamRow::setNameOnly (bool only)
{
    nameOnly = only;
    for (juce::Component* c : { (juce::Component*) &typeLabel, (juce::Component*) &removeButton,
                                (juce::Component*) &minCaption, (juce::Component*) &maxCaption, (juce::Component*) &initCaption,
                                (juce::Component*) &expCaption, (juce::Component*) &stepCaption,
                                (juce::Component*) &minEditor, (juce::Component*) &maxEditor, (juce::Component*) &defaultEditor,
                                (juce::Component*) &skewEditor, (juce::Component*) &incrementEditor })
        c->setVisible (! only);
    setInterceptsMouseClicks (! only, true);
    repaint();
}

void CsoundParameterMappingPanel::ParamRow::paint (juce::Graphics& g)
{
    if (nameOnly)
        return;
    const bool hasName = processor.getChannelParamSlot (index).channelName.isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), kSliderAccent, handleBounds, hasName, handleHovered, true);
}

void CsoundParameterMappingPanel::ParamRow::resized()
{
    juce::Rectangle<int> handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea, secondNameArea;
    CsoundParameterMappingPanel::layoutCardSkeleton (getLocalBounds(), handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea,
                                                      reservePreviewArea ? &previewArea : nullptr,
                                                      nameSplit != 0 ? &secondNameArea : nullptr);

    handleBounds = handleStrip;
    typeLabel.setBounds (typeLabelArea);
    removeButton.setBounds (removeArea);

    if (nameSplit != 0)
    {
        auto row = nameSplit == 1 ? nameArea : secondNameArea;
        row.removeFromLeft (nameSplitLetterWidth); // lettera X/Y (XYPadRow::paintOverChildren)
        channelNameEditor.setBounds (row);
    }
    else
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
    channelNameEditor.setTitle ("Channel name");
    channelNameEditor.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    setupFieldCaption (minCaption, "MIN");
    addAndMakeVisible (minCaption);
    minEditor.setInputRestrictions (0, "0123456789-");
    minEditor.setTitle ("Minimum");
    minEditor.setJustification (juce::Justification::centredLeft);
    minEditor.addListener (this);
    applyDarkFieldColours (minEditor);
    addAndMakeVisible (minEditor);

    setupFieldCaption (maxCaption, "MAX");
    addAndMakeVisible (maxCaption);
    maxEditor.setInputRestrictions (0, "0123456789-");
    maxEditor.setTitle ("Maximum");
    maxEditor.setJustification (juce::Justification::centredLeft);
    maxEditor.addListener (this);
    applyDarkFieldColours (maxEditor);
    addAndMakeVisible (maxEditor);

    setupFieldCaption (initCaption, "INIT");
    addAndMakeVisible (initCaption);
    defaultEditor.setInputRestrictions (0, "0123456789-");
    defaultEditor.setTitle ("Initial value");
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
    addChildComponent (removeButton); // mai visibile: Remove sta nel menu "..." (requestRemove lo usa)

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

    const bool nameTaken = slot.channelName.isNotEmpty() && processor.isChannelNameUsedElsewhere (slot.channelName, 1, index);
    setFieldValidity (channelNameEditor, nullptr, {}, ! nameTaken);
    channelNameEditor.setTooltip (nameTaken ? "Channel name already used by another parameter" : juce::String());

    const bool rangeOk   = slot.minValue < slot.maxValue;
    const bool defaultOk = ! rangeOk || (slot.defaultValue >= slot.minValue && slot.defaultValue <= slot.maxValue);
    setFieldValidity (minEditor,     &minCaption,  "MIN",  rangeOk,   "MIN >= MAX");
    setFieldValidity (maxEditor,     &maxCaption,  "MAX",  rangeOk,   "MAX <= MIN");
    setFieldValidity (defaultEditor, &initCaption, "INIT", defaultOk, "OUT OF RANGE");

    if (onSlotChanged)
        onSlotChanged();
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
    CsoundParameterMappingPanel::layoutCardSkeleton (getLocalBounds(), handleStrip, typeLabelArea, removeArea, nameArea, fieldsArea, &previewArea);

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
    channelNameEditor.setTitle ("Channel name");
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
    defaultToggle.setTitle ("Initial state");
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
    addChildComponent (removeButton); // mai visibile: Remove sta nel menu "..." (requestRemove lo usa)

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
}

void CsoundParameterMappingPanel::BoolParamRow::commitFromFields()
{
    CsoundAudioProcessor::BoolParamSlot slot;
    slot.channelName = channelNameEditor.getText().trim();
    slot.defaultValue = defaultToggle.getToggleState();

    processor.setBoolParamSlot (index, slot);

    const bool nameTaken = slot.channelName.isNotEmpty() && processor.isChannelNameUsedElsewhere (slot.channelName, 2, index);
    setFieldValidity (channelNameEditor, nullptr, {}, ! nameTaken);
    channelNameEditor.setTooltip (nameTaken ? "Channel name already used by another parameter" : juce::String());

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
    channelNameEditor.setTitle ("Channel name");
    channelNameEditor.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    setupFieldCaption (optionsCaption, "OPTIONS (COMMA SEPARATED)");
    addAndMakeVisible (optionsCaption);
    optionsEditor.setTextToShowWhenEmpty ("item1, item2, item3...", kPlaceholder);
    optionsEditor.setTitle ("Options, comma separated");
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
    defaultIndexCombo.setColour (juce::PopupMenu::highlightedTextColourId,       kOnAccent);
    defaultIndexCombo.setTitle ("Initial option");
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
    addChildComponent (removeButton); // mai visibile: Remove sta nel menu "..." (requestRemove lo usa)

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

    const bool nameTaken = slot.channelName.isNotEmpty() && processor.isChannelNameUsedElsewhere (slot.channelName, 3, index);
    setFieldValidity (channelNameEditor, nullptr, {}, ! nameTaken);
    channelNameEditor.setTooltip (nameTaken ? "Channel name already used by another parameter" : juce::String());
    setFieldValidity (optionsEditor, &optionsCaption, "OPTIONS (COMMA SEPARATED)", ! slot.optionLabels.isEmpty(), "NO OPTIONS");

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

    // Card UI: niente etichetta del tipo (il colore basta), il nome sta
    // nella riga di testa - vedi layoutUiCardSkeleton. typeLabel resta
    // (nascosta) per non toccare il resto.
    setupTypeLabel (typeLabel, typeLabelText, accentColour);
    addChildComponent (typeLabel);

    channelNameLabel.setText (channelName, juce::dontSendNotification);
    // Accessibilita': VoiceOver legge "Cutoff, slider, 1200" invece di un
    // controllo senza nome; lo stesso per toggle e menu.
    slider.setTitle (channelName);
    toggle.setTitle (channelName);
    comboBox.setTitle (channelName);
    valueReadout.setTitle (channelName + " value");
    setTitle (channelName);
    channelNameLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
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

            slider.onValueChange = [this]
            {
                updateValueReadout();
                // Non e' l'utente (ne' mouse/dito sullo slider ne' tastiera):
                // valore arrivato dall'host, da chnset o da uno snapshot.
                if (! slider.isMouseButtonDown() && ! slider.hasKeyboardFocus (true))
                    flashExternalChange();
            };
            refreshRangeDisplay();
            break;
        }

        case Kind::toggle:
            toggle.setName ("pillToggle");
            addAndMakeVisible (toggle);
            buttonAttachment = std::make_unique<juce::ButtonParameterAttachment> (parameter, toggle, nullptr);
            // onStateChange scatta anche per hover/pressione del mouse (BUG:
            // il puntino lampeggiava passando sopra): si guarda SOLO il
            // valore del toggle, e un clic dell'utente lo cambia con il
            // mouse ancora giu'; un cambio dall'esterno arriva senza.
            // Il toggle cambia valore al RILASCIO del mouse (Button::mouseUp),
            // quando isMouseButtonDown() e' gia' false: il clic dell'utente
            // va riconosciuto con un flag acceso dal mouseDown sul toggle e
            // spento al mouseUp (i listener ricevono mouseUp DOPO che il
            // bottone ha gia' cambiato stato) - vedi mouseDown/mouseUp.
            toggle.addMouseListener (this, false);
            lastToggleState = toggle.getToggleState();
            toggle.onStateChange = [this]
            {
                const bool now = toggle.getToggleState();
                if (now == lastToggleState)
                    return;
                lastToggleState = now;
                if (! toggleClickInProgress)
                    flashExternalChange();
            };
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
            comboBox.setColour (juce::PopupMenu::highlightedTextColourId,       kOnAccent);
            comboBox.setPopupTitle (parameter.getName (64));
            addAndMakeVisible (comboBox);
            comboAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (parameter, comboBox, nullptr);
            comboBox.onChange = [this] { if (! comboBox.wasLastChangeFromUser()) flashExternalChange(); };
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
        // Int: intero. Float: fino a 3 decimali, senza zeri finali
        // (0.2 -> "0.2", 1 -> "1", 0.125 -> "0.125"); prima veniva
        // arrotondato a intero anche per gli slider Float (0.2 -> "0").
        auto format = [this] (double v)
        {
            if (isIntegerLike)
                return juce::String ((int) std::round (v));

            auto s = juce::String (v, 3);
            if (s.containsChar ('.'))
                s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
            return s == "-0" ? juce::String ("0") : s;
        };

        minLabel.setText (format (sliderToReal (slider.getMinimum())), juce::dontSendNotification);
        maxLabel.setText (format (sliderToReal (slider.getMaximum())), juce::dontSendNotification);
    }

    updateValueReadout();
}

void CsoundParameterMappingPanel::GenericParamRow::setPreviewMode (bool shouldPreview)
{
    if (previewMode == shouldPreview)
        return;

    previewMode = shouldPreview;
    // typeLabel ("SLIDER FLOAT" ecc.) resta SEMPRE nascosta: nelle card UI
    // compatte non si mostra piu' (addChildComponent nel costruttore).
    channelNameLabel.setVisible (! previewMode);
    setInterceptsMouseClicks (! previewMode, true); // la card non e' trascinabile dall'anteprima, lo slider si'
    resized();
    repaint();
}

void CsoundParameterMappingPanel::GenericParamRow::paint (juce::Graphics& g)
{
    if (previewMode)
        return; // la card Edit sotto disegna gia' sfondo e cornice

    // Stessa maniglia (con puntini di trascinamento) delle card Edit -
    // trascinabile anche qui, vedi mouseDown/mouseDrag sotto.
    const bool hasName = getChannelName && getChannelName().isNotEmpty();
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), accentColour, handleBounds, hasName, handleHovered, false);

    // Puntino "mosso da fuori" (vedi flashExternalChange): nello spazio
    // riservato A SINISTRA del nome (uiExternalDotWidth, vedi
    // layoutUiCardSkeleton), con un alone che si spegne. Spento, resta un
    // anellino appena visibile: dice che il posto e' quello.
    {
        const auto nb = channelNameLabel.getBounds();
        const float cx = (float) nb.getX() - (float) uiExternalDotWidth * 0.5f;
        const float cy = (float) nb.getCentreY();

        if (externalFlash > 0.0f)
        {
            g.setColour (accentColour.withAlpha (0.28f * externalFlash));
            g.fillEllipse (cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
            g.setColour (accentColour.withAlpha (externalFlash));
            g.fillEllipse (cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
        }
        else
        {
            g.setColour (accentColour.withAlpha (0.25f));
            g.drawEllipse (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f, 1.2f);
        }
    }
}

void CsoundParameterMappingPanel::GenericParamRow::flashExternalChange()
{
    externalFlash = 1.0f;
    if (! isTimerRunning())
        startTimerHz (30);
    repaint();
}

void CsoundParameterMappingPanel::GenericParamRow::timerCallback()
{
    externalFlash = juce::jmax (0.0f, externalFlash - 1.0f / 21.0f); // ~0.7 s a 30 Hz

    if (externalFlash <= 0.0f)
        stopTimer();

    repaint();
}

void CsoundParameterMappingPanel::GenericParamRow::mouseDown (const juce::MouseEvent& event)
{
    if (event.eventComponent == &toggle)
    {
        toggleClickInProgress = true; // vedi toggle.onStateChange
        return;
    }
    // Vedi il commento identico su ParamRow::mouseDown piu' sopra.
    draggingFromHandle = event.eventComponent == this && handleBounds.contains (event.getPosition());
}

void CsoundParameterMappingPanel::GenericParamRow::mouseUp (const juce::MouseEvent& event)
{
    if (event.eventComponent == &toggle)
        toggleClickInProgress = false;
}

void CsoundParameterMappingPanel::GenericParamRow::mouseMove (const juce::MouseEvent& event)
{
    if (event.eventComponent == this)
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

    if (previewMode)
    {
        // Anteprima nella card Edit: tutta l'area e' la riga di controllo.
        handleStrip = {};
        fieldsArea = getLocalBounds();
    }

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

            auto rangeLabelsArea = fieldsArea.removeFromBottom (uiRangeLabelHeight);
            fieldsArea.removeFromBottom (uiSliderTrackGap);

            // Box VALUE alto uiFieldHeight, centrato sulla zona dello slider
            // (non sulle etichette min/max).
            valueReadout.setBounds (valueArea.withHeight (fieldsArea.getHeight())
                                             .withSizeKeepingCentre (valueArea.getWidth(),
                                                                     juce::jmin (uiFieldHeight, fieldsArea.getHeight())));

            // Sotto lo slider, le etichette min/max - lo slider stesso resta
            // COMPATTO (non e' l'altezza totale che deve crescere): la
            // maniglia e' disegnata molto piu' grande del normale
            // indipendentemente da quanto e' sottile questo rettangolo -
            // vedi CsoundParameterPanelLookAndFeel::drawLinearSlider nel .cpp.
            minLabel.setBounds (rangeLabelsArea.removeFromLeft (rangeLabelsArea.getWidth() / 2));
            maxLabel.setBounds (rangeLabelsArea);
            slider.setBounds (fieldsArea);
            break;
        }

        // Toggle e menu: card compatta a una riga (uiCompactCardHeight) con
        // il controllo nella riga di testa, tra il nome e le icone - ma solo
        // se il nome resta leggibile (compactCardFits); altrimenti due righe
        // come lo slider, con il controllo sotto il nome.
        case Kind::toggle:
        {
            if (compactCardFits (getWidth(), false))
            {
                auto head = nameArea;
                toggle.setBounds (head.removeFromRight (uiTogglePillWidth).withSizeKeepingCentre (uiTogglePillWidth, uiFieldHeight));
                head.removeFromRight (uiValueGap);
                channelNameLabel.setBounds (head);
            }
            else
            {
                toggle.setBounds (fieldsArea.removeFromLeft (uiTogglePillWidth).withSizeKeepingCentre (uiTogglePillWidth, uiFieldHeight));
            }
            break;
        }

        case Kind::choice:
        {
            if (compactCardFits (getWidth(), true))
            {
                auto head = nameArea;
                const int comboWidth = juce::jlimit (90, 220, head.getWidth() * 55 / 100);
                comboBox.setBounds (head.removeFromRight (comboWidth).withSizeKeepingCentre (comboWidth, uiFieldHeight));
                head.removeFromRight (uiValueGap);
                channelNameLabel.setBounds (head);
            }
            else
            {
                comboBox.setBounds (fieldsArea.withSizeKeepingCentre (fieldsArea.getWidth(), uiFieldHeight));
            }
            break;
        }
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

        // Anteprima dal vivo: ad ogni commit dei campi (anche per singolo
        // carattere) lo slider rilegge range e valore.
        editRow->onSlotChanged = [this] { if (uiRow != nullptr) uiRow->refreshRangeDisplay(); };
    }

    setupRowEditIconButton (editIconButton);
    editIconButton.onClick = [this] { setEditMode (! rowEditMode); if (onEditModeChanged) onEditModeChanged(); };
    addAndMakeVisible (editIconButton); // dopo editRow/uiRow: deve restare sempre in primo piano

    setupRowMenuButton (menuButton);
    menuButton.onClick = [this] { if (onMenuRequested) onMenuRequested(); };
    addAndMakeVisible (menuButton);
    addChildComponent (codeFlash); // sopra tutto, vedi CodeFlashOverlay

    setEditMode (false);
}

void CsoundParameterMappingPanel::FloatUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (rowEditMode ? editRow->getPreviewArea() : getLocalBounds());
    layoutRowIconButtons (getLocalBounds(), rowEditMode, menuButton, editIconButton);
    codeFlash.setBounds (getLocalBounds());
}

void CsoundParameterMappingPanel::FloatUnifiedRow::setCodeHighlight (bool on)
{
    if (on) codeFlash.flash (rowEditMode ? juce::Colours::white : kSliderAccent);
    else    codeFlash.clear();
}

void CsoundParameterMappingPanel::FloatUnifiedRow::setEditMode (bool edit)
{
    rowEditMode = edit;
    editRow->setVisible (edit);
    // uiRow resta visibile anche in Edit: diventa l'anteprima dal vivo
    // sotto il campo nome (vedi cardPreviewHeight / setPreviewMode).
    if (uiRow != nullptr)
    {
        uiRow->setVisible (true);
        uiRow->setPreviewMode (edit);
    }
    // setToggleState (non solo un cambio di colore): guida sia il cerchio
    // pieno colorato di drawButtonBackground sia la scelta tra le due
    // icone diverse in drawButtonText - vedi i commenti li'.
    editIconButton.setToggleState (edit, juce::dontSendNotification);
    editIconButton.setTooltip (edit ? "Back to controls" : "Edit this parameter");
    if (uiRow != nullptr) // accessibilita': "Edit amp" / "amp menu" invece di titoli generici
    {
        editIconButton.setTitle ((edit ? "Apply " : "Edit ") + uiRow->getTitle());
        menuButton.setTitle (uiRow->getTitle() + " menu");
    }
    // Se si torna alla vista UI, rilegge SUBITO il range reale appena
    // configurato in modalita' Edit (BUG corretto: prima restava fermo al
    // valore della costruzione) - vedi GenericParamRow::refreshRangeDisplay().
    if (! edit && uiRow != nullptr)
        uiRow->refreshRangeDisplay();
    if (onEditModeToggled)
        onEditModeToggled (edit);
    resized();
}

//==============================================================================
// XYPadRow - vedi il commento nel .h.
void CsoundParameterMappingPanel::XYPadRow::Pad::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (kPanelBg);
    g.fillRoundedRectangle (b, 6.0f);

    // Griglia leggera (quarti) e mirino sul punto.
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (int i = 1; i < 4; ++i)
    {
        const float x = b.getX() + b.getWidth()  * (float) i / 4.0f;
        const float y = b.getY() + b.getHeight() * (float) i / 4.0f;
        g.drawLine (x, b.getY(), x, b.getBottom(), 1.0f);
        g.drawLine (b.getX(), y, b.getRight(), y, 1.0f);
    }

    const auto inner = b.reduced (10.0f);
    const float cx = inner.getX() + inner.getWidth() * juce::jlimit (0.0f, 1.0f, px);
    const float cy = inner.getBottom() - inner.getHeight() * juce::jlimit (0.0f, 1.0f, py);

    g.setColour (accent.withAlpha (0.35f));
    g.drawLine (cx, b.getY(), cx, b.getBottom(), 1.0f);
    g.drawLine (b.getX(), cy, b.getRight(), cy, 1.0f);

    const float r = dragging ? 11.0f : 9.0f;
    g.setColour (accent.withAlpha (0.25f));
    g.fillEllipse (cx - r - 6.0f, cy - r - 6.0f, (r + 6.0f) * 2.0f, (r + 6.0f) * 2.0f);
    g.setColour (accent);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (kOnAccent);
    g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);

    g.setColour (accent.withAlpha (hasKeyboardFocus (false) ? 1.0f : 0.6f));
    g.drawRoundedRectangle (b.reduced (0.75f), 6.0f, hasKeyboardFocus (false) ? 2.5f : 1.5f);
}

void CsoundParameterMappingPanel::XYPadRow::Pad::nudge (float dx, float dy)
{
    px = juce::jlimit (0.0f, 1.0f, px + dx);
    py = juce::jlimit (0.0f, 1.0f, py + dy);
    if (onDrag) onDrag (px, py, true, true);
    repaint();
}

// Tastiera / VoiceOver: frecce sinistra-destra = X, su-giu' = Y (passo 1%,
// 10% con Shift); Home/End portano X a 0/1; PageUp/PageDown Y a 1/0.
bool CsoundParameterMappingPanel::XYPadRow::Pad::keyPressed (const juce::KeyPress& key)
{
    const float step = key.getModifiers().isShiftDown() ? 0.1f : 0.01f;
    if (key.isKeyCode (juce::KeyPress::leftKey))  { nudge (-step, 0.0f); return true; }
    if (key.isKeyCode (juce::KeyPress::rightKey)) { nudge ( step, 0.0f); return true; }
    if (key.isKeyCode (juce::KeyPress::upKey))    { nudge (0.0f,  step); return true; }
    if (key.isKeyCode (juce::KeyPress::downKey))  { nudge (0.0f, -step); return true; }
    if (key.isKeyCode (juce::KeyPress::homeKey))     { nudge (-1.0f, 0.0f); return true; }
    if (key.isKeyCode (juce::KeyPress::endKey))      { nudge ( 1.0f, 0.0f); return true; }
    if (key.isKeyCode (juce::KeyPress::pageUpKey))   { nudge (0.0f,  1.0f); return true; }
    if (key.isKeyCode (juce::KeyPress::pageDownKey)) { nudge (0.0f, -1.0f); return true; }
    return false;
}

// Il pad e' esposto come "slider" il cui valore (VO+freccia su/giu',
// o accessibilityPerformIncrement) muove l'asse X; il testo del valore
// riporta entrambi gli assi. L'asse Y si muove con le frecce su/giu'
// (vedi Pad::keyPressed).
struct CsoundParameterMappingPanel::XYPadRow::Pad::ValueInterface final : public juce::AccessibilityValueInterface
{
    explicit ValueInterface (Pad& p) : pad (p) {}

    bool isReadOnly() const override { return false; }
    double getCurrentValue() const override { return (double) pad.px; }
    juce::String getCurrentValueAsString() const override
    {
        return pad.describeValues != nullptr ? pad.describeValues()
                                             : juce::String (pad.px, 2) + ", " + juce::String (pad.py, 2);
    }
    void setValue (double newValue) override
    {
        pad.nudge ((float) newValue - pad.px, 0.0f);
    }
    void setValueAsString (const juce::String& newValue) override { setValue (newValue.getDoubleValue()); }
    AccessibleValueRange getRange() const override { return { { 0.0, 1.0 }, 0.01 }; }

    Pad& pad;
};

std::unique_ptr<juce::AccessibilityHandler> CsoundParameterMappingPanel::XYPadRow::Pad::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (*this,
                                                         juce::AccessibilityRole::slider,
                                                         juce::AccessibilityActions(),
                                                         juce::AccessibilityHandler::Interfaces { std::make_unique<ValueInterface> (*this) });
}

void CsoundParameterMappingPanel::XYPadRow::Pad::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    const auto inner = getLocalBounds().toFloat().reduced (10.0f);
    px = juce::jlimit (0.0f, 1.0f, (e.position.x - inner.getX()) / juce::jmax (1.0f, inner.getWidth()));
    py = juce::jlimit (0.0f, 1.0f, (inner.getBottom() - e.position.y) / juce::jmax (1.0f, inner.getHeight()));
    if (onDrag) onDrag (px, py, true, false);
    repaint();
}

void CsoundParameterMappingPanel::XYPadRow::Pad::mouseDrag (const juce::MouseEvent& e)
{
    const auto inner = getLocalBounds().toFloat().reduced (10.0f);
    px = juce::jlimit (0.0f, 1.0f, (e.position.x - inner.getX()) / juce::jmax (1.0f, inner.getWidth()));
    py = juce::jlimit (0.0f, 1.0f, (inner.getBottom() - e.position.y) / juce::jmax (1.0f, inner.getHeight()));
    if (onDrag) onDrag (px, py, false, false);
    repaint();
}

void CsoundParameterMappingPanel::XYPadRow::Pad::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    if (onDrag) onDrag (px, py, false, true);
    repaint();
}

void CsoundParameterMappingPanel::XYPadRow::Pad::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onResetRequested) onResetRequested();
}

CsoundParameterMappingPanel::XYPadRow::XYPadRow (CsoundAudioProcessor& processorToEdit, int xSlot, int ySlot,
                                                  std::function<void (const juce::String&)> onCopied,
                                                  std::function<void()> onRemoved,
                                                  std::function<void()> onEditModeChangedIn)
    : processor (processorToEdit), xIndex (xSlot), yIndex (ySlot), onEditModeChanged (std::move (onEditModeChangedIn))
{
    editRowX = std::make_unique<ParamRow> (processor, xIndex);
    editRowY = std::make_unique<ParamRow> (processor, yIndex);
    const auto dot = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));
    editRowX->setTypeLabelText ("XY PAD " + dot + " X AXIS FIELDS");
    editRowY->setTypeLabelText ("XY PAD " + dot + " Y AXIS FIELDS");
    editRowX->nameSplit = 1;
    editRowY->nameSplit = 2;
    editRowX->setChannelNameAccessibleTitle ("X channel name");
    editRowY->setChannelNameAccessibleTitle ("Y channel name");
    for (auto* r : { editRowX.get(), editRowY.get() })
    {
        r->onCopiedToClipboard = onCopied;
        r->onRemoveRequested = onRemoved;
        r->onSlotChanged = [this] { updateReadouts(); };
        r->reservePreviewArea = false;
        addChildComponent (*r);
    }

    pad.accent = kSliderAccent;
    pad.setWantsKeyboardFocus (true);
    pad.setMouseClickGrabsKeyboardFocus (false);
    pad.setDescription ("Arrow keys move X (left/right) and Y (up/down), Shift for larger steps, double click resets");
    pad.describeValues = [this]
    {
        return "X " + processor.getChannelParamSlot (xIndex).channelName + " " + formatValue (xIndex, pad.px)
             + ", Y " + processor.getChannelParamSlot (yIndex).channelName + " " + formatValue (yIndex, pad.py);
    };
    addAndMakeVisible (pad);

    auto setupName = [] (juce::Label& l, const juce::String& prefix)
    {
        l.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        l.setColour (juce::Label::textColourId, kText);
        l.setMinimumHorizontalScale (1.0f);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setText (prefix, juce::dontSendNotification);
    };
    auto setupValue = [] (juce::Label& l)
    {
        l.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
        l.setColour (juce::Label::textColourId, kTextMuted);
        l.setJustificationType (juce::Justification::centredLeft);
    };
    setupName (headLabel, ""); setupName (nameLabelX, "X"); setupName (nameLabelY, "Y");
    nameLabelX.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    nameLabelY.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    nameLabelX.setColour (juce::Label::textColourId, kSliderAccent);
    nameLabelY.setColour (juce::Label::textColourId, kSliderAccent);
    setupValue (valueLabelX); setupValue (valueLabelY);
    addAndMakeVisible (headLabel);
    addAndMakeVisible (nameLabelX); addAndMakeVisible (nameLabelY);
    addAndMakeVisible (valueLabelX); addAndMakeVisible (valueLabelY);

    auto* px = processor.apvts.getParameter (CsoundAudioProcessor::getChannelParamID (xIndex));
    auto* py = processor.apvts.getParameter (CsoundAudioProcessor::getChannelParamID (yIndex));
    if (px != nullptr)
        attachX = std::make_unique<juce::ParameterAttachment> (*px, [this] (float v) { pad.px = v; updateReadouts(); pad.repaint(); });
    if (py != nullptr)
        attachY = std::make_unique<juce::ParameterAttachment> (*py, [this] (float v) { pad.py = v; updateReadouts(); pad.repaint(); });
    if (attachX) attachX->sendInitialUpdate();
    if (attachY) attachY->sendInitialUpdate();

    pad.onDrag = [this] (float x, float y, bool begin, bool end)
    {
        if (attachX == nullptr || attachY == nullptr) return;
        if (begin) { attachX->beginGesture(); attachY->beginGesture(); }
        attachX->setValueAsPartOfGesture (x);
        attachY->setValueAsPartOfGesture (y);
        if (end) { attachX->endGesture(); attachY->endGesture(); }
    };
    pad.onResetRequested = [this]
    {
        processor.resetParameterValueToDefault (0, xIndex);
        processor.resetParameterValueToDefault (0, yIndex);
    };

    setupRowEditIconButton (editIconButton);
    editIconButton.onClick = [this] { setEditMode (! rowEditMode); if (onEditModeChanged) onEditModeChanged(); };
    addAndMakeVisible (editIconButton);

    setupRowMenuButton (menuButton);
    menuButton.onClick = [this] { if (onMenuRequested) onMenuRequested(); };
    addAndMakeVisible (menuButton);
    addChildComponent (codeFlash);

    setEditMode (false);
}

juce::String CsoundParameterMappingPanel::XYPadRow::formatValue (int slot, float normalized) const
{
    const double v = CsoundAudioProcessor::denormalizeChannelParam (processor.getChannelParamSlot (slot), normalized);
    auto s = juce::String (v, 3);
    if (s.containsChar ('.'))
        s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    return s == "-0" ? juce::String ("0") : s;
}

void CsoundParameterMappingPanel::XYPadRow::updateReadouts()
{
    const auto xName = processor.getChannelParamSlot (xIndex).channelName;
    const auto yName = processor.getChannelParamSlot (yIndex).channelName;
    headLabel.setText (xName + "  " + juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + "  " + yName, juce::dontSendNotification);
    nameLabelX.setText ("X  " + xName, juce::dontSendNotification);
    nameLabelY.setText ("Y  " + yName, juce::dontSendNotification);
    valueLabelX.setText (formatValue (xIndex, pad.px), juce::dontSendNotification);
    valueLabelY.setText (formatValue (yIndex, pad.py), juce::dontSendNotification);

    // Accessibilita': il pad e i bottoni di riga portano i nomi dei canali.
    const auto padName = "XY pad " + xName + " by " + yName;
    if (pad.getTitle() != padName)
    {
        pad.setTitle (padName);
        setTitle (padName);
        valueLabelX.setTitle (xName + " value");
        valueLabelY.setTitle (yName + " value");
        if (! rowEditMode)
        {
            editIconButton.setTitle ("Edit " + padName);
            menuButton.setTitle (padName + " menu");
        }
    }
    if (auto* h = pad.getAccessibilityHandler())
        h->notifyAccessibilityEvent (juce::AccessibilityEvent::valueChanged);
}

void CsoundParameterMappingPanel::XYPadRow::paint (juce::Graphics& g)
{
    if (rowEditMode)
        return; // la ParamRow visibile disegna la sua card
    CsoundParameterMappingPanel::paintCardChrome (g, getLocalBounds(), kSliderAccent, {}, false, false, false);
}

void CsoundParameterMappingPanel::XYPadRow::resized()
{
    editRowX->setBounds (getLocalBounds());
    editRowY->setBounds (getLocalBounds());
    layoutRowIconButtons (getLocalBounds(), rowEditMode, menuButton, editIconButton);
    codeFlash.setBounds (getLocalBounds());

    juce::Rectangle<int> handleStrip, typeLabelArea, nameArea, fieldsArea;
    layoutUiCardSkeleton (getLocalBounds(), handleStrip, typeLabelArea, nameArea, fieldsArea);

    // Riga di testa: "Cutoff x Resonance"; sotto, il pad a sinistra e, a
    // destra, le due coppie "X nome / valore", "Y nome / valore".
    headLabel.setBounds (nameArea);

    auto area = fieldsArea;
    const int padW = juce::jmin (area.getWidth() - 110, area.getHeight() * 4 / 3);
    pad.setBounds (area.removeFromLeft (juce::jmax (80, padW)));
    area.removeFromLeft (uiValueGap);
    const int rowH = 18;
    auto right = area.withSizeKeepingCentre (area.getWidth(), rowH * 4 + 10);
    nameLabelX.setBounds (right.removeFromTop (rowH));
    valueLabelX.setBounds (right.removeFromTop (rowH));
    right.removeFromTop (10);
    nameLabelY.setBounds (right.removeFromTop (rowH));
    valueLabelY.setBounds (right.removeFromTop (rowH));
}

void CsoundParameterMappingPanel::XYPadRow::setEditAxis (bool yAxis)
{
    editY = yAxis;
    // Card piena = asse scelto; l'altro asse e' solo il suo campo nome,
    // sovrapposto (in primo piano, cosi' si puo' cliccare).
    editRowX->setNameOnly (editY);
    editRowY->setNameOnly (! editY);
    // Card piena sotto, nameOnly sopra; i bottoni e il lampo restano
    // sempre in primo piano (BUG corretto: il bottone apply finiva coperto).
    (editY ? editRowY : editRowX)->toBack();
    (editY ? editRowX : editRowY)->toBehind (&pad);
    editIconButton.toFront (false);
    menuButton.toFront (false);
    codeFlash.toFront (false);
    repaint();
}

void CsoundParameterMappingPanel::XYPadRow::paintOverChildren (juce::Graphics& g)
{
    if (! rowEditMode)
        return;
    // Lettere X / Y nello spazio lasciato a sinistra di ciascun campo nome
    // (vedi ParamRow::nameSplit).
    g.setColour (kSliderAccent);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    const auto bx = editRowX->channelNameEditorBounds();
    const auto by = editRowY->channelNameEditorBounds();
    g.drawText ("X", bx.withX (bx.getX() - ParamRow::nameSplitLetterWidth).withWidth (ParamRow::nameSplitLetterWidth - 4), juce::Justification::centred, false);
    g.drawText ("Y", by.withX (by.getX() - ParamRow::nameSplitLetterWidth).withWidth (ParamRow::nameSplitLetterWidth - 4), juce::Justification::centred, false);
}

void CsoundParameterMappingPanel::XYPadRow::setEditMode (bool edit)
{
    rowEditMode = edit;
    editRowX->setVisible (edit);
    editRowY->setVisible (edit);
    if (edit)
        setEditAxis (editY);
    for (juce::Component* c : { (juce::Component*) &pad, (juce::Component*) &headLabel, (juce::Component*) &nameLabelX,
                                (juce::Component*) &nameLabelY, (juce::Component*) &valueLabelX, (juce::Component*) &valueLabelY })
        c->setVisible (! edit);
    editIconButton.setToggleState (edit, juce::dontSendNotification);
    editIconButton.setTooltip (edit ? "Back to the pad" : "Edit (X fields; Y from the ... menu)");
    {
        const auto padName = pad.getTitle().isNotEmpty() ? pad.getTitle() : juce::String ("XY pad");
        editIconButton.setTitle ((edit ? "Apply " : "Edit ") + padName);
        menuButton.setTitle (padName + " menu");
    }
    if (! edit)
        updateReadouts();
    if (onEditModeToggled)
        onEditModeToggled (edit);
    resized();
    repaint();
}

void CsoundParameterMappingPanel::XYPadRow::setCodeHighlight (bool on)
{
    if (on) codeFlash.flash (rowEditMode ? juce::Colours::white : kSliderAccent);
    else    codeFlash.clear();
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

        // Anteprima dal vivo: ad ogni commit dei campi (anche per singolo
        // carattere) lo slider rilegge range e valore.
        editRow->onSlotChanged = [this] { if (uiRow != nullptr) uiRow->refreshRangeDisplay(); };
    }

    setupRowEditIconButton (editIconButton);
    editIconButton.onClick = [this] { setEditMode (! rowEditMode); if (onEditModeChanged) onEditModeChanged(); };
    addAndMakeVisible (editIconButton); // dopo editRow/uiRow: deve restare sempre in primo piano

    setupRowMenuButton (menuButton);
    menuButton.onClick = [this] { if (onMenuRequested) onMenuRequested(); };
    addAndMakeVisible (menuButton);
    addChildComponent (codeFlash); // sopra tutto, vedi CodeFlashOverlay

    setEditMode (false);
}

void CsoundParameterMappingPanel::IntUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (rowEditMode ? editRow->getPreviewArea() : getLocalBounds());
    layoutRowIconButtons (getLocalBounds(), rowEditMode, menuButton, editIconButton);
    codeFlash.setBounds (getLocalBounds());
}

void CsoundParameterMappingPanel::IntUnifiedRow::setCodeHighlight (bool on)
{
    if (on) codeFlash.flash (rowEditMode ? juce::Colours::white : kKnobAccent);
    else    codeFlash.clear();
}

void CsoundParameterMappingPanel::IntUnifiedRow::setEditMode (bool edit)
{
    rowEditMode = edit;
    editRow->setVisible (edit);
    // uiRow resta visibile anche in Edit: diventa l'anteprima dal vivo
    // sotto il campo nome (vedi cardPreviewHeight / setPreviewMode).
    if (uiRow != nullptr)
    {
        uiRow->setVisible (true);
        uiRow->setPreviewMode (edit);
    }
    // setToggleState (non solo un cambio di colore): guida sia il cerchio
    // pieno colorato di drawButtonBackground sia la scelta tra le due
    // icone diverse in drawButtonText - vedi i commenti li'.
    editIconButton.setToggleState (edit, juce::dontSendNotification);
    editIconButton.setTooltip (edit ? "Back to controls" : "Edit this parameter");
    if (uiRow != nullptr) // accessibilita': "Edit amp" / "amp menu" invece di titoli generici
    {
        editIconButton.setTitle ((edit ? "Apply " : "Edit ") + uiRow->getTitle());
        menuButton.setTitle (uiRow->getTitle() + " menu");
    }
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
    addAndMakeVisible (editIconButton); // dopo editRow/uiRow: deve restare sempre in primo piano

    setupRowMenuButton (menuButton);
    menuButton.onClick = [this] { if (onMenuRequested) onMenuRequested(); };
    addAndMakeVisible (menuButton);
    addChildComponent (codeFlash); // sopra tutto, vedi CodeFlashOverlay

    setEditMode (false);
}

void CsoundParameterMappingPanel::BoolUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (getLocalBounds());
    layoutRowIconButtons (getLocalBounds(), rowEditMode, menuButton, editIconButton);
    codeFlash.setBounds (getLocalBounds());
}

void CsoundParameterMappingPanel::BoolUnifiedRow::setCodeHighlight (bool on)
{
    if (on) codeFlash.flash (rowEditMode ? juce::Colours::white : kToggleAccent);
    else    codeFlash.clear();
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
    if (uiRow != nullptr) // accessibilita': "Edit amp" / "amp menu" invece di titoli generici
    {
        editIconButton.setTitle ((edit ? "Apply " : "Edit ") + uiRow->getTitle());
        menuButton.setTitle (uiRow->getTitle() + " menu");
    }
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
    addAndMakeVisible (editIconButton); // dopo editRow/uiRow: deve restare sempre in primo piano

    setupRowMenuButton (menuButton);
    menuButton.onClick = [this] { if (onMenuRequested) onMenuRequested(); };
    addAndMakeVisible (menuButton);
    addChildComponent (codeFlash); // sopra tutto, vedi CodeFlashOverlay

    setEditMode (false);
}

void CsoundParameterMappingPanel::ChoiceUnifiedRow::resized()
{
    editRow->setBounds (getLocalBounds());
    if (uiRow != nullptr)
        uiRow->setBounds (getLocalBounds());
    layoutRowIconButtons (getLocalBounds(), rowEditMode, menuButton, editIconButton);
    codeFlash.setBounds (getLocalBounds());
}

void CsoundParameterMappingPanel::ChoiceUnifiedRow::setCodeHighlight (bool on)
{
    if (on) codeFlash.flash (rowEditMode ? juce::Colours::white : kMenuAccent);
    else    codeFlash.clear();
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
    if (uiRow != nullptr) // accessibilita': "Edit amp" / "amp menu" invece di titoli generici
    {
        editIconButton.setTitle ((edit ? "Apply " : "Edit ") + uiRow->getTitle());
        menuButton.setTitle (uiRow->getTitle() + " menu");
    }
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
    processor.addListener (this);

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
    addButton.setTitle ("Parameters menu");
    addButton.setDescription ("Add a parameter, expand or collapse groups, reset or randomize all");
    addButton.getProperties().set ("circular", true);
    addButton.setColour (juce::TextButton::buttonColourId, kAccent);
    //addButton.setTooltip ("Add / configure parameters");
    addButton.onClick = [this] { showAddMenu(); };
    addAndMakeVisible (addButton);

    // Snapshot dei valori (destra della title bar) - vedi il commento nel .h.
    snapshotsButton.setName ("snapshotsMenu");
    snapshotsButton.setTitle ("Snapshots");
    snapshotsButton.setDescription ("Save, recall and edit snapshots of the parameter values");
    snapshotsButton.getProperties().set ("circular", true);
    snapshotsButton.setColour (juce::TextButton::buttonColourId, kAccent);
    snapshotsButton.onClick = [this] { showSnapshotsMenu(); };
    addAndMakeVisible (snapshotsButton);

    // Titolo centrale della title bar (richiesta esplicita).
    titleLabel.setText ("Parameters", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);   // a sinistra, come le altre schede
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
    menuButton.setTitle ("Main menu");
    menuButton.setColour (juce::TextButton::buttonColourId, kAccent);
    //menuButton.setTooltip ("Undo / Redo / Save / Load / Show Parameters / Show Console");
    menuButton.onClick = [this] { showPanelMenu(); };

    viewport.setViewedComponent (&rowsContainer, false);
    // Solo verticale: il contenuto ora va sempre a capo per restare entro
    // la larghezza disponibile (vedi computeWrappedFieldRows()), quindi non
    // serve piu' scorrimento orizzontale.
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    emptyStateLabel.setText (
        "Add a Parameter from the Menu.",
        juce::dontSendNotification);
    emptyStateLabel.setJustificationType (juce::Justification::centred);
    emptyStateLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    emptyStateLabel.setColour (juce::Label::textColourId, kTextMuted);
    emptyStateLabel.setMinimumHorizontalScale (1.0f);
    addChildComponent (emptyStateLabel);

    rebuildUnifiedRows();

    // Monitor (vedi .h): elenco ~1 Hz, valori 20 Hz - solo quando il
    // pannello e' visibile.
    monitorListTimer.onTick  = [this] { if (isShowing()) refreshMonitorList(); };
    monitorValueTimer.onTick = [this] { if (isShowing()) updateMonitorValues(); };
    monitorListTimer.startTimer (1000);
    monitorValueTimer.startTimerHz (20);
}

CsoundParameterMappingPanel::~CsoundParameterMappingPanel()
{
    monitorListTimer.stopTimer();
    monitorValueTimer.stopTimer();
    processor.setAudioPeakWatches ({}, 0);   // nessun pannello: niente da sorvegliare
    processor.removeListener (this);
    setLookAndFeel (nullptr);
}

void CsoundParameterMappingPanel::sessionEditedByUndoRedo (bool structureReplaced)
{
    if (structureReplaced)
    {
        refreshAllFromProcessor(); // scarta anche pendingRow
        return;
    }

    // Ricostruzione DIFFERITA (e coalescente), e MAI con un tasto del mouse
    // (o un dito) premuto: questa notifica arriva anche dal commit di un
    // campo di testo che perde il focus - es. perche' si sta cliccando la
    // spunta "fine Edit" della stessa card (TextEditor notifica la perdita
    // di focus con un messaggio asincrono, quindi tra mouseDown e mouseUp).
    // Ricostruire in quel momento distruggerebbe il bottone sotto il mouse
    // prima del rilascio: il clic andava perso e servivano DUE clic.
    if (rebuildPending)
        return;

    rebuildPending = true;
    scheduleDeferredRebuild();
}

void CsoundParameterMappingPanel::scheduleDeferredRebuild()
{
    juce::Component::SafePointer<CsoundParameterMappingPanel> safeThis (this);

    auto attempt = [safeThis]
    {
        if (safeThis == nullptr)
            return;

        if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        {
            safeThis->scheduleDeferredRebuild(); // riprova a mouse rilasciato
            return;
        }

        safeThis->rebuildPending = false;
        safeThis->rebuildUnifiedRows();
    };

    if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        juce::Timer::callAfterDelay (40, attempt);
    else
        juce::MessageManager::callAsync (attempt);
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
    {
        CsoundActionSheetItem item;
        item.id = 101; item.text = "Show Parameters"; item.shortcut = CsoundShortcuts::label (CsoundShortcuts::showParameters());
        item.enabled = onToggleParametersRequested != nullptr;
        item.tickedFn = [this] { return isParametersPanelVisible && isParametersPanelVisible(); };
        item.keepOpen = true; // il menu resta aperto (azioni multiple)
        item.icon = CsoundActionSheetIcon::sidebarPanel;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 102; item.text = "Show Console"; item.shortcut = CsoundShortcuts::label (CsoundShortcuts::showConsole());
        item.enabled = onToggleConsoleRequested != nullptr;
        item.tickedFn = [this] { return isConsoleVisible && isConsoleVisible(); };
        item.keepOpen = true;
        item.icon = CsoundActionSheetIcon::console;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 150; item.text = "Show Graphs"; item.shortcut = CsoundShortcuts::label (CsoundShortcuts::showGraphs());
        item.enabled = onToggleGraphsRequested != nullptr;
        item.tickedFn = [this] { return isGraphsVisible && isGraphsVisible(); };
        item.keepOpen = true;
        item.icon = CsoundActionSheetIcon::waveform;
        items.push_back (item);
    }

    // Load/Save CSD: richiamano le stesse funzioni dei vecchi bottoni
    // "Load..."/"Save as..." nella toolbar PRINCIPALE di PluginEditor, ora
    // rimossi (richiesta esplicita) - PluginEditor imposta queste due
    // callback nel proprio costruttore (vedi onSaveSessionRequested/
    // onLoadSessionRequested in CsoundParameterEditor.h). Separatore PRIMA
    // di "Load..." (richiesta esplicita, BUG corretto: prima era dopo),
    // a separare la sezione "View" sopra da quella file qui sotto.
    // Save / Save as... / Load... / Initialize Session: SPOSTATE nel menu
    // che si apre cliccando la capsula del file nella toolbar (richiesta
    // esplicita, vedi sessionFileLabel.onClick in PluginEditor.cpp). Il
    // codice resta qui commentato, nel caso si voglia riportarle indietro.
    // items.push_back (CsoundActionSheetItem::separator());
    // 
    // // "Save": sovrascrive il file collegato (vedi onSaveLinkedRequested nel
    // // .h) - prima di "Save as...", che invece chiede sempre il path.
    // {
    //     CsoundActionSheetItem item;
    //     item.id = 112; item.text = "Save"; item.enabled = onSaveLinkedRequested != nullptr;
    //     item.icon = CsoundActionSheetIcon::save;
    //     items.push_back (item);
    // }
    // {
    //     CsoundActionSheetItem item;
    //     item.id = 105; item.text = "Save as..."; item.enabled = onSaveSessionRequested != nullptr;
    //     item.icon = CsoundActionSheetIcon::save;
    //     items.push_back (item);
    // }

    // {
    //     CsoundActionSheetItem item;
    //     item.id = 106; item.text = "Load..."; item.enabled = onLoadSessionRequested != nullptr;
    //     item.icon = CsoundActionSheetIcon::load;
    //     items.push_back (item);
    // }
    // 
    // items.push_back (CsoundActionSheetItem::separator());
    // 
    // // "Initialize Session" (richiesta esplicita: "pulisce tutto e carica
    // // il CSD hard coded") - stesso trattamento di Save/Load CSD sopra,
    // // PluginEditor imposta onInitializeSessionRequested nel proprio
    // // costruttore (vedi promptInitializeSession()/
    // // performInitializeSession() in PluginEditor.h/.cpp, che a loro volta
    // // richiamano CsoundAudioProcessor::initializeSession()).
    // {
    //     CsoundActionSheetItem item;
    //     item.id = 111; item.text = "Initialize Session"; item.enabled = onInitializeSessionRequested != nullptr;
    //     item.icon = CsoundActionSheetIcon::newDocument;
    //     items.push_back (item);
    // }

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
        item.id = 103; item.text = "Undo"; item.shortcut = CsoundShortcuts::label (CsoundShortcuts::undo());
        item.enabledFn = [this] { return undoManager.canUndo(); };
        item.keepOpen = true;
        item.icon = CsoundActionSheetIcon::undo;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 104; item.text = "Redo"; item.shortcut = CsoundShortcuts::label (CsoundShortcuts::redo());
        item.enabledFn = [this] { return undoManager.canRedo(); };
        item.keepOpen = true;
        item.icon = CsoundActionSheetIcon::redo;
        items.push_back (item);
    }

    // Sezione help: sintassi moderna nella barra di help (spunta) e
    // manuale Csound online, poi "About" (vedi i callback nel .h).
    items.push_back (CsoundActionSheetItem::separator());
    // "Audio/MIDI Settings...": solo nella versione standalone (macOS e iOS),
    // apre l'AudioDeviceSelectorComponent di JUCE (callback nullo negli
    // altri formati -> voce assente).
    if (onAudioSettingsRequested != nullptr)
    {
        CsoundActionSheetItem item;
        item.id = 121; item.text = "Audio/MIDI Settings...";
        item.icon = CsoundActionSheetIcon::sliderFloat;
        items.push_back (item);
    }
    // "Config": sottomenu (drill-down) con le opzioni con spunta.
    {
        CsoundActionSheetItem config;
        config.text = "Config";
        config.icon = CsoundActionSheetIcon::gear;
        {
            CsoundActionSheetItem item;
            item.id = 114; item.text = "Modern Syntax in Help";
            item.enabled = onToggleModernSyntaxRequested != nullptr;
            item.tickedFn = [this] { return isModernSyntaxEnabled != nullptr && isModernSyntaxEnabled(); };
            item.keepOpen = true;
            item.icon = CsoundActionSheetIcon::codeBraces;
            config.subItems.push_back (item);
        }
        // "Follow CSD nchnls": in Config, subito sopra "Extra Folders" (vedi sotto).
        CsoundActionSheetItem followItem;
        followItem.id = 116; followItem.text = "Follow CSD nchnls";
        followItem.enabled = onToggleFollowCsdChannelsRequested != nullptr;
        followItem.tickedFn = [this] { return isFollowingCsdChannels != nullptr && isFollowingCsdChannels(); };
        followItem.keepOpen = true;
        followItem.icon = CsoundActionSheetIcon::console;

        {
            CsoundActionSheetItem item;
            item.id = 120; item.text = "Light Editor";
            item.enabled = onToggleDarkEditorRequested != nullptr;
            item.tickedFn = [this] { return isDarkEditorEnabled != nullptr && ! isDarkEditorEnabled(); };
            item.keepOpen = true;
            item.icon = CsoundActionSheetIcon::sun;
            config.subItems.push_back (item);
        }
        config.subItems.push_back (CsoundActionSheetItem::separator());
        config.subItems.push_back (followItem);

        if (onChooseCsoundFolder != nullptr)
        {
            // "Extra Folders": seconda pagina con le quattro cartelle;
            // ogni cartella apre una terza pagina (con
            // "Back"): percorso, Choose/Change Folder, Reveal, Remove.
            CsoundActionSheetItem folders;
            folders.text = "Extra Folders";
            folders.icon = CsoundActionSheetIcon::folder;


            // Le cartelle di apeCsound (SSDIR, SFDIR...) ci sono sempre: qui
            // se ne AGGIUNGONO altre in cui Csound cerca (SSDIR/SADIR/INCDIR)
            // e si sceglie dove scrive (SFDIR, al posto di quella di default).
            const char* const names[]    = { "Extra Samples (SSDIR)", "Output Files (SFDIR)",
                                             "Extra Analysis Files (SADIR)", "Extra Include Files (INCDIR)" };
            const char* const defaults[] = { "SSDIR", "SFDIR", "SADIR", "INCDIR" };

           #if JUCE_MAC
            const juce::String revealText ("Reveal in Finder");
           #elif JUCE_WINDOWS
            const juce::String revealText ("Show in Explorer");
           #else
            const juce::String revealText ("Show in Folder");
           #endif

            // Ogni cartella apre la propria pagina (con Back). Tutte le voci
            // sono keepOpen: il menu resta aperto e testi/spunte/abilitazioni
            // si rileggono a ogni ridisegno (textFn/tickedFn/enabledFn).
            for (int i = 0; i < 4; ++i)
            {
                auto isSet = [this, i] { return isCsoundFolderSet != nullptr && isCsoundFolderSet (i); };

                CsoundActionSheetItem item;
                item.text = names[i];
                item.icon = CsoundActionSheetIcon::folder;
                item.tickedFn = isSet;

                CsoundActionSheetItem pathHeader;
                pathHeader.sectionHeader = " "; // riga di intestazione; il testo vero da textFn
                const juce::String defaultName (defaults[i]);
                pathHeader.textFn = [this, i, isSet, defaultName]
                {
                    if (isSet() && getCsoundFolderPath != nullptr)
                        return getCsoundFolderPath (i);

                    return "Default: apeCsound/" + defaultName;
                };
                item.subItems.push_back (pathHeader);

                CsoundActionSheetItem choose;
                choose.id = 123 + i;  // 123..126
                choose.textFn = [isSet] { return juce::String (isSet() ? "Change Folder..." : "Choose Folder..."); };
                choose.icon = CsoundActionSheetIcon::folder;
                choose.keepOpen = true;
                item.subItems.push_back (choose);

                if (onRevealCsoundFolder != nullptr) // non su iOS
                {
                    CsoundActionSheetItem reveal;
                    reveal.id = 130 + i;  // 130..133 (cartella in uso: scelta o default)
                    reveal.text = revealText;
                    reveal.icon = CsoundActionSheetIcon::folder;
                    reveal.keepOpen = true;
                    item.subItems.push_back (reveal);
                }

                CsoundActionSheetItem remove;
                remove.id = 140 + i;  // 140..143
                remove.text = "Remove (use default)";
                remove.icon = CsoundActionSheetIcon::trash;
                remove.keepOpen = true;
                remove.enabledFn = isSet;
                item.subItems.push_back (remove);

                folders.subItems.push_back (item);
            }

            config.subItems.push_back (folders);
        }
        config.subItems.push_back (CsoundActionSheetItem::separator());
        {
            CsoundActionSheetItem item;
            item.id = 118; item.text = "Larger Text"; item.shortcut = CsoundShortcuts::label (CsoundShortcuts::largerText());
            item.enabled = onFontSizeChangeRequested != nullptr;
            item.keepOpen = true;
            item.icon = CsoundActionSheetIcon::textLarger;
            config.subItems.push_back (item);
        }
        {
            CsoundActionSheetItem item;
            item.id = 119; item.text = "Smaller Text"; item.shortcut = CsoundShortcuts::label (CsoundShortcuts::smallerText());
            item.enabled = onFontSizeChangeRequested != nullptr;
            item.keepOpen = true;
            item.icon = CsoundActionSheetIcon::textSmaller;
            config.subItems.push_back (item);
        }
       #if JUCE_DEBUG
        // Solo nelle build Debug: test unitari (Tests/CsoundUnitTests.cpp),
        // esito nella consolle. In Release restano "apeCsound --run-tests"
        // (Standalone) e scripts/run_unit_tests.sh.
        config.subItems.push_back (CsoundActionSheetItem::separator());
        {
            CsoundActionSheetItem item;
            item.id = 160; item.text = "Run Self-Tests";
            item.enabled = onRunSelfTestsRequested != nullptr;
            item.icon = CsoundActionSheetIcon::info;
            config.subItems.push_back (item);
        }
       #endif
        items.push_back (config);
    }
    
    items.push_back (CsoundActionSheetItem::separator());
    {
        CsoundActionSheetItem item;
        item.id = 115; item.text = "Csound Manual";
        item.enabled = onOpenManualRequested != nullptr;
        item.icon = CsoundActionSheetIcon::book;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 117; item.text = "apeCsound Guide";
        item.enabled = onGuideRequested != nullptr;
        item.icon = CsoundActionSheetIcon::info;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 113; item.text = "About";//About " + juce::String (ProjectInfo::projectName);
        item.enabled = onAboutRequested != nullptr;
        item.icon = CsoundActionSheetIcon::info;
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

    CsoundActionSheet::show (*this, "", std::move (items), [safeThis] (int result)
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
            case 113: if (safeThis->onAboutRequested) safeThis->onAboutRequested(); break;
            case 114: if (safeThis->onToggleModernSyntaxRequested) safeThis->onToggleModernSyntaxRequested(); break;
            case 115: if (safeThis->onOpenManualRequested) safeThis->onOpenManualRequested(); break;
            case 116: if (safeThis->onToggleFollowCsdChannelsRequested) safeThis->onToggleFollowCsdChannelsRequested(); break;
            case 118: if (safeThis->onFontSizeChangeRequested) safeThis->onFontSizeChangeRequested (+1); break;
            case 119: if (safeThis->onFontSizeChangeRequested) safeThis->onFontSizeChangeRequested (-1); break;
            case 160: if (safeThis->onRunSelfTestsRequested) safeThis->onRunSelfTestsRequested(); break;
            case 120: if (safeThis->onToggleDarkEditorRequested) safeThis->onToggleDarkEditorRequested(); break;
            case 117: if (safeThis->onGuideRequested) safeThis->onGuideRequested(); break;
            case 150: if (safeThis->onToggleGraphsRequested) safeThis->onToggleGraphsRequested(); break;
            case 121: if (safeThis->onAudioSettingsRequested) safeThis->onAudioSettingsRequested(); break;
            case 123: case 124: case 125: case 126:
                if (safeThis->onChooseCsoundFolder) safeThis->onChooseCsoundFolder (result - 123);
                break;
            case 130: case 131: case 132: case 133:
                if (safeThis->onRevealCsoundFolder) safeThis->onRevealCsoundFolder (result - 130);
                break;
            case 140: case 141: case 142: case 143:
                if (safeThis->onRemoveCsoundFolder) safeThis->onRemoveCsoundFolder (result - 140);
                break;
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
    juce::Component::SafePointer<CsoundParameterMappingPanel> safeThis (this);

    showNativeTwoButtonAlertAsync (
        "Remove all parameters?",
        "This will remove every parameter currently mapped in this plugin. "
        "You can undo this after confirming.",
        "Cancel", "Remove All",
        [safeThis] (int choice)
    {
        if (safeThis != nullptr && choice == 2)
            safeThis->performRemoveAllParameters();
    }, this);
}

void CsoundParameterMappingPanel::performRemoveAllParameters()
{
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
    rowsInEditMode.clear();

    // Niente `this` nelle lambda: vedi il commento in wirePushUndo
    // (rebuildUnifiedRows) - la ricostruzione arriva via
    // sessionEditedByUndoRedo().
    auto& proc = processor;
    undoManager.beginNewTransaction();
    undoManager.perform (new LambdaUndoableAction
    {
        [&proc]
        {
            for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
                proc.setChannelParamSlot (i, CsoundAudioProcessor::ChannelParamSlot {});
            for (int i = 0; i < CsoundAudioProcessor::numIntParams; ++i)
                proc.setIntParamSlot (i, CsoundAudioProcessor::IntParamSlot {});
            for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
                proc.setBoolParamSlot (i, CsoundAudioProcessor::BoolParamSlot {});
            for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
                proc.setChoiceParamSlot (i, CsoundAudioProcessor::ChoiceParamSlot {});

            proc.notifySessionEditedByUndoRedo (false);
        },
        [&proc, beforeFloat, beforeInt, beforeBool, beforeChoice]
        {
            for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i) proc.setChannelParamSlot (i, beforeFloat[i]);
            for (int i = 0; i < CsoundAudioProcessor::numIntParams;     ++i) proc.setIntParamSlot (i, beforeInt[i]);
            for (int i = 0; i < CsoundAudioProcessor::numBoolParams;    ++i) proc.setBoolParamSlot (i, beforeBool[i]);
            for (int i = 0; i < CsoundAudioProcessor::numChoiceParams;  ++i) proc.setChoiceParamSlot (i, beforeChoice[i]);

            proc.notifySessionEditedByUndoRedo (false);
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

    // Con i gruppi chiusi non si vedrebbe nulla: in Edit li apre.
    if (edit)
        setAllGroupsCollapsed (false);

    resized();
}

void CsoundParameterMappingPanel::setAllGroupsCollapsed (bool collapsed)
{
    for (auto& group : displayGroups)
    {
        group.header->collapsed = collapsed;
        group.header->repaint();

        if (collapsed) collapsedGroups.insert (group.name);
        else           collapsedGroups.erase (group.name);
    }

    // Anche il Monitor (menu "+" e Option+click su un'intestazione).
    if (monitorHeader != nullptr)
    {
        monitorHeader->collapsed = collapsed;
        monitorHeader->repaint();
        if (collapsed) collapsedGroups.insert (kMonitorStateKey);
        else           collapsedGroups.erase (kMonitorStateKey);
    }

    resized();
}

void CsoundParameterMappingPanel::resetAllParametersToInit()
{
    // Logica nel processor (CsoundAudioProcessor::resetParameterValuesToDefaults),
    // condivisa con il Load CSD che riporta i valori ai default. NESSUN Undo:
    // i VALORI sono gestiti dalla DAW (vedi il commento nel .h).
    if (unifiedRows.empty())
        return;

    processor.resetParameterValuesToDefaults();

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

    // Gruppi ("; @group" nel codice): solo se ce ne sono.
    if (! displayGroups.empty())
    {
        items.push_back (CsoundActionSheetItem::separator());
        {
            CsoundActionSheetItem item;
            item.id = 111; item.text = "Expand All Groups"; item.shortcut = juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\xa5")) + " click";
            item.icon = CsoundActionSheetIcon::eyeOpen;
            items.push_back (item);
        }
        {
            CsoundActionSheetItem item;
            item.id = 112; item.text = "Collapse All Groups"; item.shortcut = juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\xa5")) + " click";
            item.icon = CsoundActionSheetIcon::eyeClosed;
            items.push_back (item);
        }
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
    {
        CsoundActionSheetItem item;
        item.id = 113; item.text = "Randomize All";
        item.enabled = ! unifiedRows.empty();
        item.icon = CsoundActionSheetIcon::sliderFloat;
        items.push_back (item);
    }
    // Diagnostica dei canali (finestra modale): da qui si mappano i canali
    // letti e mai scritti - e' una funzione dei parametri.
    items.push_back (CsoundActionSheetItem::separator());
    {
        CsoundActionSheetItem item;
        item.id = 114; item.text = "Channels...";
        item.shortcut = CsoundShortcuts::label (CsoundShortcuts::showChannels());
        item.enabled = onToggleChannelsRequested != nullptr;
        item.icon = CsoundActionSheetIcon::info;
        items.push_back (item);
    }

    // SafePointer: vedi il commento identico su showPanelMenu() sopra.
    juce::Component::SafePointer<CsoundParameterMappingPanel> safeThis (this);

    CsoundActionSheet::show (*this, "", std::move (items), [safeThis] (int result)
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
            case 110: safeThis->resetAllParametersToInit(); break;
            case 113:
                safeThis->processor.randomizeAllParameterValues();
                if (safeThis->onParameterCopiedToClipboard)
                    safeThis->onParameterCopiedToClipboard ("--- Randomized all parameters ---");
                break;
            case 111: safeThis->setAllGroupsCollapsed (false); break;
            case 112: safeThis->setAllGroupsCollapsed (true); break;
            case 114: if (safeThis->onToggleChannelsRequested) safeThis->onToggleChannelsRequested(); break;
            default: break;
        }
    });
}

//==============================================================================
// Snapshot dei valori dei parametri - vedi il commento su snapshotsButton nel .h.
std::vector<CsoundActionSheetItem> CsoundParameterMappingPanel::buildSnapshotItems()
{
    const int count = processor.getNumParameterSnapshots();
    const bool haveParams = ! unifiedRows.empty();
    juce::Component::SafePointer<CsoundParameterMappingPanel> safeSelf (this);

    // ID: 1 Save, 100+i richiama, 1000+i*4+{1,2,3} = Overwrite / Rename /
    // Delete dello snapshot i (pagina aperta dal bottone matita a destra
    // della riga; ogni id e' unico in tutto il foglio).
    std::vector<CsoundActionSheetItem> items;
    {
        CsoundActionSheetItem item;
        item.id = 1; item.text = "Save Snapshot...";
        item.icon = CsoundActionSheetIcon::save;
        item.enabled = haveParams;
        item.keepOpen = true;   // il dialogo del nome si apre SOPRA il foglio, che resta
        items.push_back (item);
    }

    items.push_back (CsoundActionSheetItem::separator());

    if (count == 0)
    {
        CsoundActionSheetItem none;
        none.id = 0; none.text = "No snapshots yet"; none.enabled = false;
        items.push_back (none);
        return items;
    }

    items.push_back (CsoundActionSheetItem::section ("Tap to recall"));

    for (int i = 0; i < count; ++i)
    {
        // Nomi dinamici (textFn): dopo un Rename a foglio aperto titolo,
        // intestazione e voci si aggiornano da soli.
        auto nameFn = [safeSelf, i] { return safeSelf != nullptr ? safeSelf->processor.getParameterSnapshotName (i) : juce::String(); };

        // Riga: tap = richiama (foglio aperto, spunta sull'ultimo richiamato);
        // bottone matita a destra (o tasto ->) = pagina Overwrite/Rename/Delete.
        CsoundActionSheetItem item;
        item.id = 100 + i;
        item.text = processor.getParameterSnapshotName (i);
        item.textFn = nameFn;
        item.icon = CsoundActionSheetIcon::sliderFloat;
        item.keepOpen = true;
        item.disclosureIcon = CsoundActionSheetIcon::pencil;
        item.tickedFn = [safeSelf, i] { return safeSelf != nullptr && safeSelf->lastRecalledSnapshot == i; };

        CsoundActionSheetItem header;
        header.sectionHeader = item.text;
        header.textFn = nameFn;
        item.subItems.push_back (header);

        CsoundActionSheetItem overwrite;
        overwrite.id = 1000 + i * 4 + 1; overwrite.text = "Overwrite with Current Values";
        overwrite.icon = CsoundActionSheetIcon::save;
        overwrite.enabled = haveParams;
        overwrite.keepOpen = true;   // esegue e torna alla lista (foglio aperto)
        item.subItems.push_back (overwrite);

        CsoundActionSheetItem rename;
        rename.id = 1000 + i * 4 + 2; rename.text = "Rename...";
        rename.icon = CsoundActionSheetIcon::pencil;
        rename.keepOpen = true;      // il dialogo si apre sopra il foglio, che resta sulla pagina
        item.subItems.push_back (rename);

        item.subItems.push_back (CsoundActionSheetItem::separator());

        CsoundActionSheetItem remove;
        remove.id = 1000 + i * 4 + 3; remove.text = "Delete";
        remove.icon = CsoundActionSheetIcon::trash;
        remove.keepOpen = true;      // la lista si ricostruisce senza la voce
        item.subItems.push_back (remove);

        items.push_back (item);
    }

    return items;
}

void CsoundParameterMappingPanel::refreshOpenSnapshotsMenu()
{
    CsoundActionSheet::replaceRootItems (*this, buildSnapshotItems());   // stesso genitore di show()
}

void CsoundParameterMappingPanel::showSnapshotsMenu()
{
    juce::Component::SafePointer<CsoundParameterMappingPanel> safeThis (this);

    CsoundActionSheet::show (*this, "Snapshots", buildSnapshotItems(), [safeThis] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;

        auto& self = *safeThis;

        if (result == 1)
        {
            self.promptSaveSnapshot();
            return;
        }

        if (result >= 100 && result < 1000)
        {
            const int index = result - 100;

            if (self.processor.recallParameterSnapshot (index))
            {
                self.lastRecalledSnapshot = index;

                if (self.onParameterCopiedToClipboard)
                    self.onParameterCopiedToClipboard ("--- Snapshot recalled: "
                                                       + self.processor.getParameterSnapshotName (index) + " ---");
            }
            return;
        }

        if (result >= 1000)
        {
            const int index  = (result - 1000) / 4;
            const int action = (result - 1000) % 4;
            const auto name  = self.processor.getParameterSnapshotName (index);

            if (action == 1)
            {
                const auto before = self.processor.getParameterSnapshotsTree();
                self.processor.overwriteParameterSnapshot (index);
                self.applySnapshotsChangeUndoable (before, self.processor.getParameterSnapshotsTree(),
                                                   "--- Snapshot overwritten: " + name + " ---");
                self.lastRecalledSnapshot = index;
                self.refreshOpenSnapshotsMenu();   // chiude la pagina dello snapshot, resta la lista
            }
            else if (action == 2)
            {
                self.promptRenameSnapshot (index);
            }
            else if (action == 3)
            {
                const auto before = self.processor.getParameterSnapshotsTree();
                self.processor.removeParameterSnapshot (index);
                self.applySnapshotsChangeUndoable (before, self.processor.getParameterSnapshotsTree(),
                                                   "--- Snapshot deleted: " + name + " ---");
                if (self.lastRecalledSnapshot == index)      self.lastRecalledSnapshot = -1;
                else if (self.lastRecalledSnapshot > index)  --self.lastRecalledSnapshot;
                self.refreshOpenSnapshotsMenu();   // lista ricostruita senza la voce, foglio aperto
            }
        }
    });
}

void CsoundParameterMappingPanel::promptSaveSnapshot()
{
    juce::Component::SafePointer<CsoundParameterMappingPanel> safeThis (this);

    // Sull'intera finestra del plugin (come i menu), non solo sul pannello.
    auto* host = getTopLevelComponent();
    if (host == nullptr)
        host = this;

    CsoundTextPrompt::show (*host, "Save Snapshot", processor.makeUniqueSnapshotName(), "Save",
        [safeThis] (const juce::String& name)
        {
            if (safeThis == nullptr || name.isEmpty())
                return;

            auto& self = *safeThis;
            const auto before = self.processor.getParameterSnapshotsTree();
            const int index = self.processor.addParameterSnapshot (name);
            self.applySnapshotsChangeUndoable (before, self.processor.getParameterSnapshotsTree(),
                                               "--- Snapshot saved: " + name + " ---");
            self.lastRecalledSnapshot = index;
            self.refreshOpenSnapshotsMenu();   // la voce nuova compare nel foglio ancora aperto
        });
}

void CsoundParameterMappingPanel::promptRenameSnapshot (int index)
{
    juce::Component::SafePointer<CsoundParameterMappingPanel> safeThis (this);

    auto* host = getTopLevelComponent();
    if (host == nullptr)
        host = this;

    const auto oldName = processor.getParameterSnapshotName (index);

    CsoundTextPrompt::show (*host, "Rename Snapshot", oldName, "Rename",
        [safeThis, index, oldName] (const juce::String& name)
        {
            if (safeThis == nullptr || name.isEmpty() || name == oldName)
                return;

            auto& self = *safeThis;
            const auto before = self.processor.getParameterSnapshotsTree();
            self.processor.renameParameterSnapshot (index, name);
            self.applySnapshotsChangeUndoable (before, self.processor.getParameterSnapshotsTree(),
                                               "--- Snapshot renamed: " + oldName + " -> " + name + " ---");
            self.refreshOpenSnapshotsMenu();   // torna alla lista principale, col nome nuovo
        });
}

void CsoundParameterMappingPanel::applySnapshotsChangeUndoable (const juce::ValueTree& before, const juce::ValueTree& after,
                                                                const juce::String& logLine)
{
    // La modifica e' GIA' applicata al processor dal chiamante: la prima
    // perform() deve essere un no-op (stesso schema di
    // CodeEditTransactionProxy in PluginEditor.cpp). Undo/Redo rimettono
    // l'albero intero (prima/dopo) e avvisano pannello ed editor
    // (indicatore "•"), come le altre azioni di struttura del pannello.
    // Niente `this` nelle lambda: l'azione vive nel processor e sopravvive
    // alla chiusura dell'editor.
    auto& proc = processor;
    auto first = std::make_shared<bool> (true);

    undoManager.beginNewTransaction();
    undoManager.perform (new LambdaUndoableAction
    {
        [&proc, after, first]
        {
            if (*first) { *first = false; return; }
            proc.setParameterSnapshotsTree (after);
            proc.notifySessionEditedByUndoRedo (false);
        },
        [&proc, before]
        {
            proc.setParameterSnapshotsTree (before);
            proc.notifySessionEditedByUndoRedo (false);
        }
    });

    // Stato "•" subito (la perform() no-op non ha notificato nessuno).
    proc.notifySessionEditedByUndoRedo (false);

    if (onParameterCopiedToClipboard)
        onParameterCopiedToClipboard (logLine);
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

    // Una sola card in Edit alla volta: la riga nuova e' in Edit, le altre
    // tornano ai controlli.
    setAllRowsEditMode (false);

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
    unifiedRowKeys.clear();
    displayGroups.clear();

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
            // Le lambda NON catturano `this` (il pannello): l'azione vive
            // nella cronologia del PROCESSOR e sopravvive alla chiusura
            // dell'editor - dopo l'esecuzione notificano il processor, e il
            // pannello aperto (questo o uno nuovo) si ricostruisce in
            // sessionEditedByUndoRedo().
            auto& proc = processor;
            undoManager.beginNewTransaction();
            undoManager.perform (new LambdaUndoableAction
            {
                [&proc, doIt]   { doIt();   proc.notifySessionEditedByUndoRedo (false); },
                [&proc, undoIt] { undoIt(); proc.notifySessionEditedByUndoRedo (false); }
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
            {
                // UNA sola card in Edit alla volta: aprirne una chiude le
                // altre (le loro onEditModeToggled(false) le tolgono dal
                // tracking). Poi la card aperta viene tenuta in vista.
                for (size_t r = 0; r < unifiedRows.size(); ++r)
                    if (unifiedRowKeys[r] != key && rowsInEditMode.count (unifiedRowKeys[r]) > 0)
                        if (auto* other = dynamic_cast<UnifiedRowInterface*> (unifiedRows[r].get()))
                            other->setEditMode (false);

                rowsInEditMode.insert (key);

                juce::MessageManager::callAsync ([this, key]
                {
                    for (size_t r = 0; r < unifiedRows.size(); ++r)
                        if (unifiedRowKeys[r] == key)
                            scrollRowIntoView (*unifiedRows[r]);
                });
            }
            else
            {
                rowsInEditMode.erase (key);
            }
        };

        if (rowsInEditMode.count ({ kind, idx }) > 0)
            iface.setEditMode (true);
    };

    // Menu "..." di ogni card (vedi showRowMenu): Edit apre la riga, Remove
    // passa dal bottone "x" della sua vista Edit (requestRemove). SafePointer:
    // la riga puo' sparire (rebuild) prima che il menu si chiuda.
    auto wireRowMenu = [this] (auto& row, int kind, int idx)
    {
        juce::Component::SafePointer<std::remove_reference_t<decltype (row)>> safeRow (&row);
        row.onMenuRequested = [this, safeRow, kind, idx]
        {
            showRowMenu (kind, idx, [safeRow] { if (safeRow != nullptr) safeRow->editRow->requestRemove(); });
        };

    };

    // XY pad ("; @xy "X" "Y"", vedi CsoundParameterGroups.h): coppie di
    // slot Float che diventano UNA card. Decise dal testo COMPILATO, come
    // i gruppi. padOf[slot] = indice dell'altro asse (-1 = slider normale).
    std::vector<int> padPartner ((size_t) CsoundAudioProcessor::numChannelParams, -1);
    std::vector<bool> padIsY ((size_t) CsoundAudioProcessor::numChannelParams, false);
    {
        // Testo INTERO: la sezione <Cabbage> sta fuori (prima) del blocco
        // <CsoundSynthesizer>, quindi non e' in getCsdText().
        const auto parsed = CsoundParameterGroups::parse (processor.getCsdTextWithSurroundings());
        auto floatSlotOf = [this] (const juce::String& name)
        {
            if (name.isEmpty()) return -1;
            for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
                if (processor.getChannelParamSlot (i).channelName == name)
                    return i;
            return -1;
        };
        for (const auto& pad : parsed.xyPads)
        {
            const int x = floatSlotOf (pad.x), y = floatSlotOf (pad.y);
            if (x < 0 || y < 0 || x == y || padPartner[(size_t) x] >= 0 || padPartner[(size_t) y] >= 0)
                continue; // canale mancante / non Float / gia' in un altro pad
            padPartner[(size_t) x] = y; padPartner[(size_t) y] = x; padIsY[(size_t) y] = true;
        }
    }

    for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
    {
        if (processor.getChannelParamSlot (i).channelName.isEmpty())
            continue;

        if (padPartner[(size_t) i] >= 0)
        {
            if (padIsY[(size_t) i])
                continue; // la card e' quella dell'asse X

            const int y = padPartner[(size_t) i];
            auto row = std::make_unique<XYPadRow> (processor, i, y, onCopied, onRemoved, onEditModeChanged);
            wirePushUndo (row->editRowX->pushUndo);
            wirePushUndo (row->editRowY->pushUndo);
            row->setEditAxis (padsEditingY.count (i) > 0);
            wireEditModeTracking (*row, row->onEditModeToggled, 0, i);
            {
                juce::Component::SafePointer<XYPadRow> safeRow (row.get());
                row->onMenuRequested = [this, safeRow] { if (safeRow != nullptr) showPadMenu (*safeRow); };
            }
            rowsContainer.addAndMakeVisible (*row);
            unifiedRows.push_back (std::move (row));
            unifiedRowKeys.emplace_back (0, i);
            continue;
        }

        auto row = std::make_unique<FloatUnifiedRow> (processor, i, onCopied, onRemoved, onEditModeChanged);
        wirePushUndo (row->editRow->pushUndo);
        wireEditModeTracking (*row, row->onEditModeToggled, 0, i);
        wireRowMenu (*row, 0, i);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
        unifiedRowKeys.emplace_back (0, i);
    }

    for (int i = 0; i < CsoundAudioProcessor::numIntParams; ++i)
    {
        if (processor.getIntParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<IntUnifiedRow> (processor, i, onCopied, onRemoved, onEditModeChanged);
        wirePushUndo (row->editRow->pushUndo);
        wireEditModeTracking (*row, row->onEditModeToggled, 1, i);
        wireRowMenu (*row, 1, i);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
        unifiedRowKeys.emplace_back (1, i);
    }

    for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
    {
        if (processor.getBoolParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<BoolUnifiedRow> (processor, i, onCopied, onRemoved, onEditModeChanged);
        wirePushUndo (row->editRow->pushUndo);
        wireEditModeTracking (*row, row->onEditModeToggled, 2, i);
        wireRowMenu (*row, 2, i);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
        unifiedRowKeys.emplace_back (2, i);
    }

    for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
    {
        if (processor.getChoiceParamSlot (i).channelName.isEmpty())
            continue;

        auto row = std::make_unique<ChoiceUnifiedRow> (processor, i, onCopied, onRemoved, onEditModeChanged);
        wirePushUndo (row->editRow->pushUndo);
        wireEditModeTracking (*row, row->onEditModeToggled, 3, i);
        wireRowMenu (*row, 3, i);
        rowsContainer.addAndMakeVisible (*row);
        unifiedRows.push_back (std::move (row));
        unifiedRowKeys.emplace_back (3, i);
    }

    const bool empty = unifiedRows.empty() && pendingRow == nullptr && monitorRows.empty();
    viewport.setVisible (! empty);
    emptyStateLabel.setVisible (empty);

    rebuildDisplayGroups();
    resized();

    // Ogni modifica alla STRUTTURA (aggiunta/rimozione/metadata, undo/redo
    // compresi, e i refresh dopo Load/ripristino) passa da qui: e' il punto
    // unico da cui PluginEditor puo' ricontrollare se la sessione differisce
    // dal file su disco - vedi onMappingChanged nel .h.
    if (onMappingChanged)
        onMappingChanged();
}

//==============================================================================
// Gruppi "; @group Nome" - vedi CsoundParameterGroups.h.
void CsoundParameterMappingPanel::GroupHeader::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float radius = 10.0f;

    // Cornice: sfondo nettamente diverso dal pannello (tinta dell'accento)
    // e bordo nel colore dell'accento, cosi' il gruppo si vede subito come
    // un blocco. Chiuso: e' alta quanto la sola intestazione.
    // Monitor (special): grigio neutro invece del teal dei gruppi di
    // parametri - NON un colore dei tipi (blu/viola/verde/arancione), che
    // farebbe pensare a un tipo di parametro.
    const auto accent = special ? kMonitorAccent : kAccent;

    g.setColour (accent.withAlpha (0.14f));
    g.fillRoundedRectangle (b, radius);
    g.setColour (accent.withAlpha (0.7f));
    g.drawRoundedRectangle (b.reduced (0.75f), radius, 1.5f);

    // Intestazione: fascia PIENA nel colore dell'accento, testo scuro -
    // come i bottoni teal della toolbar.
    auto head = b.withHeight ((float) groupHeaderHeight);
    {
        juce::Path p;
        p.addRoundedRectangle (head.getX(), head.getY(), head.getWidth(), head.getHeight(),
                               radius, radius, true, true, collapsed, collapsed);
        g.setColour (accent);
        g.fillPath (p);
    }

    // Triangolo: a destra se chiuso, in giu' se aperto.
    juce::Path tri;
    const float cx = 18.0f, cy = head.getCentreY(), r = 5.0f;
    if (collapsed)
        tri.addTriangle (cx - r * 0.6f, cy - r, cx - r * 0.6f, cy + r, cx + r * 0.8f, cy);
    else
        tri.addTriangle (cx - r, cy - r * 0.6f, cx + r, cy - r * 0.6f, cx, cy + r * 0.8f);
    g.setColour (kOnAccent);
    g.fillPath (tri);

    auto headInt = head.toNearestInt();
    g.setColour (kOnAccent);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    const int nameWidth = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), name)) + 4;
    auto area = headInt.withTrimmedLeft (32).withTrimmedRight (12);
    g.drawFittedText (name, area.removeFromLeft (juce::jmin (nameWidth, area.getWidth() - 36)), juce::Justification::centredLeft, 1);

    // Numero di parametri, a destra, in una pillola scura sull'accento -
    // SOLO a gruppo chiuso: aperto, i parametri si vedono.
    if (collapsed)
    {
        const auto label = juce::String (count);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        const int w = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label)) + 14;
        auto pill = headInt.removeFromRight (w + 10).withSizeKeepingCentre (w, 18);
        g.setColour (kOnAccent.withAlpha (0.75f));
        g.fillRoundedRectangle (pill.toFloat(), 9.0f);
        g.setColour (kText);
        g.drawFittedText (label, pill, juce::Justification::centred, 1);
    }
}

void CsoundParameterMappingPanel::rebuildDisplayGroups()
{
    displayGroups.clear();

    // Testo COMPILATO (Apply/Load), non la bozza dell'editor: i gruppi non
    // saltano mentre si scrive.
    const auto parsed = CsoundParameterGroups::parse (processor.getCsdTextWithSurroundings());

    // Canali scritti (chnset) sotto un @group: le loro card del Monitor
    // vanno in quel gruppo, mescolate ai parametri nell'ordine del codice.
    monitorPlacement.clear();
    for (const auto& [channel, placement] : parsed.written)
        if (placement.group >= 0 && placement.group < parsed.groupNames.size())
            monitorPlacement[channel] = { parsed.groupNames[placement.group], placement.order };

    if (! parsed.hasGroups() || (unifiedRows.empty() && monitorPlacement.empty()))
    {
        for (auto& row : unifiedRows)
            row->setVisible (true);
        monitorPlacement.clear();
        assignMonitorGroups();
        return;
    }

    auto channelOf = [this] (int kind, int idx) -> juce::String
    {
        switch (kind)
        {
            case 0:  return processor.getChannelParamSlot (idx).channelName;
            case 1:  return processor.getIntParamSlot (idx).channelName;
            case 2:  return processor.getBoolParamSlot (idx).channelName;
            default: return processor.getChoiceParamSlot (idx).channelName;
        }
    };

    // Un gruppo per nome, nell'ordine del codice, + "Other" in coda.
    const int numNamed = parsed.groupNames.size();
    std::vector<std::vector<std::pair<int, juce::Component*>>> buckets ((size_t) numNamed + 1); // (ordine, riga)

    for (size_t r = 0; r < unifiedRows.size(); ++r)
    {
        const auto key = unifiedRowKeys[r];
        const auto it = parsed.channels.find (channelOf (key.first, key.second));

        if (it != parsed.channels.end() && it->second.group >= 0)
            buckets[(size_t) it->second.group].emplace_back (it->second.order, unifiedRows[r].get());
        else
            buckets[(size_t) numNamed].emplace_back ((int) r, unifiedRows[r].get()); // ordine del pannello
    }

    // Gruppi che contengono solo card del Monitor (nessun parametro): esistono
    // comunque (si mostrano quando il motore scrive quei canali).
    std::vector<bool> hasMonitor ((size_t) numNamed + 1, false);
    for (const auto& [channel, placement] : parsed.written)
        if (placement.group >= 0 && placement.group < numNamed)
            hasMonitor[(size_t) placement.group] = true;

    // Default all'arrivo di un insieme di gruppi NUOVO (file caricato,
    // @group cambiati): pochi gruppi -> tutti aperti; molti -> aperto solo
    // il primo, cosi' la lista resta un'overview. Poi comanda l'utente.
    {
        juce::StringArray presentNames;
        for (int gi = 0; gi <= numNamed; ++gi)
            if (! buckets[(size_t) gi].empty() || hasMonitor[(size_t) gi])
                presentNames.add (gi < numNamed ? parsed.groupNames[gi] : juce::String ("Other"));

        const auto signature = presentNames.joinIntoString ("\n");
        if (signature != lastGroupSignature)
        {
            lastGroupSignature = signature;
            collapsedGroups.clear();
            if (presentNames.size() > kMaxGroupsOpenByDefault)
                for (int i = 1; i < presentNames.size(); ++i)
                    collapsedGroups.insert (presentNames[i]);
        }
    }

    for (int gi = 0; gi <= numNamed; ++gi)
    {
        auto& bucket = buckets[(size_t) gi];
        if (bucket.empty() && ! hasMonitor[(size_t) gi])
            continue;

        std::sort (bucket.begin(), bucket.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

        DisplayGroup group;
        group.name = gi < numNamed ? parsed.groupNames[gi] : juce::String ("Other");
        for (auto& [order, row] : bucket)
        {
            group.rows.push_back (row);
            group.rowOrders.push_back (order);
        }

        group.header = std::make_unique<GroupHeader>();
        group.header->name = group.name;
        group.header->count = (int) group.rows.size();
        group.header->collapsed = collapsedGroups.count (group.name) > 0;

        auto* header = group.header.get();
        const auto name = group.name;
        group.header->onToggle = [this, header, name]
        {
            header->collapsed = ! header->collapsed;
            if (header->collapsed) collapsedGroups.insert (name);
            else                   collapsedGroups.erase (name);
            header->repaint();
            resized();
        };
        group.header->onToggleAll = [this] (bool collapseAll) { setAllGroupsCollapsed (collapseAll); };

        rowsContainer.addAndMakeVisible (*group.header);
        group.header->toBack(); // dietro alle card del gruppo
        displayGroups.push_back (std::move (group));
    }

    assignMonitorGroups();
}

void CsoundParameterMappingPanel::assignMonitorGroups()
{
    // Una card va nel gruppo del suo primo canale, se quel gruppo esiste nel
    // pannello; altrimenti nel gruppo Monitor in fondo.
    for (auto& row : monitorRows)
    {
        row->groupName = {};
        row->order = 0;
        if (row->entries.empty())
            continue;
        const auto it = monitorPlacement.find (row->entries.front().name);
        if (it == monitorPlacement.end())
            continue;
        for (const auto& group : displayGroups)
            if (group.name == it->second.first)
            {
                row->groupName = it->second.first;
                row->order = it->second.second;
                break;
            }
    }
}

void CsoundParameterMappingPanel::refreshGrouping()
{
    // Anche i pad XY dipendono dal testo compilato: ricostruzione completa.
    rebuildUnifiedRows();
}

void CsoundParameterMappingPanel::setHighlightedChannel (const juce::String& channelName)
{
    // Stesso canale di prima (caret che si muove sulla stessa riga): niente
    // nuovo lampo. Canale diverso o nuovo: lampo sulla card.
    if (highlightedChannel == channelName)
        return;
    highlightedChannel = channelName;

    for (size_t i = 0; i < unifiedRows.size() && i < unifiedRowKeys.size(); ++i)
    {
        const auto [kind, slot] = unifiedRowKeys[i];
        juce::String name;
        switch (kind)
        {
            case 0:  name = processor.getChannelParamSlot (slot).channelName; break;
            case 1:  name = processor.getIntParamSlot (slot).channelName;     break;
            case 2:  name = processor.getBoolParamSlot (slot).channelName;    break;
            default: name = processor.getChoiceParamSlot (slot).channelName;  break;
        }

        bool on = channelName.isNotEmpty() && name == channelName;
        if (! on && channelName.isNotEmpty())
            if (auto* padRow = dynamic_cast<XYPadRow*> (unifiedRows[i].get()))
                on = processor.getChannelParamSlot (padRow->yIndex).channelName == channelName;
        if (auto* iface = dynamic_cast<UnifiedRowInterface*> (unifiedRows[i].get()))
            iface->setCodeHighlight (on);
        if (on && unifiedRows[i]->isVisible())
            scrollRowIntoView (*unifiedRows[i]);
    }
}

//==============================================================================
// Monitor - vedi il commento nel .h.
//==============================================================================
// Card del Monitor - un aspetto per tipo (vedi csoundMonitor::Kind):
//
//   range     barra con tacche ogni quarto e min/max sotto (range da chn_k)
//   adaptive  barra tratteggiata: il range e' quello visto finora (min/max
//             in corsivo) - "non dichiarato"
//   bipolar   barra dallo zero, tacca centrale marcata, lati in due toni
//   audio     meter a segmenti verde/giallo/rosso in dB, scala -48..0,
//             picco tenuto
//   spark     curva con area piena e punto sul valore corrente
//   text      display "LCD": riquadro incassato, testo monospaziato
//   lamp      spia tonda con alone
//
// Ogni card ha a sinistra del nome un glifo del suo tipo.
namespace
{
    const juce::Colour kMeterGreen  { 0xff4cc46a };
    const juce::Colour kMeterYellow { 0xffe6c44a };
    const juce::Colour kMeterRed    { 0xffe0503c };
    const juce::Colour kLcdBg       { 0xff0b1216 };
    const juce::Colour kStuck       { 0xffe0b84a };   // "no change since Apply" (come gli orfani di Channels)
    const juce::Colour kNegative    { 0xff6f8fb8 };   // lato negativo delle barre bipolari

    // Glifo 12x12 del tipo, disegnato nel riquadro r.
    void drawKindGlyph (juce::Graphics& g, juce::Rectangle<float> r, csoundMonitor::Kind k, bool multi)
    {
        using K = csoundMonitor::Kind;
        g.setColour (kTextMuted);
        const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        juce::Path p;

        if (multi)
        {
            for (int i = 0; i < 3; ++i)
            {
                const float bh = h * (0.4f + 0.3f * (float) ((i * 2) % 3));
                g.fillRect (x + (float) i * w * 0.36f, y + h - bh, w * 0.24f, bh);
            }
            return;
        }

        switch (k)
        {
            case K::audio:
                // tre segmenti di meter crescenti: verde, giallo, rosso
                g.setColour (kMeterGreen);  g.fillRect (x,             y + h * 0.55f, w * 0.28f, h * 0.45f);
                g.setColour (kMeterYellow); g.fillRect (x + w * 0.36f, y + h * 0.30f, w * 0.28f, h * 0.70f);
                g.setColour (kMeterRed);    g.fillRect (x + w * 0.72f, y,             w * 0.28f, h);
                break;
            case K::spark:
                p.startNewSubPath (x, y + h * 0.8f);
                p.lineTo (x + w * 0.3f, y + h * 0.35f);
                p.lineTo (x + w * 0.55f, y + h * 0.6f);
                p.lineTo (x + w, y + h * 0.1f);
                g.strokePath (p, juce::PathStrokeType (1.4f));
                break;
            case K::text:
                g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.0f);
                g.fillRect (x + 3.0f, y + h * 0.5f - 0.75f, w - 6.0f, 1.5f);
                break;
            case K::lamp:
                g.drawEllipse (r.reduced (1.0f), 1.3f);
                g.fillEllipse (r.reduced (3.5f));
                break;
            case K::steps:
                for (int i = 0; i < 4; ++i)
                {
                    const auto cell = juce::Rectangle<float> (x + (float) i * w * 0.26f, y + h * 0.3f, w * 0.2f, h * 0.4f);
                    if (i == 2) g.fillRect (cell); else g.drawRect (cell, 1.0f);
                }
                break;
            case K::bipolar:
            {
                // quadrante a mezzaluna con lancetta
                juce::Path arc;
                arc.addCentredArc (x + w * 0.5f, y + h * 0.9f, w * 0.5f, h * 0.75f, 0.0f,
                                   -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
                g.strokePath (arc, juce::PathStrokeType (1.3f));
                g.drawLine (x + w * 0.5f, y + h * 0.9f, x + w * 0.78f, y + h * 0.3f, 1.4f);
                break;
            }
            case K::range:
                // barra piena con righello sotto
                g.fillRect (x, y + h * 0.2f, w * 0.65f, h * 0.35f);
                g.drawRect (x, y + h * 0.2f, w, h * 0.35f, 1.0f);
                for (int i = 0; i <= 4; ++i)
                    g.fillRect (x + (w - 1.0f) * (float) i / 4.0f, y + h * 0.65f, 1.0f, h * 0.25f);
                break;
            case K::adaptive:
            {
                // binario tratteggiato con un indice ad anello
                for (float dx = 0.0f; dx < w; dx += 3.0f)
                    g.fillRect (x + dx, y + h * 0.5f - 0.5f, 1.5f, 1.0f);
                g.drawEllipse (x + w * 0.55f - 3.0f, y + h * 0.5f - 3.0f, 6.0f, 6.0f, 1.4f);
                break;
            }
        }
    }

    juce::Colour meterColourForDb (double db)
    {
        return db >= -6.0 ? kMeterRed : (db >= -18.0 ? kMeterYellow : kMeterGreen);
    }
}

//==============================================================================
// Card del Monitor: canale fermo, reset, info e menu.

bool CsoundParameterMappingPanel::MonitorRow::stuck() const
{
    bool anyNumeric = false;
    for (const auto& e : entries)
    {
        if (e.isString || e.isAudio || e.led)
            continue;   // testo costante e silenzio sono legittimi; una spia ha visto l'1
        anyNumeric = true;
        if (! e.change.looksStuck())
            return false;
    }
    return anyNumeric;
}

void CsoundParameterMappingPanel::MonitorRow::resetObserved()
{
    for (auto& e : entries)
    {
        e.seenAny = false;
        e.peakHold = 0.0;
        e.peakHoldAge = 0;
        e.change = {};
        e.clipped = false;
        std::fill (e.history.begin(), e.history.end(), (float) shown (e));
    }
    repaint();
}

juce::String CsoundParameterMappingPanel::MonitorRow::infoText() const
{
    if (entries.empty())
        return {};

    juce::String text;
    if (multi)
    {
        juce::String base; int side = 0;
        const bool stereo = csoundMonitor::splitStereoName (entries.front().name, base, side);
        text << title << ": " << (int) entries.size() << (entries.front().isAudio ? " audio channels" : " channels")
             << (stereo ? " paired by name (L / R)" : " grouped by name (same prefix + index)")
             << " - " << entries.front().name << " .. " << entries.back().name;
    }
    else
    {
        const auto& e = entries.front();
        double lo, hi; barRange (e, lo, hi);
        text << e.name << ": " << csoundMonitor::describeKind (kindOf (e), lo, hi);
    }
    if (stuck())
        text << "\nNo change since Apply: the code creates this channel but never writes a new value"
                " (a chnset that does not run, a misspelt name, a name built at run time).";
    if (anyClipped())
        text << "\nCLIP: a sample reached 0 dBFS. Click the card to clear it.";
   #if JUCE_IOS
    text << "\nDouble-tap: reset the observed range. Touch and hold: menu.";
   #else
    text << "\nDouble-click: reset the observed range. Right-click: menu.";
   #endif
    return text;
}

juce::String CsoundParameterMappingPanel::MonitorRow::getTooltip()
{
    return infoText();
}

juce::String CsoundParameterMappingPanel::MonitorRow::chnKLines() const
{
    juce::StringArray lines;
    for (const auto& e : entries)
    {
        if (e.isString || e.isAudio)
            continue;   // chn_S / chn_a: niente range da dichiarare
        double lo, hi; barRange (e, lo, hi);
        if (! e.hasHints && ! e.seenAny)
            continue;
        lines.add (csoundMonitor::chnKDeclaration (e.name, e.value, lo, hi, attrs.spark));
    }
    return lines.joinIntoString ("\n");
}

void CsoundParameterMappingPanel::MonitorRow::showMenu()
{
    std::vector<CsoundActionSheetItem> items;
    // Intestazione breve (la spiegazione lunga e' nel tooltip): il tipo e
    // il range, e una seconda riga se la card e' ferma o in CLIP.
    {
        CsoundActionSheetItem info;
        if (multi)
        {
            juce::String base; int side = 0;
            info.sectionHeader = csoundMonitor::splitStereoName (entries.front().name, base, side)
                                   ? juce::String ("Stereo pair, L / R")
                                   : juce::String ((int) entries.size()) + " channels, by name";
        }
        else
        {
            double lo, hi; barRange (entries.front(), lo, hi);
            info.sectionHeader = csoundMonitor::shortKind (kindOf (entries.front()), lo, hi);
        }
        items.push_back (info);
    }
    if (stuck())
    {
        CsoundActionSheetItem note;
        note.sectionHeader = "No change since Apply";
        items.push_back (note);
    }
    if (anyClipped())
    {
        CsoundActionSheetItem note;
        note.sectionHeader = "Clipped at 0 dBFS";
        items.push_back (note);
    }
    {
        CsoundActionSheetItem item;
        item.id = 1;
        item.text = entries.size() > 1 ? "Copy chn_k Declarations" : "Copy chn_k Declaration";
        item.enabled = chnKLines().isNotEmpty();
        item.icon = CsoundActionSheetIcon::copy;
        items.push_back (item);
    }
    if (anyClipped())
    {
        CsoundActionSheetItem item;
        item.id = 3;
        item.text = "Reset Clip";
        item.icon = CsoundActionSheetIcon::resetDefault;
        items.push_back (item);
    }
    {
        CsoundActionSheetItem item;
        item.id = 2;
        item.text = "Reset Observed Range";
        item.icon = CsoundActionSheetIcon::resetDefault;
        items.push_back (item);
    }

    juce::Component::SafePointer<MonitorRow> safeThis (this);
    CsoundActionSheet::show (*this, title, std::move (items), [safeThis] (int result)
    {
        if (safeThis == nullptr)
            return;
        if (result == 1)
        {
            const auto lines = safeThis->chnKLines();
            juce::SystemClipboard::copyTextToClipboard (lines);
            if (safeThis->onLog)
                safeThis->onLog ("--- Copied to the clipboard (paste it in the orchestra header) ---\n" + lines);
        }
        else if (result == 2)
        {
            safeThis->resetObserved();
        }
        else if (result == 3)
        {
            safeThis->resetClip();
        }
    });
}

void CsoundParameterMappingPanel::MonitorRow::mouseUp (const juce::MouseEvent& e)
{
    const bool longPress = e.getLengthOfMousePress() > 450 && ! e.mouseWasDraggedSinceMouseDown();
    if (e.mods.isPopupMenu() || longPress)
        showMenu();
    else if (anyClipped() && e.getNumberOfClicks() == 1)
        resetClip();
}

bool CsoundParameterMappingPanel::MonitorRow::anyClipped() const
{
    for (const auto& e : entries)
        if (e.clipped)
            return true;
    return false;
}

void CsoundParameterMappingPanel::MonitorRow::resetClip()
{
    for (auto& e : entries)
        e.clipped = false;
    repaint();
}

void CsoundParameterMappingPanel::MonitorRow::mouseDoubleClick (const juce::MouseEvent&)
{
    resetObserved();
}

csoundMonitor::Kind CsoundParameterMappingPanel::MonitorRow::kindOf (const Entry& e) const
{
    double lo, hi;
    barRange (e, lo, hi);
    return csoundMonitor::kindOf (e.isString, e.isAudio, attrs.spark, e.led, e.hasHints, lo, hi, e.steps);
}

int CsoundParameterMappingPanel::MonitorRow::preferredHeight() const noexcept
{
    using K = csoundMonitor::Kind;
    if (multi)
        return 26 + 66;                          // titolo + barre verticali con etichetta
    if (entries.empty())
        return height;
    switch (kindOf (entries.front()))
    {
        case K::lamp:  return 36;                // una riga
        case K::text:  return 26 + 30;           // titolo + display
        case K::spark: return 26 + 42;           // titolo + curva
        case K::audio: return 26 + 30;           // titolo + meter a segmenti + scala
        case K::range:    return 26 + 32;        // titolo + barra + righello + estremi
        case K::adaptive: return 26 + 32;        // titolo + binario + estremi
        case K::bipolar: return 26 + 50;         // titolo + quadrante
        default:       return height;            // titolo + barra + etichette
    }
}

bool CsoundParameterMappingPanel::MonitorRow::isLed (const Entry& e) const
{
    return ! e.isString && ! e.isAudio && csoundMonitor::showsAsLed (e.hasHints, e.hintMin, e.hintMax, e.binary);
}

double CsoundParameterMappingPanel::MonitorRow::shown (const Entry& e) const
{
    return e.isAudio ? csoundMonitor::linearToDb (e.value) : e.value;
}

void CsoundParameterMappingPanel::MonitorRow::barRange (const Entry& e, double& lo, double& hi) const
{
    if (e.isAudio)       { lo = -60.0;     hi = 0.0; }
    else if (e.hasHints) { lo = e.hintMin; hi = e.hintMax; }
    else                 { lo = e.seenMin; hi = e.seenMax; }
}

juce::String CsoundParameterMappingPanel::MonitorRow::displayValue (const Entry& e) const
{
    if (e.isString)
        return e.text;
    return csoundMonitor::formatMonitorValue (e.value, e.isAudio);
}

void CsoundParameterMappingPanel::MonitorRow::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds();
    g.setColour (kCardBg);
    g.fillRect (b);
    g.setColour (kMonitorAccent.withAlpha (0.6f));
    g.fillRect (b.getX(), b.getY(), 5, b.getHeight());

    if (entries.empty())
        return;

    // Canale fermo dall'Apply: card attenuata e, al posto del valore, la
    // spiegazione (il codice lo crea ma non lo scrive).
    if (stuck())
    {
        g.beginTransparencyLayer (0.4f);
        paintContent (g);
        g.endTransparencyLayer();
        g.setColour (kStuck.withAlpha (0.9f));
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::italic)));
        g.drawFittedText ("no change since Apply", valueArea, juce::Justification::centredRight, 1);
        return;
    }
    paintContent (g);
}

void CsoundParameterMappingPanel::MonitorRow::paintContent (juce::Graphics& g)
{
    const auto b = getLocalBounds();

    auto area = b.reduced (cardPaddingH, 0).withTrimmedLeft (6);

    if (multi)
    {
        paintMulti (g, area);
        return;
    }

    const auto& e = entries.front();
    const auto k = kindOf (e);

    if (k == csoundMonitor::Kind::lamp)
    {
        paintLamp (g, area, e);
        return;
    }

    // Intestazione comune: glifo del tipo, nome, valore a destra.
    auto head = area.removeFromTop (26);
    drawKindGlyph (g, head.removeFromLeft (14).withSizeKeepingCentre (12, 12).toFloat(), k, false);
    head.removeFromLeft (6);
    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    const int nameW = juce::jmin (head.getWidth() / 2, 180);
    g.drawFittedText (title, head.removeFromLeft (nameW), juce::Justification::centredLeft, 1);
    valueArea = head.withTrimmedLeft (8);
    if (k != csoundMonitor::Kind::text && ! stuck())
    {
        g.setColour (k == csoundMonitor::Kind::audio ? meterColourForDb (shown (e)) : kTextMuted);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
        const auto valueText = k == csoundMonitor::Kind::steps
            ? displayValue (e) + " / " + csoundMonitor::formatMonitorValue (e.hintMax)
            : displayValue (e);
        g.drawFittedText (valueText, head.withTrimmedLeft (8), juce::Justification::centredRight, 1);
    }

    switch (k)
    {
        case csoundMonitor::Kind::audio:    paintAudio (g, area, e); break;
        case csoundMonitor::Kind::spark:    paintSpark (g, area, e); break;
        case csoundMonitor::Kind::text:     paintText  (g, area, e); break;
        case csoundMonitor::Kind::bipolar:  paintBipolar  (g, area, e); break;
        case csoundMonitor::Kind::steps:    paintSteps    (g, area, e); break;
        case csoundMonitor::Kind::adaptive: paintAdaptive (g, area, e); break;
        default:                            paintRange    (g, area, e); break;
    }
}

void CsoundParameterMappingPanel::MonitorRow::paintRange (juce::Graphics& g, juce::Rectangle<int> area, const Entry& e) const
{
    // RANGE DICHIARATO - una barra di livello solida: fondo incassato,
    // riempimento pieno da sinistra, righello con 5 tacche sotto e gli
    // estremi dichiarati.
    double lo, hi; barRange (e, lo, hi);
    auto bar = area.withHeight (11).toFloat();

    g.setColour (kPanelBg);
    g.fillRoundedRectangle (bar, 2.0f);
    g.setColour (kAccent.withAlpha (0.35f));
    g.drawRoundedRectangle (bar.reduced (0.5f), 2.0f, 1.0f);

    if (hi > lo)
    {
        const float f = csoundMonitor::barSpan (shown (e), lo, hi).to;
        auto fill = bar.reduced (1.5f);
        fill.setWidth (fill.getWidth() * f);
        g.setGradientFill (juce::ColourGradient (kAccent.darker (0.3f), fill.getX(), 0.0f,
                                                 kAccent.brighter (0.15f), bar.getRight(), 0.0f, false));
        g.fillRoundedRectangle (fill, 1.5f);
    }

    // righello: 5 tacche (0, 1/4, 1/2, 3/4, 1) sotto la barra
    const float tickTop = bar.getBottom() + 2.0f;
    g.setColour (kTextMuted.withAlpha (0.7f));
    for (int i = 0; i <= 4; ++i)
    {
        const float tx = bar.getX() + (bar.getWidth() - 1.0f) * (float) i / 4.0f;
        g.fillRect (tx, tickTop, 1.0f, i % 2 == 0 ? 4.0f : 2.5f);
    }

    auto labels = area.withTrimmedTop (18).withHeight (11);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (csoundMonitor::formatMonitorValue (lo), labels, juce::Justification::centredLeft, false);
    g.drawText (csoundMonitor::formatMonitorValue (hi), labels, juce::Justification::centredRight, false);
}

void CsoundParameterMappingPanel::MonitorRow::paintAdaptive (juce::Graphics& g, juce::Rectangle<int> area, const Entry& e) const
{
    // RANGE NON DICHIARATO - nessun riempimento: un binario tratteggiato e
    // un indice ad anello che si sposta fra il minimo e il massimo visti
    // finora (in corsivo, con le frecce: osservati, non dichiarati).
    double lo, hi; barRange (e, lo, hi);
    auto track = area.withHeight (14).toFloat().reduced (6.0f, 0.0f);
    const float cy = track.getCentreY();

    const float dash[] = { 4.0f, 3.0f };
    juce::Path line; line.startNewSubPath (track.getX(), cy); line.lineTo (track.getRight(), cy);
    juce::Path dashed;
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, line, dash, 2);
    g.setColour (kTextMuted.withAlpha (0.6f));
    g.fillPath (dashed);
    g.fillRect (track.getX() - 0.75f, cy - 4.0f, 1.5f, 8.0f);       // estremi
    g.fillRect (track.getRight() - 0.75f, cy - 4.0f, 1.5f, 8.0f);

    if (hi > lo)
    {
        const float f = csoundMonitor::barSpan (shown (e), lo, hi).to;
        const float mx = track.getX() + track.getWidth() * f;
        g.setColour (kAccent.withAlpha (0.25f));
        g.fillEllipse (mx - 8.0f, cy - 8.0f, 16.0f, 16.0f);
        g.setColour (kAccent);
        g.drawEllipse (mx - 5.5f, cy - 5.5f, 11.0f, 11.0f, 2.0f);
    }

    auto labels = area.withTrimmedTop (17).withHeight (11);
    g.setColour (kTextMuted.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::italic)));
    const juce::String down (juce::CharPointer_UTF8 ("\xe2\x86\x93 ")), up (juce::CharPointer_UTF8 (" \xe2\x86\x91"));
    g.drawText (down + csoundMonitor::formatMonitorValue (lo), labels, juce::Justification::centredLeft, false);
    g.drawText (csoundMonitor::formatMonitorValue (hi) + up, labels, juce::Justification::centredRight, false);
}

void CsoundParameterMappingPanel::MonitorRow::paintBipolar (juce::Graphics& g, juce::Rectangle<int> area, const Entry& e) const
{
    // RANGE ATTRAVERSO LO ZERO - quadrante a mezzaluna con lancetta: lo zero
    // in alto, i negativi a sinistra (blu), i positivi a destra; l'arco si
    // colora dallo zero al valore.
    double lo, hi; barRange (e, lo, hi);
    auto dial = area.withTrimmedBottom (4).toFloat();
    const float radius = juce::jmax (10.0f, juce::jmin (dial.getHeight() - 4.0f, dial.getWidth() * 0.5f - 40.0f));
    const float cx = dial.getCentreX(), cy = dial.getBottom() - 2.0f;
    constexpr float halfPi = juce::MathConstants<float>::halfPi;
    const juce::Point<float> centre (cx, cy);

    const auto angleOf = [&] (double v)
    {
        const float f = (float) juce::jlimit (0.0, 1.0, (v - lo) / (hi - lo));
        return -halfPi + f * 2.0f * halfPi;
    };

    juce::Path trackArc;
    trackArc.addCentredArc (cx, cy, radius, radius, 0.0f, -halfPi, halfPi, true);
    g.setColour (kPanelBg.brighter (0.1f));
    g.strokePath (trackArc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    if (hi > lo)
    {
        const float a0 = angleOf (0.0), a1 = angleOf (shown (e));
        if (std::abs (a1 - a0) > 0.001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (cx, cy, radius, radius, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
            g.setColour (a1 < a0 ? kNegative : kAccent);
            g.strokePath (valueArc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        // tacca dello zero
        g.setColour (kText);
        g.drawLine ({ centre.getPointOnCircumference (radius - 6.0f, a0), centre.getPointOnCircumference (radius + 6.0f, a0) }, 1.5f);

        // lancetta
        g.drawLine ({ centre, centre.getPointOnCircumference (radius - 2.0f, a1) }, 2.0f);
        g.fillEllipse (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
    }

    // estremi ai due lati dell'arco
    g.setColour (kTextMuted.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (10.0f, e.hasHints ? juce::Font::plain : juce::Font::italic)));
    g.drawText (csoundMonitor::formatMonitorValue (lo), juce::Rectangle<float> (cx - radius - 60.0f, cy - 12.0f, 50.0f, 12.0f).toNearestInt(),
                juce::Justification::centredRight, false);
    g.drawText (csoundMonitor::formatMonitorValue (hi), juce::Rectangle<float> (cx + radius + 10.0f, cy - 12.0f, 50.0f, 12.0f).toNearestInt(),
                juce::Justification::centredLeft, false);
}

void CsoundParameterMappingPanel::MonitorRow::paintAudio (juce::Graphics& g, juce::Rectangle<int> area, const Entry& e) const
{
    // Meter a segmenti -60..0 dB: verde fino a -18, giallo fino a -6, rosso.
    // A destra la spia CLIP: si accende a 0 dBFS e resta accesa finche'
    // non si clicca la card (come su un mixer).
    auto clipBox = area.removeFromRight (34).withHeight (12).toFloat().withTrimmedLeft (6.0f).translated (0.0f, -1.0f);
    g.setColour (e.clipped ? kMeterRed : kMeterRed.withAlpha (0.12f));
    g.fillRoundedRectangle (clipBox, 2.0f);
    g.setColour (e.clipped ? juce::Colours::white : kMeterRed.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
    g.drawText ("CLIP", clipBox, juce::Justification::centred, false);

    auto meter = area.withHeight (10).toFloat();
    g.setColour (kLcdBg);
    g.fillRoundedRectangle (meter.expanded (1.0f), 2.0f);

    const int segments = 30;
    const float gap = 1.5f;
    const float segW = (meter.getWidth() - gap * (float) (segments - 1)) / (float) segments;
    const double level = shown (e);
    for (int i = 0; i < segments; ++i)
    {
        const double segDb = -60.0 + 60.0 * (double) (i + 1) / (double) segments;
        const bool lit = level >= segDb - 60.0 / segments;
        const auto colour = meterColourForDb (segDb);
        g.setColour (lit ? colour : colour.withAlpha (0.13f));
        g.fillRect (meter.getX() + (float) i * (segW + gap), meter.getY(), segW, meter.getHeight());
    }

    // Picco tenuto: una linea chiara.
    const double peakDb = csoundMonitor::linearToDb (e.peakHold);
    if (peakDb > -59.0)
    {
        const float px = meter.getX() + meter.getWidth() * (float) ((peakDb + 60.0) / 60.0);
        g.setColour (meterColourForDb (peakDb).brighter (0.4f));
        g.fillRect (px - 1.0f, meter.getY() - 2.0f, 2.0f, meter.getHeight() + 4.0f);
    }

    // Scala in dB.
    auto scale = area.withTrimmedTop (13).withHeight (11);
    g.setColour (kTextMuted.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    for (const int db : { -48, -24, -12, -6, 0 })
    {
        const int x = scale.getX() + (int) ((float) scale.getWidth() * (float) (db + 60) / 60.0f);
        g.drawText (juce::String (db), x - 14, scale.getY(), 28, scale.getHeight(),
                    db == 0 ? juce::Justification::centredRight : juce::Justification::centred, false);
    }
}

void CsoundParameterMappingPanel::MonitorRow::paintSpark (juce::Graphics& g, juce::Rectangle<int> area, const Entry& e) const
{
    double lo, hi; barRange (e, lo, hi);
    auto plot = area.withTrimmedBottom (6).toFloat();
    g.setColour (kPanelBg);
    g.fillRoundedRectangle (plot, 3.0f);
    if (! (hi > lo) || e.history.empty())
        return;

    const auto yOf = [&] (double v) { return plot.getBottom() - 3.0f - (plot.getHeight() - 6.0f) * (float) juce::jlimit (0.0, 1.0, (v - lo) / (hi - lo)); };

    if (lo < 0.0 && hi > 0.0)
    {
        g.setColour (kTextMuted.withAlpha (0.35f));
        g.fillRect (plot.getX() + 2.0f, yOf (0.0) - 0.5f, plot.getWidth() - 4.0f, 1.0f);
    }

    juce::Path line, area2;
    const int n = (int) e.history.size();
    float lastX = 0.0f, lastY = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float v = e.history[(size_t) ((e.historyPos + i) % n)];
        const float x = plot.getX() + 2.0f + (plot.getWidth() - 8.0f) * (float) i / (float) juce::jmax (1, n - 1);
        const float y = yOf (v);
        if (i == 0) { line.startNewSubPath (x, y); area2.startNewSubPath (x, plot.getBottom() - 2.0f); area2.lineTo (x, y); }
        else        { line.lineTo (x, y); area2.lineTo (x, y); }
        lastX = x; lastY = y;
    }
    area2.lineTo (lastX, plot.getBottom() - 2.0f);
    area2.closeSubPath();

    g.setGradientFill (juce::ColourGradient (kAccent.withAlpha (0.35f), 0.0f, plot.getY(), kAccent.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
    g.fillPath (area2);
    g.setColour (kAccent);
    g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillEllipse (lastX - 3.0f, lastY - 3.0f, 6.0f, 6.0f);
}

void CsoundParameterMappingPanel::MonitorRow::paintText (juce::Graphics& g, juce::Rectangle<int> area, const Entry& e) const
{
    // Display incassato tipo LCD, testo monospaziato in accento.
    auto box = area.withTrimmedBottom (6).toFloat();
    g.setColour (kLcdBg);
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (kMonitorAccent.withAlpha (0.35f));
    g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (kAccent.brighter (0.2f));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    g.drawFittedText (e.text.isEmpty() ? juce::String ("-") : e.text, box.reduced (10.0f, 0.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1);
}

void CsoundParameterMappingPanel::MonitorRow::paintLamp (juce::Graphics& g, juce::Rectangle<int> area, const Entry& e) const
{
    auto row = area;
    drawKindGlyph (g, row.removeFromLeft (14).withSizeKeepingCentre (12, 12).toFloat(), csoundMonitor::Kind::lamp, false);
    row.removeFromLeft (6);
    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawFittedText (title, row.removeFromLeft (juce::jmin (row.getWidth() / 2, 180)), juce::Justification::centredLeft, 1);

    const bool on = e.value >= 0.5;
    auto right = row.removeFromRight (64);
    g.setColour (on ? kText : kTextMuted);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
    g.drawFittedText (on ? "ON" : "OFF", right.removeFromRight (34), juce::Justification::centredRight, 1);
    const auto lamp = juce::Rectangle<float> (14.0f, 14.0f).withCentre (right.toFloat().getCentre());
    if (on)
    {
        g.setColour (kAccent.withAlpha (0.25f));
        g.fillEllipse (lamp.expanded (4.0f));
    }
    g.setColour (on ? kAccent : kPanelBg.brighter (0.15f));
    g.fillEllipse (lamp);
    g.setColour (kAccent.withAlpha (on ? 1.0f : 0.35f));
    g.drawEllipse (lamp, 1.2f);
}

void CsoundParameterMappingPanel::MonitorRow::paintSteps (juce::Graphics& g, juce::Rectangle<int> area, const Entry& e) const
{
    // Una cella per valore intero del range: la corrente piena e luminosa,
    // le precedenti tenui (la "strada fatta" di un sequencer), le altre
    // vuote. Il numero nella cella se c'e' posto.
    const int lo = (int) e.hintMin, hi = (int) e.hintMax;
    const int cells = hi - lo + 1;
    const int current = juce::jlimit (lo, hi, (int) std::lround (e.value));
    auto strip = area.withHeight (18).toFloat();
    const float gap = cells > 16 ? 2.0f : 3.0f;
    const float cellW = (strip.getWidth() - gap * (float) (cells - 1)) / (float) cells;

    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold)));
    for (int i = 0; i < cells; ++i)
    {
        const int v = lo + i;
        const auto cell = juce::Rectangle<float> (strip.getX() + (float) i * (cellW + gap), strip.getY(), cellW, strip.getHeight());
        if (v == current)
        {
            g.setColour (kAccent.withAlpha (0.3f));
            g.fillRoundedRectangle (cell.expanded (1.5f), 4.0f);
            g.setColour (kAccent);
            g.fillRoundedRectangle (cell, 3.0f);
        }
        else
        {
            g.setColour (v < current ? kAccent.withAlpha (0.22f) : kPanelBg);
            g.fillRoundedRectangle (cell, 3.0f);
            g.setColour (kAccent.withAlpha (0.35f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 3.0f, 1.0f);
        }

        if (cellW >= 14.0f)
        {
            g.setColour (v == current ? kOnAccent : kTextMuted);
            g.drawText (juce::String (v), cell, juce::Justification::centred, false);
        }
    }
}

void CsoundParameterMappingPanel::MonitorRow::paintMulti (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const bool audio = entries.front().isAudio;
    auto head = area.removeFromTop (26);
    drawKindGlyph (g, head.removeFromLeft (14).withSizeKeepingCentre (12, 12).toFloat(), csoundMonitor::Kind::range, true);
    head.removeFromLeft (6);
    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawFittedText (title, head.removeFromLeft (juce::jmin (head.getWidth() / 2, 180)), juce::Justification::centredLeft, 1);
    valueArea = head;
    if (! stuck())
    {
        g.setColour (kTextMuted);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
        g.drawFittedText (juce::String ((int) entries.size()) + (audio ? " audio" : " ch"), head, juce::Justification::centredRight, 1);
    }

    auto plot = area.withTrimmedTop (2).withTrimmedBottom (4);
    const int labelH = 12;
    auto bars = plot.withTrimmedBottom (labelH);
    const int n = (int) entries.size();
    const float slot = (float) bars.getWidth() / (float) n;
    const float barW = juce::jmax (2.0f, slot - 3.0f);

    g.setColour (audio ? kLcdBg : kPanelBg);
    g.fillRoundedRectangle (bars.toFloat(), 3.0f);

    for (int i = 0; i < n; ++i)
    {
        const auto& e = entries[(size_t) i];
        double lo, hi; barRange (e, lo, hi);
        const float x = (float) bars.getX() + slot * (float) i + (slot - barW) * 0.5f;

        if (e.led)
        {
            // Spia: cerchio pieno se acceso (es. i passi di un sequencer).
            const float d = juce::jmin (barW, (float) bars.getHeight() * 0.5f, 18.0f);
            const auto r = juce::Rectangle<float> (d, d).withCentre ({ x + barW * 0.5f, (float) bars.getCentreY() });
            const bool on = e.value >= 0.5;
            if (on)
            {
                g.setColour (kAccent.withAlpha (0.25f));
                g.fillEllipse (r.expanded (3.0f));
            }
            g.setColour (on ? kAccent : kPanelBg.brighter (0.15f));
            g.fillEllipse (r);
            g.setColour (kAccent.withAlpha (on ? 1.0f : 0.35f));
            g.drawEllipse (r, 1.2f);
        }
        else if (audio)
        {
            // Meter verticale a segmenti, verde/giallo/rosso, con la spia
            // CLIP in cima (resta accesa finche' non si clicca la card).
            g.setColour (e.clipped ? kMeterRed : kMeterRed.withAlpha (0.12f));
            g.fillRect (x, (float) bars.getY() + 2.0f, barW, 5.0f);
            const int segments = 12;
            const float segH = ((float) bars.getHeight() - 4.0f - 7.0f - (float) (segments - 1)) / (float) segments;
            const double level = shown (e);
            for (int s2 = 0; s2 < segments; ++s2)
            {
                const double segDb = -60.0 + 60.0 * (double) (s2 + 1) / (double) segments;
                const auto colour = meterColourForDb (segDb);
                g.setColour (level >= segDb - 60.0 / segments ? colour : colour.withAlpha (0.13f));
                g.fillRect (x, (float) bars.getBottom() - 2.0f - (float) (s2 + 1) * (segH + 1.0f) + 1.0f, barW, segH);
            }
        }
        else if (hi > lo)
        {
            // Barra verticale; bipolare (range attraverso lo zero) = dallo zero.
            const auto span = csoundMonitor::barSpan (shown (e), lo, hi);
            const float bottom = (float) bars.getBottom(), h = (float) bars.getHeight();
            const float y1 = bottom - h * span.to, y0 = bottom - h * span.from;
            const bool negative = span.zero >= 0.0f && span.from < span.zero;
            g.setColour ((negative ? kNegative : kAccent).withAlpha (e.hasHints ? 0.85f : 0.6f));
            g.fillRoundedRectangle (x, y1, barW, juce::jmax (2.0f, y0 - y1), 2.0f);
            if (span.zero >= 0.0f)
            {
                g.setColour (kText.withAlpha (0.7f));
                g.fillRect (x - 1.0f, bottom - h * span.zero - 0.75f, barW + 2.0f, 1.5f);
            }
        }

        if (slot >= 14.0f)
        {
            g.setColour (kTextMuted);
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawFittedText (e.label, juce::Rectangle<int> ((int) x - 4, bars.getBottom(), (int) barW + 8, labelH),
                              juce::Justification::centred, 1);
        }
    }
}

void CsoundParameterMappingPanel::refreshMonitorList()
{
    const auto engine = processor.getEngineIdentity();
    auto channels = processor.listMonitorChannels();

    juce::StringArray names;
    for (const auto& c : channels) names.add (c.name + "|" + c.attributes);
    const auto signature = names.joinIntoString ("\n");

    if (signature == monitorSignature && engine == monitorEngine)
        return;

    const bool wasEmpty = monitorRows.empty();
    const bool sameRows = signature == monitorSignature;   // stesso elenco, motore nuovo: solo i puntatori cambiano
    const bool sameEngine = engine == monitorEngine;
    monitorEngine = engine;
    monitorSignature = signature;
    monitorChannels = std::move (channels);

    // CLIP: il thread audio sorveglia il picco dei canali audio del Monitor.
    {
        std::vector<cs_float*> audioPtrs;
        for (const auto& c : monitorChannels)
            if (c.isAudio && c.valuePtr != nullptr && (int) audioPtrs.size() < CsoundAudioProcessor::kMaxAudioPeakWatches)
                audioPtrs.push_back (c.valuePtr);
        processor.setAudioPeakWatches (audioPtrs, engine);
    }

    if (! sameRows || monitorRows.empty())
    {
        // Card dai canali: attributi di chn_k + accorpamento per nome
        // (CsoundMonitorLayout.h). Ricreate solo se cambia l'elenco.
        std::vector<csoundMonitor::ChannelInfo> infos;
        for (const auto& c : monitorChannels)
            infos.push_back ({ c.name, c.isString, c.isAudio });

        // Stesso motore, elenco cambiato (un canale nuovo creato a runtime):
        // le card si ricostruiscono, ma lo stato osservato dei canali che
        // c'erano gia' resta - riconoscimento della spia, min/max visti,
        // storia, picco. Altrimenti a ogni canale nuovo una spia tornerebbe
        // barra per un secondo e i range adattivi ripartirebbero da zero.
        std::map<juce::String, MonitorRow::Entry> previous;
        if (sameEngine)
            for (const auto& r : monitorRows)
                for (const auto& old : r->entries)
                    previous[old.name] = old;

        monitorRows.clear();
        for (const auto& card : csoundMonitor::buildCards (infos))
        {
            auto row = std::make_unique<MonitorRow>();
            row->title = card.title;
            row->multi = card.multi;
            if (! card.channels.empty())
                row->attrs = csoundMonitor::parseAttributes (monitorChannels[(size_t) card.channels.front()].attributes);

            for (int idx : card.channels)
            {
                const auto& c = monitorChannels[(size_t) idx];
                MonitorRow::Entry e;
                e.name = c.name; e.channelIndex = idx; e.isString = c.isString; e.isAudio = c.isAudio;
                e.hasHints = c.hasHints; e.hintMin = c.hintMin; e.hintMax = c.hintMax;
                juce::String base; int index = -1;
                if (csoundMonitor::splitStereoName (c.name, base, index))
                    e.label = index == 1 ? "L" : "R";
                else
                    e.label = csoundMonitor::splitIndexedName (c.name, base, index) ? juce::String (index) : c.name;
                if (row->attrs.spark)
                    e.history.assign ((size_t) MonitorRow::kHistoryLength, 0.0f);
                if (const auto it = previous.find (c.name); it != previous.end() && it->second.isAudio == e.isAudio)
                {
                    const auto& old = it->second;
                    e.text = old.text; e.value = old.value;
                    e.seenMin = old.seenMin; e.seenMax = old.seenMax; e.seenAny = old.seenAny;
                    e.peakHold = old.peakHold; e.peakHoldAge = old.peakHoldAge;
                    e.binary = old.binary; e.led = old.led;
                    e.ints = old.ints; e.steps = old.steps;
                    e.change = old.change;
                    e.clipped = old.clipped;
                    if (old.history.size() == e.history.size())
                    {
                        e.history = old.history;
                        e.historyPos = old.historyPos;
                    }
                }
                row->entries.push_back (std::move (e));
            }
            row->setTitle (row->title);
            row->onLog = [this] (const juce::String& m) { if (onParameterCopiedToClipboard) onParameterCopiedToClipboard (m); };
            rowsContainer.addAndMakeVisible (*row);
            monitorRows.push_back (std::move (row));
        }
        assignMonitorGroups();
    }

    if (monitorRows.empty())
    {
        monitorHeader = nullptr;
    }
    else if (monitorHeader == nullptr)
    {
        monitorHeader = std::make_unique<GroupHeader>();
        monitorHeader->name = "Monitor";
        monitorHeader->special = true;
        monitorHeader->setTitle ("Monitor, channels written by the code");
        if (wasEmpty && (int) monitorRows.size() > kMonitorCollapsedByDefaultAbove)
            collapsedGroups.insert (kMonitorStateKey);
        monitorHeader->onToggle = [this]
        {
            monitorHeader->collapsed = ! monitorHeader->collapsed;
            if (monitorHeader->collapsed) collapsedGroups.insert (kMonitorStateKey);
            else                          collapsedGroups.erase (kMonitorStateKey);
            monitorHeader->repaint();
            resized();
        };
        monitorHeader->onToggleAll = [this] (bool collapseAll) { setAllGroupsCollapsed (collapseAll); };
        rowsContainer.addAndMakeVisible (*monitorHeader);
        monitorHeader->toBack();
    }

    if (monitorHeader != nullptr)
    {
        monitorHeader->count = (int) monitorRows.size();
        monitorHeader->collapsed = collapsedGroups.count (kMonitorStateKey) > 0;
    }

    const bool empty = unifiedRows.empty() && pendingRow == nullptr && monitorRows.empty();
    viewport.setVisible (! empty);
    emptyStateLabel.setVisible (empty);
    resized();
}

void CsoundParameterMappingPanel::updateMonitorValues()
{
    if (monitorRows.empty())
        return;

    // Motore cambiato (Apply): i puntatori non valgono piu' - rifai la lista
    // prima di leggere qualunque cosa.
    if (processor.getEngineIdentity() != monitorEngine)
    {
        refreshMonitorList();
        return;
    }

    bool layoutChanged = false;

    // Solo le card nella parte visibile del pannello vengono ridisegnate.
    // I valori si leggono comunque per tutte (spia, passi, min/max visti,
    // storia, "fermo" e CLIP restano giusti anche fuori vista); quando una
    // card rientra scorrendo, il viewport la ridisegna con lo stato attuale.
    // (JUCE scarterebbe gia' il ridisegno fuori dal viewport, ma cosi' non
    // si percorre nemmeno la catena dei repaint() 20 volte al secondo.)
    const auto viewArea = viewport.getViewArea();

    for (auto& rowPtr : monitorRows)
    {
        auto& row = *rowPtr;
        if (! row.isVisible())
            continue;   // gruppo chiuso (Monitor o gruppo dei parametri)
        bool changed = false;

        for (auto& e : row.entries)
        {
            if (! juce::isPositiveAndBelow (e.channelIndex, (int) monitorChannels.size()))
                continue;
            const auto& ch = monitorChannels[(size_t) e.channelIndex];

            if (ch.isString)
            {
                const auto t = processor.readMonitorString (ch);
                if (t != e.text) { e.text = t; changed = true; }
                continue;
            }

            double v = processor.readMonitorValue (ch);
            if (e.isAudio)
            {
                const double raw = v;
                v = csoundMonitor::meterBallistics (v, e.value);   // un blocco di ksmps letto ogni 50 ms: balistica
                if (processor.takeAudioPeak (ch.valuePtr) >= 1.0f && ! e.clipped)
                {
                    e.clipped = true;
                    changed = true;
                }
                // Picco tenuto: 1 s fermo, poi scende.
                if (raw >= e.peakHold) { e.peakHold = raw; e.peakHoldAge = 0; }
                else if (++e.peakHoldAge > 20) e.peakHold *= 0.9;
            }
            if (! e.seenAny) { e.seenMin = e.seenMax = v; e.seenAny = true; }
            e.seenMin = juce::jmin (e.seenMin, v);
            e.seenMax = juce::jmax (e.seenMax, v);
            if (v != e.value) { e.value = v; changed = true; }

            const bool wasStuck = e.change.looksStuck();
            e.change.observe (v);
            if (wasStuck != e.change.looksStuck())
                changed = true;
            e.binary.observe (v);
            e.ints.observe (v);
            if (! row.multi && ! e.isAudio)
            {
                const bool steps = csoundMonitor::showsAsSteps (e.hasHints, e.hintMin, e.hintMax, e.ints);
                if (steps != e.steps) { e.steps = steps; changed = true; }
            }
            if (! row.multi)
            {
                const bool led = row.isLed (e);
                if (led != e.led)
                {
                    e.led = led;
                    changed = true;
                    layoutChanged = true;   // card singola: altezza diversa
                }
            }

            if (! e.history.empty())
            {
                e.history[(size_t) e.historyPos] = (float) row.shown (e);
                e.historyPos = (e.historyPos + 1) % (int) e.history.size();
                changed = true; // la sparkline scorre anche a valore fermo
            }
        }

        // Card multipla: tutti i membri spie o nessuno (csoundMonitor::combine).
        if (row.multi && ! row.entries.empty() && ! row.entries.front().isAudio)
        {
            auto group = row.entries.front().binary;
            bool hintsAllow = true;
            for (const auto& e : row.entries)
            {
                group = csoundMonitor::combine (group, e.binary);
                if (e.isString || (e.hasHints && ! (e.hintMin == 0.0 && e.hintMax == 1.0)))
                    hintsAllow = false;
            }
            const bool led = hintsAllow && group.looksBinary();
            for (auto& e : row.entries)
                if (e.led != led) { e.led = led; changed = true; }
        }

        if (changed && row.getBounds().intersects (viewArea))
            row.repaint();
    }

    if (layoutChanged)
        resized();   // una card e' diventata (o non e' piu') una spia: altezza diversa
}

void CsoundParameterMappingPanel::scrollRowIntoView (juce::Component& row)
{
    const auto bounds = row.getBounds(); // in rowsContainer
    const int viewTop = viewport.getViewPositionY();
    const int viewBottom = viewTop + viewport.getViewHeight();

    if (bounds.getBottom() > viewBottom)
        viewport.setViewPosition (viewport.getViewPositionX(), juce::jmax (0, bounds.getBottom() - viewport.getViewHeight() + 4));
    else if (bounds.getY() < viewTop)
        viewport.setViewPosition (viewport.getViewPositionX(), juce::jmax (0, bounds.getY() - 4));
}

void CsoundParameterMappingPanel::showPadMenu (XYPadRow& row)
{
    const auto xName = processor.getChannelParamSlot (row.xIndex).channelName;
    const auto yName = processor.getChannelParamSlot (row.yIndex).channelName;

    std::vector<CsoundActionSheetItem> items;
    auto add = [&items] (int id, const juce::String& text, CsoundActionSheetIcon icon)
    {
        CsoundActionSheetItem item;
        item.id = id; item.text = text; item.icon = icon;
        items.push_back (item);
    };

    add (1, "Edit X: " + xName, CsoundActionSheetIcon::sliderFloat);
    add (2, "Edit Y: " + yName, CsoundActionSheetIcon::sliderFloat);
    items.push_back (CsoundActionSheetItem::separator());
    add (3, "Copy chnget lines", CsoundActionSheetIcon::copy);
    add (4, "Reset to Init Values", CsoundActionSheetIcon::resetDefault);
    add (5, "Randomize", CsoundActionSheetIcon::sliderFloat);
    add (6, "Show in Code", CsoundActionSheetIcon::codeBraces);
    items.push_back (CsoundActionSheetItem::separator());
    add (7, "Remove X: " + xName, CsoundActionSheetIcon::trash);
    add (8, "Remove Y: " + yName, CsoundActionSheetIcon::trash);

    juce::Component::SafePointer<CsoundParameterMappingPanel> safeThis (this);
    juce::Component::SafePointer<XYPadRow> safeRow (&row);
    const int x = row.xIndex, y = row.yIndex;

    CsoundActionSheet::show (*this, xName + " / " + yName, std::move (items), [safeThis, safeRow, x, y, xName, yName] (int result)
    {
        if (safeThis == nullptr || safeRow == nullptr)
            return;
        auto& proc = safeThis->processor;
        auto log = [safeThis] (const juce::String& m) { if (safeThis->onParameterCopiedToClipboard) safeThis->onParameterCopiedToClipboard (m); };

        auto openEdit = [safeThis, safeRow, x] (bool yAxis)
        {
            if (yAxis) safeThis->padsEditingY.insert (x); else safeThis->padsEditingY.erase (x);
            safeRow->setEditAxis (yAxis);
            if (! safeRow->rowEditMode) safeRow->setEditMode (true);
            if (safeRow->onEditModeChanged) safeRow->onEditModeChanged();
        };

        auto confirmRemove = [safeThis, safeRow] (const juce::String& name, bool yAxis)
        {
            showNativeTwoButtonAlertAsync (
                "Remove parameter?",
                "\"" + name + "\" will be removed from the parameters (the pad becomes a slider). You can undo this.",
                "Cancel", "Remove",
                [safeRow, yAxis] (int choice)
                {
                    if (choice == 2 && safeRow != nullptr)
                        (yAxis ? safeRow->editRowY : safeRow->editRowX)->requestRemove();
                },
                safeThis.getComponent());
        };

        switch (result)
        {
            case 1: openEdit (false); break;
            case 2: openEdit (true); break;
            case 3:
            {
                const auto sx = proc.getChannelParamSlot (x), sy = proc.getChannelParamSlot (y);
                juce::String text;
                text << ";" << makeFloatConfigComment (sx) << "\n" << "k" << sx.channelName << " chnget \"" << sx.channelName << "\"\n"
                     << ";" << makeFloatConfigComment (sy) << "\n" << "k" << sy.channelName << " chnget \"" << sy.channelName << "\"\n";
                juce::SystemClipboard::copyTextToClipboard (text);
                log ("--- Copied to clipboard: chnget lines for " + xName + " and " + yName + " ---");
                break;
            }
            case 4: proc.resetParameterValueToDefault (0, x); proc.resetParameterValueToDefault (0, y); break;
            case 5: proc.randomizeParameterValue (0, x); proc.randomizeParameterValue (0, y); break;
            case 6: if (safeThis->onChannelNameClicked) safeThis->onChannelNameClicked (xName); break;
            case 7: confirmRemove (xName, false); break;
            case 8: confirmRemove (yName, true); break;
            default: break;
        }
    });
}

void CsoundParameterMappingPanel::showRowMenu (int kind, int slotIndex, std::function<void()> onRemove)
{
    juce::String name;
    switch (kind)
    {
        case 0:  name = processor.getChannelParamSlot (slotIndex).channelName; break;
        case 1:  name = processor.getIntParamSlot (slotIndex).channelName;     break;
        case 2:  name = processor.getBoolParamSlot (slotIndex).channelName;    break;
        default: name = processor.getChoiceParamSlot (slotIndex).channelName;  break;
    }

    std::vector<CsoundActionSheetItem> items;
    auto add = [&items] (int id, const juce::String& text, CsoundActionSheetIcon icon)
    {
        CsoundActionSheetItem item;
        item.id = id; item.text = text; item.icon = icon;
        items.push_back (item);
    };

    add (2, "Copy chnget line", CsoundActionSheetIcon::copy);
    add (3, "Reset to Init Value", CsoundActionSheetIcon::resetDefault);
    add (5, "Randomize", CsoundActionSheetIcon::sliderFloat);
    add (6, "Show in Code", CsoundActionSheetIcon::codeBraces);
    items.push_back (CsoundActionSheetItem::separator());
    add (4, "Remove", CsoundActionSheetIcon::trash);

    juce::Component::SafePointer<CsoundParameterMappingPanel> safeThis (this);

    CsoundActionSheet::show (*this, name, std::move (items), [safeThis, kind, slotIndex, onRemove, name] (int result)
    {
        if (safeThis == nullptr)
            return;

        auto& proc = safeThis->processor;
        auto log = [safeThis] (const juce::String& msg)
        {
            if (safeThis->onParameterCopiedToClipboard)
                safeThis->onParameterCopiedToClipboard (msg);
        };

        switch (result)
        {
            case 2:
                switch (kind)
                {
                    case 0: { const auto s = proc.getChannelParamSlot (slotIndex); copyChngetToClipboard (s.channelName, makeFloatConfigComment (s), log); break; }
                    case 1: { const auto s = proc.getIntParamSlot (slotIndex);     copyChngetToClipboard (s.channelName, makeIntConfigComment (s), log); break; }
                    case 2: { const auto s = proc.getBoolParamSlot (slotIndex);    copyChngetToClipboard (s.channelName, makeBoolConfigComment (s), log); break; }
                    default:{ const auto s = proc.getChoiceParamSlot (slotIndex);  copyChngetToClipboard (s.channelName, makeChoiceConfigComment (s), log); break; }
                }
                break;

            case 3: proc.resetParameterValueToDefault (kind, slotIndex); break;
            case 5: proc.randomizeParameterValue (kind, slotIndex); break;
            case 6: if (safeThis->onChannelNameClicked) safeThis->onChannelNameClicked (name); break;

            case 4:
                // Conferma prima di rimuovere (richiesta esplicita); Undo resta possibile.
                showNativeTwoButtonAlertAsync (
                    "Remove parameter?",
                    "\"" + name + "\" will be removed from the parameters. You can undo this.",
                    "Cancel", "Remove",
                    [onRemove] (int choice) { if (choice == 2 && onRemove) onRemove(); },
                    safeThis.getComponent());
                break;

            default: break;
        }
    });
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
        // Titolo a SINISTRA; a destra [+] [snapshot]. Il "+" e' allineato in
        // verticale alla lente della toolbar principale (setMenuButtonCentreX),
        // lo snapshot al burger.
        auto bar = titleBarBounds.reduced (panelTitleBarPaddingH, 0);
        const int d = panelTitleBarButtonDiameter;
        snapshotsButton.setBounds (bar.removeFromRight (d).withSizeKeepingCentre (d, d));
        int addX = bar.getRight() - 10 - d;   // default: subito a sinistra dello snapshot
        if (menuButtonCentreX > 0)
            addX = juce::jlimit (bar.getX() + 60, snapshotsButton.getX() - 4 - d, menuButtonCentreX - d / 2);
        addButton.setBounds (addX, bar.getCentreY() - d / 2, d, d);
        titleLabel.setBounds (bar.withRight (addX - 6).withTrimmedLeft (-4));
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

    if (displayGroups.empty())
    {
        for (auto& row : unifiedRows)
        {
            auto* iface = dynamic_cast<UnifiedRowInterface*> (row.get());
            const int h = iface != nullptr ? iface->getPreferredHeight (contentWidth) : rowHeight;
            row->setVisible (true);
            row->setBounds (0, y, contentWidth, h);
            y += h + gapBetweenRows;
        }
    }
    else
    {
        // Per gruppi (vedi CsoundParameterGroups.h): intestazione, poi le
        // righe del gruppo; chiuso = righe nascoste.
        for (auto& group : displayGroups)
        {
            // Parametri + card del Monitor del gruppo, nell'ordine del codice.
            struct Item { int order; juce::Component* comp; MonitorRow* monitor; };
            std::vector<Item> items;
            for (size_t i = 0; i < group.rows.size(); ++i)
                items.push_back ({ i < group.rowOrders.size() ? group.rowOrders[i] : (int) i, group.rows[i], nullptr });
            for (auto& m : monitorRows)
                if (m->groupName == group.name)
                    items.push_back ({ m->order, m.get(), m.get() });
            std::stable_sort (items.begin(), items.end(), [] (const Item& a, const Item& b) { return a.order < b.order; });

            // Gruppo con sole card del Monitor non ancora scritte (motore fermo): nascosto.
            if (items.empty())
            {
                group.header->setVisible (false);
                continue;
            }
            group.header->setVisible (true);
            group.header->count = (int) items.size();

            const int groupTop = y;
            const int innerWidth = contentWidth - groupInsetH * 2;
            y += groupHeaderHeight;

            if (! group.header->collapsed)
            {
                y += gapBetweenRows;

                for (auto& item : items)
                {
                    int h = rowHeight;
                    if (item.monitor != nullptr)
                        h = item.monitor->preferredHeight();
                    else if (auto* iface = dynamic_cast<UnifiedRowInterface*> (item.comp))
                        h = iface->getPreferredHeight (innerWidth);
                    item.comp->setVisible (true);
                    item.comp->setBounds (groupInsetH, y, innerWidth, h);
                    y += h + gapBetweenRows;
                }

                y += groupInsetBottom - gapBetweenRows;
            }
            else
            {
                for (auto& item : items)
                    item.comp->setVisible (false);
            }

            group.header->setBounds (0, groupTop, contentWidth, y - groupTop);
            y += groupGap;
        }
    }

    if (pendingRow != nullptr)
    {
        auto* iface = dynamic_cast<UnifiedRowInterface*> (pendingRow.get());
        const int h = iface != nullptr ? iface->getPreferredHeight (contentWidth) : rowHeight;
        pendingRow->setBounds (0, y, contentWidth, h);
        y += h + gapBetweenRows;
    }

    // Sezione Monitor in coda (vedi refreshMonitorList): solo le card che
    // non stanno in un gruppo dei parametri.
    int ungroupedMonitor = 0;
    for (auto& row : monitorRows)
        if (row->groupName.isEmpty())
            ++ungroupedMonitor;

    if (monitorHeader != nullptr)
        monitorHeader->setVisible (ungroupedMonitor > 0);

    if (monitorHeader != nullptr && ungroupedMonitor > 0)
    {
        monitorHeader->count = ungroupedMonitor;
        const int groupTop = y;
        const int innerWidth = contentWidth - groupInsetH * 2;
        y += groupHeaderHeight;

        if (! monitorHeader->collapsed)
        {
            y += gapBetweenRows;
            for (auto& row : monitorRows)
            {
                if (row->groupName.isNotEmpty())
                    continue;
                const int h = row->preferredHeight();
                row->setVisible (true);
                row->setBounds (groupInsetH, y, innerWidth, h);
                y += h + gapBetweenRows;
            }
            y += groupInsetBottom - gapBetweenRows;
        }
        else
        {
            for (auto& row : monitorRows)
                if (row->groupName.isEmpty())
                    row->setVisible (false);
        }

        monitorHeader->setBounds (0, groupTop, contentWidth, y - groupTop);
        y += groupGap;
    }

    rowsContainer.setSize (contentWidth, y);
}
