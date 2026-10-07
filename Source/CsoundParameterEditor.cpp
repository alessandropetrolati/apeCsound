#include "CsoundParameterEditor.h"

namespace
{
    // Tema SCURO dedicato a questo pannello (stesso spirito della console
    // di log in PluginEditor, che e' gia' scura): il resto dell'app usa un
    // tema chiaro (CsoundLookAndFeel) sotto cui il testo nero di default di
    // TextEditor/ComboBox risultava illeggibile su alcuni sfondi/stati -
    // qui i colori vengono impostati ESPLICITAMENTE su ogni componente
    // (TextEditor/ComboBox non ereditano piu' nulla dalla LookAndFeel
    // condivisa per i colori che contano), cosi' il contrasto e' garantito
    // a prescindere dal tema globale.
    const juce::Colour kPanelBg      { 0xff10181f }; // come logConsole in PluginEditor
    const juce::Colour kTitleBarBg   { 0xff0a1016 };
    const juce::Colour kFieldBg      { 0xff202a33 };
    const juce::Colour kFieldOutline { 0xff3a4550 };
    const juce::Colour kAccent       { 0xff17a2b8 }; // stesso accento teal del resto dell'app
    const juce::Colour kText         { 0xffe8eef1 };
    const juce::Colour kTextMuted    { 0xff8a9aa5 };
    const juce::Colour kPlaceholder  { 0xff5b6b74 };

    void applyDarkFieldColours (juce::TextEditor& field)
    {
        field.setColour (juce::TextEditor::backgroundColourId,     kFieldBg);
        field.setColour (juce::TextEditor::textColourId,           kText);
        field.setColour (juce::TextEditor::outlineColourId,        kFieldOutline);
        field.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
        field.setColour (juce::TextEditor::highlightColourId,      kAccent.withAlpha (0.35f));
        field.setColour (juce::TextEditor::highlightedTextColourId, kText);
    }

    void applyDarkComboColours (juce::ComboBox& combo)
    {
        combo.setColour (juce::ComboBox::backgroundColourId, kFieldBg);
        combo.setColour (juce::ComboBox::textColourId,       kText);
        combo.setColour (juce::ComboBox::outlineColourId,    kFieldOutline);
        combo.setColour (juce::ComboBox::arrowColourId,      kTextMuted);

        // Colori del menu a tendina (PopupMenu): la LookAndFeel condivisa
        // non li imposta (resta sul tema chiaro per il resto dell'app), ma
        // li cerca comunque risalendo dal ComboBox che l'ha aperto - basta
        // impostarli qui.
        combo.setColour (juce::PopupMenu::backgroundColourId,          kFieldBg);
        combo.setColour (juce::PopupMenu::textColourId,                kText);
        combo.setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
        combo.setColour (juce::PopupMenu::highlightedTextColourId,     juce::Colours::white);
    }

    // Header di colonna condiviso da tutte e tre le pagine: un juce::Label
    // per colonna (non una sola stringa con spazi a mano, che non restava
    // allineata ai campi sotto), stile comune centralizzato qui.
    void setupHeaderLabel (juce::Label& label, const juce::String& text,
                            juce::Justification justification = juce::Justification::centredLeft)
    {
        label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        label.setColour (juce::Label::textColourId, kTextMuted);
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (justification);
    }

    // Stesso formato ESATTO che CsoundCodeEditor::itemDropped inserisce
    // trascinando la maniglia "#N" sull'editor di codice (vedi li'), MENO
    // l'indentazione (che dipende da dove l'utente incollera' il testo,
    // non prevedibile qui) - usato dal tasto destro sulla maniglia di
    // ciascuna riga (Param/Int/Bool/ChoiceParamRow::mouseDown) per copiare
    // negli appunti senza dover trascinare fisicamente sull'editor.
    juce::String makeChngetClipboardText (const juce::String& channelName)
    {
        const auto varName = "k" + channelName.removeCharacters (" \t");
        return varName + " chnget \"" + channelName + "\"\n";
    }

    // Menu contestuale "Copy" mostrato dal tasto destro sulla maniglia -
    // su richiesta esplicita l'azione NON e' piu' automatica al solo
    // right-click: deve comparire un vero menu con una voce "Copy" da
    // selezionare, cosi' il right-click da solo non fa nulla (nessuna
    // sorpresa, coerente con la convenzione di qualunque altro programma).
    // channelName vuoto -> voce disabilitata (niente da copiare).
    // notifyResult riceve il messaggio da mostrare in consolle SOLO se
    // l'utente seleziona davvero "Copy" (non se chiude il menu senza
    // scegliere nulla).
    void showCopyChngetMenu (const juce::String& channelName,
                              std::function<void (const juce::String&)> notifyResult)
    {
        juce::PopupMenu menu;
        constexpr int copyItemId = 1;
        menu.addItem (copyItemId, "Copy", channelName.isNotEmpty());

        menu.showMenuAsync (juce::PopupMenu::Options(),
            [channelName, notifyResult] (int result)
            {
                if (result != copyItemId)
                    return; // menu chiuso senza scegliere "Copy"

                const auto clip = makeChngetClipboardText (channelName);
                juce::SystemClipboard::copyTextToClipboard (clip);

                if (notifyResult)
                    notifyResult ("--- Copiato negli appunti: " + clip.trim() + " ---");
            });
    }

    // Stesso identico path SVG di CsoundLookAndFeel's makeTuneIconPath (il
    // bottone "Parameters" nella toolbar) - duplicato qui invece di
    // condiviso: e' una singola riga, non vale un header apposito solo per
    // questo. Usata per la tab "UI" su richiesta esplicita ("usa la stessa
    // che hai usato per Parameters"), invece di inventare un'icona nuova.
    juce::Path makeTuneIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M3,17V19H9V17H3M3,5V7H13V5H3M13,21V19H21V17H13V15H11V21H13M7,9V11H3V13H7V15H9V9H7M21,"
            "13V11H11V13H21M15,9H17V7H21V5H17V3H15V9Z");
    }
}

//==============================================================================
// CsoundParameterPanelLookAndFeel - vedi il commento in CsoundParameterEditor.h.
// Niente "piatto": bordi arrotondati ovunque, un alone (glow) accentato
// sui campi col focus/sotto il mouse, freccia del combo disegnata a mano -
// il tocco "cool" che differenzia questo pannello dal resto, chiaro e
// squadrato, dell'app.
CsoundParameterPanelLookAndFeel::CsoundParameterPanelLookAndFeel()
{
    setColour (juce::ScrollBar::thumbColourId, kAccent.withAlpha (0.55f));
    setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId,            kFieldBg);
    setColour (juce::PopupMenu::textColourId,                  kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

    // Colori comuni a QUALUNQUE componente "di serie" di JUCE che finisca
    // dentro un pannello con questa LookAndFeel (non solo TextEditor/
    // ComboBox dei field custom sopra, che hanno gia' i loro setColour()
    // per-istanza): servono soprattutto a GenericEditorWindow (vedi
    // PluginEditor.h), che usa Slider/ToggleButton/ComboBox "di serie" -
    // senza questi colori qui, quei controlli ereditano il tema CHIARO di
    // CsoundLookAndFeel (il resto dell'app) e diventano illeggibili/
    // invisibili su questo sfondo scuro (es. il tick del ToggleButton che
    // "non si vede" segnalato).
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
        // Alone accentato: due tratti concentrici a opacita' decrescente
        // invece di un bordo secco - da' un effetto di "luce" morbida
        // intorno al campo attivo.
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

    // Piccolo chevron centrato nel pulsante freccia (buttonX/Y/W/H, passati
    // da JUCE - non vanno ignorati: usarli e' quello che evita una freccia
    // enorme alta quanto l'intero combo, il bug della versione precedente).
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
    // Niente angoli arrotondati qui: la finestra nativa del PopupMenu resta
    // comunque un rettangolo squadrato e opaco (bianco) sotto di noi - un
    // fillRoundedRectangle lascia scoperti i 4 angoli, che e' esattamente il
    // difetto "si vede il bianco sotto l'angolo arrotondato" segnalato.
    // Riempiendo l'intero rettangolo non resta alcun pixel scoperto; il tocco
    // "cool" del pannello resta nell'evidenziazione arrotondata delle singole
    // voci (vedi drawPopupMenuItem), non nel contorno del menu stesso.
    g.setColour (findColour (juce::PopupMenu::backgroundColourId));
    g.fillAll();
    g.setColour (kFieldOutline);
    g.drawRect (0, 0, width, height, 1);
}

void CsoundParameterPanelLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                                                   int standardMenuItemHeight, int& idealWidth, int& idealHeight)
{
    juce::LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);

    // Un po' piu' alte delle righe di default: piu' respiro, piu' facili
    // da centrare col mouse - coerente con l'altezza dei campi del pannello.
    if (! isSeparator)
        idealHeight = juce::jmax (idealHeight, 26);
}

void CsoundParameterPanelLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                                           bool isSeparator, bool isActive, bool isHighlighted,
                                                           bool isTicked, bool hasSubMenu, const juce::String& text,
                                                           const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                                           const juce::Colour* textColour)
{
    juce::ignoreUnused (icon); // nessuna voce di questo pannello usa un'icona

    if (isSeparator)
    {
        auto line = area.reduced (10, 0);
        g.setColour (kFieldOutline);
        g.drawLine ((float) line.getX(), (float) line.getCentreY(), (float) line.getRight(), (float) line.getCentreY(), 1.0f);
        return;
    }

    auto itemArea = area.reduced (4, 1);

    // Riquadro pieno a spigoli vivi dietro la voce evidenziata (hover/
    // selezione da tastiera) - stesso linguaggio visivo squadrato del
    // resto del pannello (handle di drag, bordo dei campi, ecc.).
    if (isHighlighted && isActive)
    {
        g.setColour (kAccent);
        g.fillRect (itemArea);
    }

    auto textArea = itemArea.reduced (10, 0);

    if (isTicked)
    {
        // Spunta minimale (non l'icona di sistema): un segno di spunta
        // disegnato a mano, stesso stile accentato. Dimensione FISSA e
        // centrata verticalmente nella riga (non tickArea.getBottom()/getY()
        // dell'intera riga, che la allungava quanto l'altezza della voce -
        // il bug della "spunta enorme" segnalato).
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
        // Non usato in questo pannello (nessun sottomenu/scorciatoia nei
        // combo "Curva"), ma lasciato per correttezza se in futuro servisse.
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

    // Niente binario/frecce disegnati: solo un thumb sottile a spigoli
    // vivi, inset di 2px dai bordi, che si accende leggermente al
    // passaggio/pressione del mouse - stile minimale coerente col resto
    // del pannello.
    auto thumbBounds = isScrollbarVertical
                            ? juce::Rectangle<int> (x + 2, thumbStartPosition, juce::jmax (2, width - 4), thumbSize)
                            : juce::Rectangle<int> (thumbStartPosition, y + 2, thumbSize, juce::jmax (2, height - 4));

    const auto alpha = isMouseDown ? 0.85f : (isMouseOver ? 0.65f : 0.45f);
    g.setColour (kAccent.withAlpha (alpha));
    g.fillRect (thumbBounds);
}

void CsoundParameterPanelLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    // Solo la tab "UI" ha un'icona - le altre quattro (Float/Int/Bool/
    // Choice) restano col rendering di testo standard di LookAndFeel_V4.
    if (button.getName() != "genericEditorTab")
    {
        juce::LookAndFeel_V4::drawButtonText (g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        return;
    }

    auto textColour = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                                   : juce::TextButton::textColourOffId);
    g.setColour (textColour);

    auto bounds = button.getLocalBounds().toFloat().reduced (8.0f, 0.0f);
    auto icon = makeTuneIconPath();

    const float iconSize = juce::jmin (bounds.getHeight() * 0.55f, 15.0f);
    auto iconArea = bounds.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize);
    icon.scaleToFit (iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true);
    g.fillPath (icon);

    bounds.removeFromLeft (6.0f);

    g.setFont (juce::Font (juce::FontOptions (juce::jmin (14.0f, (float) button.getHeight() * 0.5f), juce::Font::bold)));
    g.drawFittedText (button.getButtonText(), bounds.toNearestInt(), juce::Justification::centredLeft, 1);
}

//==============================================================================
CsoundParameterMappingPanel::ParamRow::ParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex)
    : processor (processorToEdit),
      index (slotIndex)
{
    // Niente juce::Label per "#N": e' disegnato direttamente in paint(),
    // nell'area handleBounds (calcolata in resized()), che mouseDown/
    // mouseDrag usano per riconoscere un trascinamento - vedi il commento
    // sulla maniglia in CsoundParameterEditor.h. Mostrata SOLO se il canale
    // ha gia' un nome (vedi paint()): trascinare uno slot vuoto non ha
    // senso, quindi la maniglia non compare finche' non c'e' un nome.
    setMouseCursor (juce::MouseCursor::NormalCursor);

    // Senza questo, JUCE porta automaticamente il focus da tastiera sul
    // primo figlio focalizzabile (channelNameEditor) ad OGNI click sulla
    // riga - incluso un click sulla maniglia "#N", che e' un'area
    // disegnata dentro ParamRow stesso, non un componente figlio separato
    // (vedi Component::internalMouseDown/grabKeyboardFocusInternal in
    // JUCE: chiama sempre grabKeyboardFocusInternal su "this" PRIMA del
    // nostro mouseDown, e se "this" - cioe' ParamRow - non vuole il focus,
    // JUCE lo passa al default child, trovando channelNameEditor). Risultato
    // indesiderato: cliccare la maniglia (sx per trascinare, dx per il
    // menu Copy) faceva anche entrare il campo nome in editing. Disattivato
    // qui: il focus da tastiera arriva SOLO cliccando direttamente su un
    // campo (channelNameEditor/minEditor/ecc, che gestiscono il proprio
    // grabKeyboardFocus perche' il click arriva a LORO come "this", non a
    // ParamRow), esattamente come richiesto.
    setMouseClickGrabsKeyboardFocus (false);

    // "Rename": qui si da' al parametro il suo nome VERO, cioe' il nome del
    // canale Csound (chnget) a cui e' agganciato - l'host continua a
    // vedere sempre "Param N" (vedi CsoundAudioProcessor::createChannelParamLayout),
    // cosi' l'automazione resta stabile anche rinominando/riassegnando i canali.
    channelNameEditor.setTextToShowWhenEmpty ("(no channel)", kPlaceholder);
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    minEditor.setInputRestrictions (0, "0123456789.,-eE");
    minEditor.setJustification (juce::Justification::centredRight);
    minEditor.addListener (this);
    applyDarkFieldColours (minEditor);
    addAndMakeVisible (minEditor);

    maxEditor.setInputRestrictions (0, "0123456789.,-eE");
    maxEditor.setJustification (juce::Justification::centredRight);
    maxEditor.addListener (this);
    applyDarkFieldColours (maxEditor);
    addAndMakeVisible (maxEditor);

    defaultEditor.setInputRestrictions (0, "0123456789.,-eE");
    defaultEditor.setJustification (juce::Justification::centredRight);
    defaultEditor.addListener (this);
    applyDarkFieldColours (defaultEditor);
    addAndMakeVisible (defaultEditor);

    // Skew (juce::Slider::setSkewFactor - 1.0 = lineare) e increment (passo
    // di quantizzazione in unita' reali) al posto del vecchio menu a
    // tendina Linear/Exponential/Logarithmic: due numeri, niente curve
    // predefinite da scegliere da una lista.
    skewEditor.setInputRestrictions (0, "0123456789.,-eE");
    skewEditor.setJustification (juce::Justification::centredRight);
    skewEditor.addListener (this);
    applyDarkFieldColours (skewEditor);
    addAndMakeVisible (skewEditor);

    incrementEditor.setInputRestrictions (0, "0123456789.,-eE");
    incrementEditor.setJustification (juce::Justification::centredRight);
    incrementEditor.addListener (this);
    applyDarkFieldColours (incrementEditor);
    addAndMakeVisible (incrementEditor);

    refreshFromProcessor();
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

    // getText().getFloatValue() torna 0 anche su testo non numerico: per un
    // campo vuoto o illeggibile manteniamo il range di default (0..1)
    // invece di forzare silenziosamente 0, cosi' un errore di digitazione
    // non trasforma il range in un punto singolo senza che l'utente se ne
    // accorga subito.
    const auto minText = minEditor.getText().trim();
    const auto maxText = maxEditor.getText().trim();
    const auto defaultText = defaultEditor.getText().trim();
    slot.minValue = minText.isNotEmpty() ? minText.getFloatValue() : 0.0f;
    slot.maxValue = maxText.isNotEmpty() ? maxText.getFloatValue() : 1.0f;
    slot.defaultValue = defaultText.isNotEmpty() ? defaultText.getFloatValue() : slot.minValue;

    // Stesso principio: un campo vuoto o illeggibile mantiene il default
    // (skew=1.0 lineare, increment=0.001) invece di forzare silenziosamente
    // 0 (che per skew non avrebbe nemmeno senso - vedi denormalizeChannelParam).
    const auto skewText = skewEditor.getText().trim();
    const auto incrementText = incrementEditor.getText().trim();
    slot.skew = skewText.isNotEmpty() ? skewText.getFloatValue() : 1.0f;
    slot.increment = incrementText.isNotEmpty() ? incrementText.getFloatValue() : 0.001f;

    processor.setChannelParamSlot (index, slot);

    // La maniglia di trascinamento compare/scompare in base al nome canale
    // (vedi paint()): va ridisegnata subito, non solo alla prossima
    // occasione di repaint.
    repaint();
}

void CsoundParameterMappingPanel::ParamRow::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    commitFromFields();
    editor.giveAwayKeyboardFocus();
}

void CsoundParameterMappingPanel::ParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
}

void CsoundParameterMappingPanel::ParamRow::textEditorTextChanged (juce::TextEditor&)
{
    // Commit ad ogni carattere (non solo a Return/focus perso): cosi' la
    // maniglia di trascinamento compare/scompare SUBITO mentre si digita o
    // si cancella il nome canale, non solo dopo aver confermato - vedi
    // paint() e il commento sulla maniglia in CsoundParameterEditor.h.
    commitFromFields();
}

void CsoundParameterMappingPanel::ParamRow::paint (juce::Graphics& g)
{
    // Righe "zebrate" (sfondo leggermente alternato ogni due) cosi' e' piu'
    // facile seguire una riga su 32 a colpo d'occhio, invece di un unico
    // sfondo scuro uniforme - tocco "cool" in piu' rispetto a prima.
    if ((index % 2) == 1)
    {
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        g.fillRect (getLocalBounds());
    }

    // La maniglia (riquadro + griglia di pallini) compare SOLO se il
    // canale ha gia' un nome: trascinare uno slot vuoto produrrebbe un
    // chnget senza nome, quindi non ha senso offrirla prima.
    if (processor.getChannelParamSlot (index).channelName.isEmpty())
        return;

    // Riquadro ben visibile (non solo testo/icona su sfondo trasparente):
    // bordo accentato sempre presente, riempimento che si accende
    // ulteriormente al passaggio del mouse - cosi' e' inequivocabile che
    // e' un controllo trascinabile, non un'etichetta.
    auto box = handleBounds.toFloat().reduced (2.0f);
    g.setColour (kAccent.withAlpha (handleHovered ? 0.45f : 0.22f));
    g.fillRect (box);
    g.setColour (kAccent.withAlpha (handleHovered ? 1.0f : 0.75f));
    g.drawRect (box, 1.2f);

    // Griglia 2x3 di pallini grandi e ben contrastati - la classica icona
    // "grip"/maniglia di trascinamento, immediatamente riconoscibile.
    g.setColour (handleHovered ? juce::Colours::white : kText);
    const auto cx = box.getCentreX();
    const auto cy = box.getCentreY();
    constexpr float dotSize = 3.4f;
    constexpr float dotSpacingX = 6.5f;
    constexpr float dotSpacingY = 6.0f;
    for (int row = -1; row <= 1; ++row)
    {
        for (int col = 0; col <= 1; ++col)
        {
            const float dx = (col == 0 ? -1.0f : 1.0f) * (dotSpacingX / 2.0f);
            const float dy = (float) row * dotSpacingY;
            g.fillEllipse (cx + dx - dotSize / 2.0f, cy + dy - dotSize / 2.0f, dotSize, dotSize);
        }
    }
}

void CsoundParameterMappingPanel::ParamRow::resized()
{
    juce::Rectangle<int> name, min, max, defaultVal, skewIncrement;
    CsoundParameterMappingPanel::layoutColumns (getLocalBounds().reduced (4, 2), handleBounds, name, min, max, defaultVal, skewIncrement);

    channelNameEditor.setBounds (name);
    minEditor.setBounds (min);
    maxEditor.setBounds (max);
    defaultEditor.setBounds (defaultVal);

    // skewIncrement e' un'unica colonna condivisa dai due campi: divisa a
    // meta' con un piccolo margine fra loro.
    auto skewArea = skewIncrement.removeFromLeft (skewIncrement.getWidth() / 2 - 3);
    skewIncrement.removeFromLeft (6);
    skewEditor.setBounds (skewArea);
    incrementEditor.setBounds (skewIncrement);
}

void CsoundParameterMappingPanel::ParamRow::mouseDown (const juce::MouseEvent& event)
{
    // Tasto destro sulla maniglia: apre un menu con la voce "Copy" (NON
    // copia automaticamente al solo right-click) - selezionandola copia
    // negli appunti lo stesso chnget che trascinare la maniglia sull'editor
    // inserirebbe (vedi makeChngetClipboardText/showCopyChngetMenu), senza
    // dover trascinare fisicamente.
    if (event.mods.isPopupMenu() && handleBounds.contains (event.getPosition()))
    {
        showCopyChngetMenu (processor.getChannelParamSlot (index).channelName, onCopiedToClipboard);
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

    // Si trascina solo se lo slot ha gia' un canale assegnato: trascinare
    // uno slot vuoto produrrebbe un "chnget" senza nome, inutile.
    const auto slot = processor.getChannelParamSlot (index);
    if (slot.channelName.isEmpty())
        return;

    container->startDragging ("csoundChannel:" + slot.channelName, this);
}

//==============================================================================
void CsoundParameterMappingPanel::layoutColumns (juce::Rectangle<int> area,
                                                   juce::Rectangle<int>& handle,
                                                   juce::Rectangle<int>& name,
                                                   juce::Rectangle<int>& min,
                                                   juce::Rectangle<int>& max,
                                                   juce::Rectangle<int>& defaultVal,
                                                   juce::Rectangle<int>& skewIncrement)
{
    handle = area.removeFromLeft (handleWidth);
    area.removeFromLeft (4);

    skewIncrement = area.removeFromRight (curveWidth);
    area.removeFromRight (6);

    defaultVal = area.removeFromRight (defaultWidth);
    area.removeFromRight (6);

    max = area.removeFromRight (minMaxWidth);
    area.removeFromRight (6);

    min = area.removeFromRight (minMaxWidth);
    area.removeFromRight (6);

    // Il nome prende TUTTO lo spazio che resta, fino al campo min (non una
    // larghezza fissa): cosi' la riga - e quindi l'intera pagina - scala
    // con la larghezza della finestra del plugin invece di lasciare spazio
    // vuoto o tagliare il nome.
    name = area;
}

void CsoundParameterMappingPanel::layoutIntColumns (juce::Rectangle<int> area,
                                                      juce::Rectangle<int>& handle,
                                                      juce::Rectangle<int>& name,
                                                      juce::Rectangle<int>& min,
                                                      juce::Rectangle<int>& max,
                                                      juce::Rectangle<int>& defaultVal)
{
    handle = area.removeFromLeft (handleWidth);
    area.removeFromLeft (4);

    defaultVal = area.removeFromRight (defaultWidth);
    area.removeFromRight (6);

    max = area.removeFromRight (minMaxWidth);
    area.removeFromRight (6);

    min = area.removeFromRight (minMaxWidth);
    area.removeFromRight (6);

    name = area;
}

void CsoundParameterMappingPanel::layoutBoolColumns (juce::Rectangle<int> area,
                                                       juce::Rectangle<int>& handle,
                                                       juce::Rectangle<int>& name,
                                                       juce::Rectangle<int>& defaultVal)
{
    handle = area.removeFromLeft (handleWidth);
    area.removeFromLeft (4);

    defaultVal = area.removeFromRight (boolDefaultWidth);
    area.removeFromRight (6);

    name = area;
}

void CsoundParameterMappingPanel::layoutChoiceColumns (juce::Rectangle<int> area,
                                                         juce::Rectangle<int>& handle,
                                                         juce::Rectangle<int>& name,
                                                         juce::Rectangle<int>& options,
                                                         juce::Rectangle<int>& defaultIndex)
{
    handle = area.removeFromLeft (handleWidth);
    area.removeFromLeft (4);

    defaultIndex = area.removeFromRight (choiceDefaultIndexWidth);
    area.removeFromRight (6);

    options = area.removeFromRight (choiceOptionsWidth);
    area.removeFromRight (6);

    name = area;
}

//==============================================================================
CsoundParameterMappingPanel::IntParamRow::IntParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex)
    : processor (processorToEdit),
      index (slotIndex)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);

    // Vedi il commento identico su ParamRow::ParamRow sopra: senza questo
    // il click sulla maniglia porterebbe il focus su channelNameEditor.
    setMouseClickGrabsKeyboardFocus (false);

    channelNameEditor.setTextToShowWhenEmpty ("(no channel)", kPlaceholder);
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    minEditor.setInputRestrictions (0, "0123456789-");
    minEditor.setJustification (juce::Justification::centredRight);
    minEditor.addListener (this);
    applyDarkFieldColours (minEditor);
    addAndMakeVisible (minEditor);

    maxEditor.setInputRestrictions (0, "0123456789-");
    maxEditor.setJustification (juce::Justification::centredRight);
    maxEditor.addListener (this);
    applyDarkFieldColours (maxEditor);
    addAndMakeVisible (maxEditor);

    defaultEditor.setInputRestrictions (0, "0123456789-");
    defaultEditor.setJustification (juce::Justification::centredRight);
    defaultEditor.addListener (this);
    applyDarkFieldColours (defaultEditor);
    addAndMakeVisible (defaultEditor);

    refreshFromProcessor();
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

    repaint();
}

void CsoundParameterMappingPanel::IntParamRow::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    commitFromFields();
    editor.giveAwayKeyboardFocus();
}

void CsoundParameterMappingPanel::IntParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
}

void CsoundParameterMappingPanel::IntParamRow::textEditorTextChanged (juce::TextEditor&)
{
    commitFromFields();
}

void CsoundParameterMappingPanel::IntParamRow::paint (juce::Graphics& g)
{
    if ((index % 2) == 1)
    {
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        g.fillRect (getLocalBounds());
    }

    if (processor.getIntParamSlot (index).channelName.isEmpty())
        return;

    auto box = handleBounds.toFloat().reduced (2.0f);
    g.setColour (kAccent.withAlpha (handleHovered ? 0.45f : 0.22f));
    g.fillRect (box);
    g.setColour (kAccent.withAlpha (handleHovered ? 1.0f : 0.75f));
    g.drawRect (box, 1.2f);

    g.setColour (handleHovered ? juce::Colours::white : kText);
    const auto cx = box.getCentreX();
    const auto cy = box.getCentreY();
    constexpr float dotSize = 3.4f;
    constexpr float dotSpacingX = 6.5f;
    constexpr float dotSpacingY = 6.0f;
    for (int row = -1; row <= 1; ++row)
    {
        for (int col = 0; col <= 1; ++col)
        {
            const float dx = (col == 0 ? -1.0f : 1.0f) * (dotSpacingX / 2.0f);
            const float dy = (float) row * dotSpacingY;
            g.fillEllipse (cx + dx - dotSize / 2.0f, cy + dy - dotSize / 2.0f, dotSize, dotSize);
        }
    }
}

void CsoundParameterMappingPanel::IntParamRow::resized()
{
    juce::Rectangle<int> name, min, max, defaultVal;
    CsoundParameterMappingPanel::layoutIntColumns (getLocalBounds().reduced (4, 2), handleBounds, name, min, max, defaultVal);

    channelNameEditor.setBounds (name);
    minEditor.setBounds (min);
    maxEditor.setBounds (max);
    defaultEditor.setBounds (defaultVal);
}

void CsoundParameterMappingPanel::IntParamRow::mouseDown (const juce::MouseEvent& event)
{
    // Vedi il commento identico in ParamRow::mouseDown sopra.
    if (event.mods.isPopupMenu() && handleBounds.contains (event.getPosition()))
    {
        showCopyChngetMenu (processor.getIntParamSlot (index).channelName, onCopiedToClipboard);
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

    container->startDragging ("csoundChannel:" + slot.channelName, this);
}

//==============================================================================
CsoundParameterMappingPanel::BoolParamRow::BoolParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex)
    : processor (processorToEdit),
      index (slotIndex)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);

    // Vedi il commento identico su ParamRow::ParamRow sopra: senza questo
    // il click sulla maniglia porterebbe il focus su channelNameEditor.
    setMouseClickGrabsKeyboardFocus (false);

    channelNameEditor.setTextToShowWhenEmpty ("(no channel)", kPlaceholder);
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    defaultToggle.setColour (juce::ToggleButton::textColourId, kText);
    defaultToggle.setColour (juce::ToggleButton::tickColourId, kAccent);
    defaultToggle.setColour (juce::ToggleButton::tickDisabledColourId, kFieldOutline);
    defaultToggle.onClick = [this] { commitFromFields(); };
    addAndMakeVisible (defaultToggle);

    refreshFromProcessor();
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
    repaint();
}

void CsoundParameterMappingPanel::BoolParamRow::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    commitFromFields();
    editor.giveAwayKeyboardFocus();
}

void CsoundParameterMappingPanel::BoolParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
}

void CsoundParameterMappingPanel::BoolParamRow::textEditorTextChanged (juce::TextEditor&)
{
    commitFromFields();
}

void CsoundParameterMappingPanel::BoolParamRow::paint (juce::Graphics& g)
{
    if ((index % 2) == 1)
    {
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        g.fillRect (getLocalBounds());
    }

    if (processor.getBoolParamSlot (index).channelName.isEmpty())
        return;

    auto box = handleBounds.toFloat().reduced (2.0f);
    g.setColour (kAccent.withAlpha (handleHovered ? 0.45f : 0.22f));
    g.fillRect (box);
    g.setColour (kAccent.withAlpha (handleHovered ? 1.0f : 0.75f));
    g.drawRect (box, 1.2f);

    g.setColour (handleHovered ? juce::Colours::white : kText);
    const auto cx = box.getCentreX();
    const auto cy = box.getCentreY();
    constexpr float dotSize = 3.4f;
    constexpr float dotSpacingX = 6.5f;
    constexpr float dotSpacingY = 6.0f;
    for (int row = -1; row <= 1; ++row)
    {
        for (int col = 0; col <= 1; ++col)
        {
            const float dx = (col == 0 ? -1.0f : 1.0f) * (dotSpacingX / 2.0f);
            const float dy = (float) row * dotSpacingY;
            g.fillEllipse (cx + dx - dotSize / 2.0f, cy + dy - dotSize / 2.0f, dotSize, dotSize);
        }
    }
}

void CsoundParameterMappingPanel::BoolParamRow::resized()
{
    juce::Rectangle<int> name, defaultArea;
    CsoundParameterMappingPanel::layoutBoolColumns (getLocalBounds().reduced (4, 2), handleBounds, name, defaultArea);

    channelNameEditor.setBounds (name);
    defaultToggle.setBounds (defaultArea);
}

void CsoundParameterMappingPanel::BoolParamRow::mouseDown (const juce::MouseEvent& event)
{
    // Vedi il commento identico in ParamRow::mouseDown sopra.
    if (event.mods.isPopupMenu() && handleBounds.contains (event.getPosition()))
    {
        showCopyChngetMenu (processor.getBoolParamSlot (index).channelName, onCopiedToClipboard);
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

    container->startDragging ("csoundChannel:" + slot.channelName, this);
}

//==============================================================================
CsoundParameterMappingPanel::ChoiceParamRow::ChoiceParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex)
    : processor (processorToEdit),
      index (slotIndex)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);

    // Vedi il commento identico su ParamRow::ParamRow sopra: senza questo
    // il click sulla maniglia porterebbe il focus su channelNameEditor.
    setMouseClickGrabsKeyboardFocus (false);

    channelNameEditor.setTextToShowWhenEmpty ("(no channel)", kPlaceholder);
    channelNameEditor.addListener (this);
    applyDarkFieldColours (channelNameEditor);
    addAndMakeVisible (channelNameEditor);

    optionsEditor.setTextToShowWhenEmpty ("opt1, opt2, opt3...", kPlaceholder);
    optionsEditor.addListener (this);
    applyDarkFieldColours (optionsEditor);
    addAndMakeVisible (optionsEditor);

    defaultIndexEditor.setInputRestrictions (0, "0123456789");
    defaultIndexEditor.setJustification (juce::Justification::centredRight);
    defaultIndexEditor.addListener (this);
    applyDarkFieldColours (defaultIndexEditor);
    addAndMakeVisible (defaultIndexEditor);

    refreshFromProcessor();
}

void CsoundParameterMappingPanel::ChoiceParamRow::refreshFromProcessor()
{
    const auto slot = processor.getChoiceParamSlot (index);
    channelNameEditor.setText (slot.channelName, false);
    optionsEditor.setText (slot.optionLabels.joinIntoString (", "), false);
    defaultIndexEditor.setText (juce::String (slot.defaultIndex), false);
}

void CsoundParameterMappingPanel::ChoiceParamRow::commitFromFields()
{
    CsoundAudioProcessor::ChoiceParamSlot slot;
    slot.channelName = channelNameEditor.getText().trim();

    // Etichette separate da virgole, spazi ai bordi tolti, voci vuote
    // ignorate (es. "a,,b" -> "a","b", non "a","","b"): oltre
    // maxChoiceOptions vengono scartate, perche' l'host vede comunque sempre
    // esattamente quel numero di opzioni (vedi ChoiceHostParameter) - non
    // avrebbero un indice raggiungibile.
    juce::StringArray parsed;
    parsed.addTokens (optionsEditor.getText(), ",", "");
    for (auto& option : parsed)
        option = option.trim();
    parsed.removeEmptyStrings();
    while (parsed.size() > CsoundAudioProcessor::maxChoiceOptions)
        parsed.remove (parsed.size() - 1);
    slot.optionLabels = parsed;

    const auto defaultText = defaultIndexEditor.getText().trim();
    slot.defaultIndex = juce::jlimit (0, CsoundAudioProcessor::maxChoiceOptions - 1,
                                       defaultText.isNotEmpty() ? defaultText.getIntValue() : 0);

    processor.setChoiceParamSlot (index, slot);
    repaint();
}

void CsoundParameterMappingPanel::ChoiceParamRow::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    commitFromFields();
    editor.giveAwayKeyboardFocus();
}

void CsoundParameterMappingPanel::ChoiceParamRow::textEditorFocusLost (juce::TextEditor&)
{
    commitFromFields();
}

void CsoundParameterMappingPanel::ChoiceParamRow::textEditorTextChanged (juce::TextEditor&)
{
    commitFromFields();
}

void CsoundParameterMappingPanel::ChoiceParamRow::paint (juce::Graphics& g)
{
    if ((index % 2) == 1)
    {
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        g.fillRect (getLocalBounds());
    }

    if (processor.getChoiceParamSlot (index).channelName.isEmpty())
        return;

    auto box = handleBounds.toFloat().reduced (2.0f);
    g.setColour (kAccent.withAlpha (handleHovered ? 0.45f : 0.22f));
    g.fillRect (box);
    g.setColour (kAccent.withAlpha (handleHovered ? 1.0f : 0.75f));
    g.drawRect (box, 1.2f);

    g.setColour (handleHovered ? juce::Colours::white : kText);
    const auto cx = box.getCentreX();
    const auto cy = box.getCentreY();
    constexpr float dotSize = 3.4f;
    constexpr float dotSpacingX = 6.5f;
    constexpr float dotSpacingY = 6.0f;
    for (int row = -1; row <= 1; ++row)
    {
        for (int col = 0; col <= 1; ++col)
        {
            const float dx = (col == 0 ? -1.0f : 1.0f) * (dotSpacingX / 2.0f);
            const float dy = (float) row * dotSpacingY;
            g.fillEllipse (cx + dx - dotSize / 2.0f, cy + dy - dotSize / 2.0f, dotSize, dotSize);
        }
    }
}

void CsoundParameterMappingPanel::ChoiceParamRow::resized()
{
    juce::Rectangle<int> name, options, defaultIndex;
    CsoundParameterMappingPanel::layoutChoiceColumns (getLocalBounds().reduced (4, 2), handleBounds, name, options, defaultIndex);

    channelNameEditor.setBounds (name);
    optionsEditor.setBounds (options);
    defaultIndexEditor.setBounds (defaultIndex);
}

void CsoundParameterMappingPanel::ChoiceParamRow::mouseDown (const juce::MouseEvent& event)
{
    // Vedi il commento identico in ParamRow::mouseDown sopra.
    if (event.mods.isPopupMenu() && handleBounds.contains (event.getPosition()))
    {
        showCopyChngetMenu (processor.getChoiceParamSlot (index).channelName, onCopiedToClipboard);
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

    container->startDragging ("csoundChannel:" + slot.channelName, this);
}

//==============================================================================
// FloatParamsPage / BoolParamsPage / ChoiceParamsPage - vedi il commento in
// testa alla loro dichiarazione in CsoundParameterEditor.h: una tab = un
// header di colonne fisso + un viewport scrollabile con SOLO le righe di
// quel tipo.
CsoundParameterMappingPanel::FloatParamsPage::FloatParamsPage (CsoundAudioProcessor& processorToEdit)
{
    setupHeaderLabel (headerNameLabel, "chnget");
    setupHeaderLabel (headerMinLabel, "Min", juce::Justification::centredLeft);
    setupHeaderLabel (headerMaxLabel, "Max", juce::Justification::centredLeft);
    setupHeaderLabel (headerDefaultLabel, "Default", juce::Justification::centredLeft);
    setupHeaderLabel (headerSkewLabel, "Skew", juce::Justification::centredLeft);
    setupHeaderLabel (headerIncrementLabel, "Incr", juce::Justification::centredLeft);

    for (auto* label : { &headerNameLabel, &headerMinLabel, &headerMaxLabel, &headerDefaultLabel, &headerSkewLabel, &headerIncrementLabel })
        addAndMakeVisible (*label);

    for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
    {
        rows[(size_t) i] = std::make_unique<ParamRow> (processorToEdit, i);
        rows[(size_t) i]->onCopiedToClipboard = [this] (const juce::String& msg)
        {
            if (onParameterCopiedToClipboard)
                onParameterCopiedToClipboard (msg);
        };
        rowsContainer.addAndMakeVisible (*rows[(size_t) i]);
    }

    viewport.setViewedComponent (&rowsContainer, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

void CsoundParameterMappingPanel::FloatParamsPage::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto headerArea = area.removeFromTop (headerHeight);
    area.removeFromTop (4);

    viewport.setBounds (area);

    // Larghezza del contenuto: viewport.getWidth() MENO uno spazio fisso
    // riservato alla scrollbar (vedi scrollbarGutter), non
    // viewport.getMaximumVisibleWidth() - quel valore dipende da se la
    // scrollbar e' GIA' visibile in questo esatto istante, che durante
    // alcune sequenze di layout da' un risultato "vecchio" (misurato prima
    // che la scrollbar comparisse), con il risultato che i campi finivano
    // posizionati troppo a destra, sotto la scrollbar stessa.
    const int contentWidth = juce::jmax (100, viewport.getWidth() - scrollbarGutter);
    rowsContainer.setSize (contentWidth, CsoundAudioProcessor::numChannelParams * rowHeight);

    for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
        rows[(size_t) i]->setBounds (0, i * rowHeight, contentWidth, rowHeight);

    headerArea.setWidth (contentWidth);

    // reduced(4,0): stesso inset orizzontale che ParamRow::resized() applica
    // alle righe (getLocalBounds().reduced(4,2)) - senza questo, le colonne
    // dell'intestazione partivano 4px piu' a sinistra dei campi veri sotto,
    // un disallineamento sottile ma visibile su Min/Max/Default/Skew/Incr.
    headerArea = headerArea.reduced (4, 0);

    juce::Rectangle<int> handle, name, min, max, defaultVal, skewIncrement;
    CsoundParameterMappingPanel::layoutColumns (headerArea, handle, name, min, max, defaultVal, skewIncrement);
    juce::ignoreUnused (handle);

    headerNameLabel.setBounds (name);
    headerMinLabel.setBounds (min);
    headerMaxLabel.setBounds (max);
    headerDefaultLabel.setBounds (defaultVal);

    // Stessa divisione a meta' di ParamRow::resized(), cosi' le due
    // etichette "Skew"/"Incr" restano allineate ai due campi sotto.
    auto skewArea = skewIncrement.removeFromLeft (skewIncrement.getWidth() / 2 - 3);
    skewIncrement.removeFromLeft (6);
    headerSkewLabel.setBounds (skewArea);
    headerIncrementLabel.setBounds (skewIncrement);
}

void CsoundParameterMappingPanel::FloatParamsPage::refreshAllFromProcessor()
{
    for (auto& row : rows)
        row->refreshFromProcessor();
}

//==============================================================================
CsoundParameterMappingPanel::IntParamsPage::IntParamsPage (CsoundAudioProcessor& processorToEdit)
{
    setupHeaderLabel (headerNameLabel, "chnget");
    setupHeaderLabel (headerMinLabel, "Min", juce::Justification::centredLeft);
    setupHeaderLabel (headerMaxLabel, "Max", juce::Justification::centredLeft);
    setupHeaderLabel (headerDefaultLabel, "Default", juce::Justification::centredLeft);

    for (auto* label : { &headerNameLabel, &headerMinLabel, &headerMaxLabel, &headerDefaultLabel })
        addAndMakeVisible (*label);

    for (int i = 0; i < CsoundAudioProcessor::numIntParams; ++i)
    {
        rows[(size_t) i] = std::make_unique<IntParamRow> (processorToEdit, i);
        rows[(size_t) i]->onCopiedToClipboard = [this] (const juce::String& msg)
        {
            if (onParameterCopiedToClipboard)
                onParameterCopiedToClipboard (msg);
        };
        rowsContainer.addAndMakeVisible (*rows[(size_t) i]);
    }

    viewport.setViewedComponent (&rowsContainer, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

void CsoundParameterMappingPanel::IntParamsPage::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto headerArea = area.removeFromTop (headerHeight);
    area.removeFromTop (4);

    viewport.setBounds (area);

    const int contentWidth = juce::jmax (100, viewport.getWidth() - scrollbarGutter);
    rowsContainer.setSize (contentWidth, CsoundAudioProcessor::numIntParams * rowHeight);

    for (int i = 0; i < CsoundAudioProcessor::numIntParams; ++i)
        rows[(size_t) i]->setBounds (0, i * rowHeight, contentWidth, rowHeight);

    headerArea.setWidth (contentWidth);
    headerArea = headerArea.reduced (4, 0);

    juce::Rectangle<int> handle, name, min, max, defaultVal;
    CsoundParameterMappingPanel::layoutIntColumns (headerArea, handle, name, min, max, defaultVal);
    juce::ignoreUnused (handle);

    headerNameLabel.setBounds (name);
    headerMinLabel.setBounds (min);
    headerMaxLabel.setBounds (max);
    headerDefaultLabel.setBounds (defaultVal);
}

void CsoundParameterMappingPanel::IntParamsPage::refreshAllFromProcessor()
{
    for (auto& row : rows)
        row->refreshFromProcessor();
}

//==============================================================================
CsoundParameterMappingPanel::BoolParamsPage::BoolParamsPage (CsoundAudioProcessor& processorToEdit)
{
    setupHeaderLabel (headerNameLabel, "chnget");
    setupHeaderLabel (headerDefaultLabel, "Default", juce::Justification::centredLeft);

    for (auto* label : { &headerNameLabel, &headerDefaultLabel })
        addAndMakeVisible (*label);

    for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
    {
        rows[(size_t) i] = std::make_unique<BoolParamRow> (processorToEdit, i);
        rows[(size_t) i]->onCopiedToClipboard = [this] (const juce::String& msg)
        {
            if (onParameterCopiedToClipboard)
                onParameterCopiedToClipboard (msg);
        };
        rowsContainer.addAndMakeVisible (*rows[(size_t) i]);
    }

    viewport.setViewedComponent (&rowsContainer, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

void CsoundParameterMappingPanel::BoolParamsPage::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto headerArea = area.removeFromTop (headerHeight);
    area.removeFromTop (4);

    viewport.setBounds (area);

    const int contentWidth = juce::jmax (100, viewport.getWidth() - scrollbarGutter);
    rowsContainer.setSize (contentWidth, CsoundAudioProcessor::numBoolParams * rowHeight);

    for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
        rows[(size_t) i]->setBounds (0, i * rowHeight, contentWidth, rowHeight);

    headerArea.setWidth (contentWidth);
    headerArea = headerArea.reduced (4, 0);

    juce::Rectangle<int> handle, name, defaultVal;
    CsoundParameterMappingPanel::layoutBoolColumns (headerArea, handle, name, defaultVal);
    juce::ignoreUnused (handle);

    headerNameLabel.setBounds (name);
    headerDefaultLabel.setBounds (defaultVal);
}

void CsoundParameterMappingPanel::BoolParamsPage::refreshAllFromProcessor()
{
    for (auto& row : rows)
        row->refreshFromProcessor();
}

//==============================================================================
CsoundParameterMappingPanel::ChoiceParamsPage::ChoiceParamsPage (CsoundAudioProcessor& processorToEdit)
{
    setupHeaderLabel (headerNameLabel, "chnget");
    setupHeaderLabel (headerOptionsLabel, "Items (comma-separated)");
    setupHeaderLabel (headerDefaultLabel, "Default", juce::Justification::centredLeft);

    for (auto* label : { &headerNameLabel, &headerOptionsLabel, &headerDefaultLabel })
        addAndMakeVisible (*label);

    for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
    {
        rows[(size_t) i] = std::make_unique<ChoiceParamRow> (processorToEdit, i);
        rows[(size_t) i]->onCopiedToClipboard = [this] (const juce::String& msg)
        {
            if (onParameterCopiedToClipboard)
                onParameterCopiedToClipboard (msg);
        };
        rowsContainer.addAndMakeVisible (*rows[(size_t) i]);
    }

    viewport.setViewedComponent (&rowsContainer, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

void CsoundParameterMappingPanel::ChoiceParamsPage::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto headerArea = area.removeFromTop (headerHeight);
    area.removeFromTop (4);

    viewport.setBounds (area);

    const int contentWidth = juce::jmax (100, viewport.getWidth() - scrollbarGutter);
    rowsContainer.setSize (contentWidth, CsoundAudioProcessor::numChoiceParams * rowHeight);

    for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
        rows[(size_t) i]->setBounds (0, i * rowHeight, contentWidth, rowHeight);

    headerArea.setWidth (contentWidth);
    headerArea = headerArea.reduced (4, 0);

    juce::Rectangle<int> handle, name, options, defaultIndex;
    CsoundParameterMappingPanel::layoutChoiceColumns (headerArea, handle, name, options, defaultIndex);
    juce::ignoreUnused (handle);

    headerNameLabel.setBounds (name);
    headerOptionsLabel.setBounds (options);
    headerDefaultLabel.setBounds (defaultIndex);
}

void CsoundParameterMappingPanel::ChoiceParamsPage::refreshAllFromProcessor()
{
    for (auto& row : rows)
        row->refreshFromProcessor();
}

//==============================================================================
// GenericParamRow - vedi il commento in testa alla dichiarazione in
// CsoundParameterEditor.h. Colori impostati DIRETTAMENTE sui componenti
// (stesso approccio di applyDarkFieldColours/applyDarkComboColours sopra),
// non solo sulla LookAndFeel condivisa: uno Slider "congela" i colori della
// sua casella di testo interna (valueBox) nell'istante esatto in cui viene
// creata - un colore impostato qui, DIRETTAMENTE sull'istanza, vince
// comunque su qualunque LookAndFeel a prescindere da quando/se la
// propagazione di un eventuale cambio di LookAndFeel scatta.
CsoundParameterMappingPanel::GenericParamRow::GenericParamRow (
    const juce::String& channelName, juce::RangedAudioParameter& parameter,
    Kind rowKind, const juce::StringArray& choiceLabels)
    : kind (rowKind)
{
    nameLabel.setText (channelName, juce::dontSendNotification);
    nameLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    nameLabel.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (nameLabel);

    switch (kind)
    {
        case Kind::slider:
            slider.setScrollWheelEnabled (false);

            // Casella di testo EDITABILE (TextBoxRight, vedi l'header): si
            // puo' anche scrivere il valore a mano, non solo trascinare -
            // richiesto esplicitamente. Colori impostati DIRETTAMENTE
            // sull'istanza (non solo sulla LookAndFeel condivisa, che pure
            // li imposta - vedi CsoundParameterPanelLookAndFeel): uno
            // Slider "congela" i colori della sua casella di testo interna
            // nell'istante esatto in cui viene creata, quindi un colore
            // impostato qui, PRIMA che la casella sia creata (succede dentro
            // il costruttore dell'attachment sotto, la prima volta che la
            // LookAndFeel viene letta), e' la garanzia piu' solida che il
            // testo non resti "congelato" con i colori sbagliati.
            slider.setColour (juce::Slider::trackColourId,             kAccent);
            slider.setColour (juce::Slider::thumbColourId,             kAccent);
            slider.setColour (juce::Slider::backgroundColourId,        kFieldBg);
            slider.setColour (juce::Slider::textBoxTextColourId,       juce::Colours::white);
            slider.setColour (juce::Slider::textBoxBackgroundColourId, kFieldBg);
            slider.setColour (juce::Slider::textBoxOutlineColourId,    kFieldOutline);
            slider.setColour (juce::Slider::textBoxHighlightColourId,  kAccent.withAlpha (0.35f));
            addAndMakeVisible (slider);

            // sendInitialUpdate() (chiamata dentro il costruttore
            // dell'attachment) imposta subito lo slider al valore corrente
            // del parametro - niente valore "a zero" fino al primo cambio.
            sliderAttachment = std::make_unique<juce::SliderParameterAttachment> (parameter, slider);

            // Forza la casella di testo a rileggere ORA i colori appena
            // impostati sopra: la casella viene creata/ricreata da
            // LookAndFeel::createSliderTextBox ogni volta che
            // sendLookAndFeelChange()/colourChanged() scattano - qui lo
            // forziamo esplicitamente invece di sperare che sia gia'
            // successo "per conto suo" nell'ordine giusto.
            slider.sendLookAndFeelChange();
            break;

        case Kind::toggle:
            toggle.setColour (juce::ToggleButton::textColourId,         kText);
            toggle.setColour (juce::ToggleButton::tickColourId,         kAccent);
            toggle.setColour (juce::ToggleButton::tickDisabledColourId, kFieldOutline);
            addAndMakeVisible (toggle);
            // ButtonParameterAttachment chiama gia' da solo sendInitialUpdate().
            buttonAttachment = std::make_unique<juce::ButtonParameterAttachment> (parameter, toggle);
            break;

        case Kind::choice:
            // Le etichette vanno aggiunte PRIMA di costruire l'attachment,
            // nello STESSO ordine (0-based, indice+1 come ID) con cui
            // ComboBoxParameterAttachment mappera' l'indice selezionato al
            // valore del parametro - vedi GenericEditorPage::refreshRows().
            for (int i = 0; i < choiceLabels.size(); ++i)
                comboBox.addItem (choiceLabels[i], i + 1);
            comboBox.setColour (juce::ComboBox::backgroundColourId, kFieldBg);
            comboBox.setColour (juce::ComboBox::textColourId,       kText);
            comboBox.setColour (juce::ComboBox::outlineColourId,    kFieldOutline);
            comboBox.setColour (juce::PopupMenu::backgroundColourId,            kFieldBg);
            comboBox.setColour (juce::PopupMenu::textColourId,                  kText);
            comboBox.setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
            comboBox.setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
            addAndMakeVisible (comboBox);
            comboAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (parameter, comboBox);
            break;
    }
}

void CsoundParameterMappingPanel::GenericParamRow::resized()
{
    auto area = getLocalBounds().reduced (4, 2);
    auto nameArea = area.removeFromLeft (nameLabelWidth);
    area.removeFromLeft (8);
    nameLabel.setBounds (nameArea);

    switch (kind)
    {
        case Kind::slider:  slider.setBounds (area);   break;
        case Kind::toggle:  toggle.setBounds (area.removeFromLeft (juce::jmin (area.getWidth(), 120))); break;
        case Kind::choice:  comboBox.setBounds (area); break;
    }
}

//==============================================================================
CsoundParameterMappingPanel::GenericEditorPage::GenericEditorPage()
{
    viewport.setViewedComponent (&rowsContainer, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    emptyStateLabel.setText (
                             "No parameters configured.\n"
                             "Assign a Csound channel to the parameters in the Float, Int, Bool, or Choice tabs to see them here.",
        juce::dontSendNotification);
    emptyStateLabel.setJustificationType (juce::Justification::centred);
    emptyStateLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    emptyStateLabel.setColour (juce::Label::textColourId, kTextMuted);
    emptyStateLabel.setMinimumHorizontalScale (1.0f);
    addChildComponent (emptyStateLabel);
}

void CsoundParameterMappingPanel::GenericEditorPage::refreshRows (CsoundAudioProcessor& processor)
{
    rows.clear();

    for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
    {
        const auto slot = processor.getChannelParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;

        if (auto* param = processor.apvts.getParameter (CsoundAudioProcessor::getChannelParamID (i)))
            rows.push_back (std::make_unique<GenericParamRow> (slot.channelName, *param,
                                                                  GenericParamRow::Kind::slider, juce::StringArray()));
    }

    for (int i = 0; i < CsoundAudioProcessor::numIntParams; ++i)
    {
        const auto slot = processor.getIntParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;

        if (auto* param = processor.apvts.getParameter (CsoundAudioProcessor::getIntParamID (i)))
            rows.push_back (std::make_unique<GenericParamRow> (slot.channelName, *param,
                                                                  GenericParamRow::Kind::slider, juce::StringArray()));
    }

    for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
    {
        const auto slot = processor.getBoolParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;

        if (auto* param = processor.apvts.getParameter (CsoundAudioProcessor::getBoolParamID (i)))
            rows.push_back (std::make_unique<GenericParamRow> (slot.channelName, *param,
                                                                  GenericParamRow::Kind::toggle, juce::StringArray()));
    }

    for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
    {
        const auto slot = processor.getChoiceParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;

        if (auto* param = processor.apvts.getParameter (CsoundAudioProcessor::getChoiceParamID (i)))
        {
            juce::StringArray labels;
            for (int opt = 0; opt < CsoundAudioProcessor::maxChoiceOptions; ++opt)
                labels.add (CsoundAudioProcessor::getChoiceOptionLabel (slot, opt));

            rows.push_back (std::make_unique<GenericParamRow> (slot.channelName, *param,
                                                                  GenericParamRow::Kind::choice, labels));
        }
    }

    for (auto& row : rows)
        rowsContainer.addAndMakeVisible (*row);

    const bool empty = rows.empty();
    viewport.setVisible (! empty);
    emptyStateLabel.setVisible (empty);

    resized();
}

void CsoundParameterMappingPanel::GenericEditorPage::resized()
{
    auto area = getLocalBounds();

    emptyStateLabel.setBounds (area.reduced (24));

    area = area.reduced (8);
    viewport.setBounds (area);

    const int contentWidth = juce::jmax (100, viewport.getWidth() - scrollbarGutter);
    rowsContainer.setSize (contentWidth, (int) rows.size() * genericRowHeight);

    for (size_t i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (0, (int) i * genericRowHeight, contentWidth, genericRowHeight);
}

//==============================================================================
CsoundParameterMappingPanel::CsoundParameterMappingPanel (CsoundAudioProcessor& processorToEdit)
    : processor (processorToEdit)
{
    // Tema dedicato "cool" (vedi CsoundParameterPanelLookAndFeel) - si
    // applica a questo componente e, a cascata, a tutti i figli (barra del
    // titolo, tab, pagine, righe) che non impostano la propria LookAndFeel.
    setLookAndFeel (&lookAndFeel);

    // Barra del titolo della "finestra" flottante (vedi il commento in testa
    // alla classe in CsoundParameterEditor.h): solo qui, non nelle pagine,
    // perche' resta identica a prescindere dalla tab selezionata.
    titleLabel.setText ("Parameters", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, kText);
    // Non deve intercettare il mouse: altrimenti un click/trascinamento che
    // parte esattamente sopra il testo "Parameters" non arriverebbe mai a
    // CsoundParameterMappingPanel::mouseDown/mouseDrag (vedi sotto), che e'
    // quello che davvero sposta la finestra.
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton.setColour (juce::TextButton::buttonOnColourId, kFieldBg);
    closeButton.setColour (juce::TextButton::textColourOffId, kTextMuted);
    closeButton.setColour (juce::TextButton::textColourOnId, kText);
    closeButton.onClick = [this] { if (onCloseButtonClicked) onCloseButtonClicked(); };
    addAndMakeVisible (closeButton);

    floatPage         = std::make_unique<FloatParamsPage>  (processorToEdit);
    intPage           = std::make_unique<IntParamsPage>    (processorToEdit);
    boolPage          = std::make_unique<BoolParamsPage>   (processorToEdit);
    choicePage        = std::make_unique<ChoiceParamsPage> (processorToEdit);

    // Inoltra il riscontro "copiato negli appunti" di ogni riga, di
    // qualunque tab, fino a onParameterCopiedToClipboard (vedi dichiarazione
    // in CsoundParameterEditor.h) - PluginEditor lo aggancia a appendToLog.
    auto forwardCopyNotice = [this] (const juce::String& msg)
    {
        if (onParameterCopiedToClipboard)
            onParameterCopiedToClipboard (msg);
    };
    floatPage->onParameterCopiedToClipboard  = forwardCopyNotice;
    intPage->onParameterCopiedToClipboard    = forwardCopyNotice;
    boolPage->onParameterCopiedToClipboard   = forwardCopyNotice;
    choicePage->onParameterCopiedToClipboard = forwardCopyNotice;
    genericEditorPage = std::make_unique<GenericEditorPage>();

    // Le 5 pagine sono figli diretti del pannello (non piu' ospitate da un
    // TabbedComponent): showPage() decide quale e' visibile, addChildComponent
    // (non addAndMakeVisible) le aggiunge gia' nascoste - showPage(0) in
    // fondo al costruttore le rende visibili/invisibili correttamente.
    addChildComponent (*floatPage);
    addChildComponent (*intPage);
    addChildComponent (*boolPage);
    addChildComponent (*choicePage);
    addChildComponent (*genericEditorPage);

    // 5 bottoni "a mano" al posto di TabbedButtonBar - vedi il commento sul
    // perche' in CsoundParameterEditor.h. onClick chiama semplicemente
    // showPage(i): nessuna euristica di layout/overflow di mezzo.
    //
    // setConnectedEdges unisce visivamente i 5 bottoni in un'unica barra
    // segmentata (stile macOS/iOS segmented control) - tutti gli angoli
    // sono comunque a 90 gradi (vedi CsoundParameterPanelLookAndFeel::
    // drawButtonBackground, che non arrotonda nulla), setConnectedEdges
    // qui serve solo a non disegnare il bordo tra bottoni adiacenti.
    //
    // Ordine VISIVO richiesto: UI, Float, Int, Bool, Choice (UI per prima a
    // sinistra) - DIVERSO dall'ordine degli indici di pagina (float=0,
    // int=1, bool=2, choice=3, genericEditor=4, usati da showPage() e da
    // tutte le pagine/array esistenti, invariati per non toccare altrove).
    // getOrderedTabs() sotto e' l'UNICA fonte di verita' per l'ordine
    // visivo, condivisa da questo ciclo, updateTabButtonStyles() e
    // resized() - evita di dover tenere sincronizzati tre elenchi scritti
    // a mano in punti diversi.
    genericEditorTabButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    floatTabButton.setConnectedEdges         (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    intTabButton.setConnectedEdges           (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    boolTabButton.setConnectedEdges          (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    choiceTabButton.setConnectedEdges        (juce::Button::ConnectedOnLeft);

    // Nome usato da CsoundParameterPanelLookAndFeel::drawButtonText per
    // riconoscere questo bottone e disegnarci sopra l'icona "tune" - vedi
    // makeTuneIconPath sopra.
    genericEditorTabButton.setName ("genericEditorTab");

    for (auto& tab : getOrderedTabs())
    {
        auto* button = tab.first;
        const int pageIndex = tab.second;
        button->onClick = [this, pageIndex] { showPage (pageIndex); };
        button->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (*button);
    }

    updateTabButtonStyles();

    // Tab mostrata di default all'apertura del pannello: 4 = "UI" (Generic
    // Editor, vedi getOrderedTabs()), non 0 (Float) - e' la vista con gli
    // slider/toggle/combo VERI agganciati ai parametri reali, quella che
    // l'utente vuole vedere per prima entrando nel pannello Parametri.
    showPage (4);
}

std::array<std::pair<juce::TextButton*, int>, 5> CsoundParameterMappingPanel::getOrderedTabs()
{
    // {bottone, indice di pagina} nell'ordine VISIVO sinistra->destra - vedi
    // il commento nel costruttore sopra. L'indice di pagina e' quello usato
    // da showPage()/floatPage/intPage/boolPage/choicePage/genericEditorPage,
    // invariato: solo l'ORDINE in questo elenco decide la posizione a video.
    return { { { &genericEditorTabButton, 4 },
               { &floatTabButton,         0 },
               { &intTabButton,           1 },
               { &boolTabButton,          2 },
               { &choiceTabButton,        3 } } };
}

CsoundParameterMappingPanel::~CsoundParameterMappingPanel()
{
    // Stacca subito il collegamento alla nostra LookAndFeel (membro di
    // questa stessa classe, distrutto comunque dopo per ordine di
    // dichiarazione - vedi CsoundParameterEditor.h): farlo qui, per primo,
    // evita che un repaint durante lo smontaggio dei figli trovi un
    // puntatore a LookAndFeel gia' invalido.
    setLookAndFeel (nullptr);
}

void CsoundParameterMappingPanel::showPage (int pageIndex)
{
    currentPageIndex = pageIndex;

    floatPage->setVisible         (pageIndex == 0);
    intPage->setVisible           (pageIndex == 1);
    boolPage->setVisible          (pageIndex == 2);
    choicePage->setVisible        (pageIndex == 3);
    genericEditorPage->setVisible (pageIndex == 4);

    // La tab "UI" mostra i VALORI correnti: va ricostruita ogni volta che
    // diventa quella corrente (non solo alla prima apertura), cosi' un
    // canale rinominato/assegnato nel frattempo in una delle altre 4 tab si
    // riflette qui subito invece di restare congelato.
    if (pageIndex == 4)
        genericEditorPage->refreshRows (processor);

    updateTabButtonStyles();
}

void CsoundParameterMappingPanel::updateTabButtonStyles()
{
    // Stesso massimo contrasto del precedente drawTabButton: selezionata =
    // pieno accento teal/testo bianco, non selezionata = grigio chiaro/testo
    // quasi nero - qui applicato con semplici setColour() su TextButton
    // normali, niente LookAndFeel custom per i bottoni.
    for (auto& tab : getOrderedTabs())
    {
        auto* button = tab.first;
        const bool selected = (tab.second == currentPageIndex);

        button->setColour (juce::TextButton::buttonColourId, selected ? kAccent : juce::Colour (0xff9aa7b0));
        button->setColour (juce::TextButton::buttonOnColourId, selected ? kAccent : juce::Colour (0xff9aa7b0));
        button->setColour (juce::TextButton::textColourOffId, selected ? juce::Colours::white : juce::Colour (0xff0e1318));
        button->setColour (juce::TextButton::textColourOnId, selected ? juce::Colours::white : juce::Colour (0xff0e1318));
    }
}

void CsoundParameterMappingPanel::refreshAllFromProcessor()
{
    floatPage->refreshAllFromProcessor();
    intPage->refreshAllFromProcessor();
    boolPage->refreshAllFromProcessor();
    choicePage->refreshAllFromProcessor();

    // La tab Generic Editor si ricostruisce comunque da sola ogni volta che
    // diventa quella corrente (vedi showPage()), ma se e' GIA' quella
    // visibile in questo momento (l'utente ha caricato una sessione mentre
    // era su "UI") va aggiornata subito, altrimenti mostrerebbe ancora
    // slider/toggle/combo della sessione precedente finche' non si cambia
    // tab e si ritorna.
    if (currentPageIndex == 4)
        genericEditorPage->refreshRows (processor);
}

void CsoundParameterMappingPanel::paint (juce::Graphics& g)
{
    // Pannello "a finestra": corpo scuro a spigoli vivi (90 gradi, nessun
    // bordo arrotondato) con una sottile ombra verso l'esterno, cosi' si
    // stacca visivamente dal resto dell'editor (codice/consolle, visibili
    // sotto il velo semitrasparente disegnato da PluginEditor) invece di
    // sembrare parte dello sfondo.
    auto bounds = getLocalBounds().toFloat();

    juce::DropShadow shadow (juce::Colours::black.withAlpha (0.55f), 18, {});
    juce::Path outline;
    outline.addRectangle (bounds);
    shadow.drawForPath (g, outline);

    g.setColour (kPanelBg);
    g.fillRect (bounds);

    auto titleBarBounds = bounds.removeFromTop ((float) titleBarHeight);
    g.setColour (kTitleBarBg);
    g.fillRect (titleBarBounds);

    g.setColour (kFieldOutline);
    g.drawLine (titleBarBounds.getX(), titleBarBounds.getBottom(), titleBarBounds.getRight(), titleBarBounds.getBottom(), 1.0f);

    g.setColour (kAccent.withAlpha (0.6f));
    g.drawRect (getLocalBounds().toFloat().reduced (0.5f), 1.2f);
}

void CsoundParameterMappingPanel::resized()
{
    auto area = getLocalBounds();

    auto titleBarArea = area.removeFromTop (titleBarHeight).reduced (10, 4);
    closeButton.setBounds (titleBarArea.removeFromRight (64));
    titleLabel.setBounds (titleBarArea);

    area = area.reduced (4);

    // Barra tab: 5 bottoni di larghezza ESATTAMENTE uguale che riempiono
    // tutta la riga - layout banale, niente auto-layout/euristiche di
    // overflow di mezzo (vedi il commento in testa ai membri *TabButton in
    // CsoundParameterEditor.h sul perche' non usiamo piu' TabbedComponent).
    auto tabBarArea = area.removeFromTop (tabBarHeight);
    const int eachTabWidth = tabBarArea.getWidth() / 5;

    const auto orderedTabs = getOrderedTabs();

    for (int i = 0; i < 5; ++i)
    {
        const int x = i * eachTabWidth;
        const int w = (i == 4) ? (tabBarArea.getWidth() - x) : eachTabWidth;
        orderedTabs[(size_t) i].first->setBounds (tabBarArea.getX() + x, tabBarArea.getY(), w, tabBarArea.getHeight());
    }

    area.removeFromTop (4);

    // Tutte e 5 le pagine occupano la stessa area sotto la barra tab -
    // showPage() decide quale e' visibile, qui basta dare a tutte lo stesso
    // rettangolo (quella nascosta non viene disegnata, il costo e'
    // trascurabile).
    floatPage->setBounds (area);
    intPage->setBounds (area);
    boolPage->setBounds (area);
    choicePage->setBounds (area);
    genericEditorPage->setBounds (area);
}

void CsoundParameterMappingPanel::mouseDown (const juce::MouseEvent& event)
{
    // Solo dalla barra del titolo (non dal corpo del pannello, dove si
    // vuole poter cliccare/trascinare normalmente sui campi) - closeButton
    // e' un figlio e intercetta gia' da solo i propri click, quindi non
    // serve escluderlo esplicitamente qui.
    draggingTitleBar = event.position.y < (float) titleBarHeight;

    if (draggingTitleBar)
        titleBarDragger.startDraggingComponent (this, event);
}

void CsoundParameterMappingPanel::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingTitleBar)
        return;

    titleBarDragger.dragComponent (this, event, nullptr);

    // Resta dentro i confini del genitore (l'editor del plugin): niente
    // ComponentBoundsConstrainer (pensato per ridimensionare, qui serve solo
    // bloccare la posizione) - un clamp manuale sulla posizione basta.
    if (auto* parent = getParentComponent())
    {
        auto bounds = getBounds();
        bounds.setPosition (juce::jlimit (0, juce::jmax (0, parent->getWidth()  - bounds.getWidth()),  bounds.getX()),
                             juce::jlimit (0, juce::jmax (0, parent->getHeight() - bounds.getHeight()), bounds.getY()));
        setBounds (bounds);
    }
}
