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

    // applyDarkComboColours() rimossa: nessun ComboBox e' piu' usato in
    // questo pannello da quando il menu "Curve" e' stato tolto. drawComboBox/
    // getComboBoxFont restano nella LookAndFeel (sotto) solo per completezza,
    // nel caso un ComboBox venga reintrodotto in futuro.
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
}

void CsoundParameterPanelLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 5.0f);
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
        g.drawRoundedRectangle (bounds.expanded (1.5f), 6.0f, 2.5f);
        g.setColour (kAccent);
        g.drawRoundedRectangle (bounds, 5.0f, 1.4f);
    }
    else
    {
        g.setColour (kFieldOutline);
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
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
    g.fillRoundedRectangle (bounds, 5.0f);

    g.setColour (box.hasKeyboardFocus (true) ? kAccent : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, 5.0f, 1.2f);

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

    // Riquadro arrotondato pieno dietro la voce evidenziata (hover/
    // selezione da tastiera), non il solito rettangolo vivo a spigoli
    // vivi di LookAndFeel_V4 - stesso linguaggio visivo del resto del
    // pannello (handle di drag, bordo dei campi, ecc.).
    if (isHighlighted && isActive)
    {
        g.setColour (kAccent);
        g.fillRoundedRectangle (itemArea.toFloat(), 4.0f);
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

    // Niente binario/frecce disegnati: solo un thumb sottile, arrotondato,
    // inset di 2px dai bordi, che si accende leggermente al passaggio/
    // pressione del mouse - stile minimale coerente col resto del pannello.
    auto thumbBounds = isScrollbarVertical
                            ? juce::Rectangle<int> (x + 2, thumbStartPosition, juce::jmax (2, width - 4), thumbSize)
                            : juce::Rectangle<int> (thumbStartPosition, y + 2, thumbSize, juce::jmax (2, height - 4));

    const auto alpha = isMouseDown ? 0.85f : (isMouseOver ? 0.65f : 0.45f);
    g.setColour (kAccent.withAlpha (alpha));
    g.fillRoundedRectangle (thumbBounds.toFloat(), 3.0f);
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

    // "Rename": qui si da' al parametro il suo nome VERO, cioe' il nome del
    // canale Csound (chnget/chnset) a cui e' agganciato - l'host continua a
    // vedere sempre "Param N" (vedi CsoundAudioProcessor::createChannelParamLayout),
    // cosi' l'automazione resta stabile anche rinominando/riassegnando i canali.
    channelNameEditor.setTextToShowWhenEmpty ("(no channel - e.g. freq)", kPlaceholder);
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

    refreshFromProcessor();
}

void CsoundParameterMappingPanel::ParamRow::refreshFromProcessor()
{
    const auto slot = processor.getChannelParamSlot (index);

    channelNameEditor.setText (slot.channelName, false);
    minEditor.setText (juce::String (slot.minValue), false);
    maxEditor.setText (juce::String (slot.maxValue), false);
    defaultEditor.setText (juce::String (slot.defaultValue), false);
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

    // Niente piu' combo "Curve" nel pannello (rimosso su richiesta): la
    // curva resta sempre lineare da qui - l'enum/i branch esponenziale e
    // logaritmico restano lato processor per un eventuale reintroduzione
    // futura dell'UI, ma non sono piu' raggiungibili da questo pannello.
    slot.curve = CsoundAudioProcessor::ChannelParamCurve::linear;

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
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (kAccent.withAlpha (handleHovered ? 1.0f : 0.75f));
    g.drawRoundedRectangle (box, 4.0f, 1.2f);

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
    juce::Rectangle<int> name, min, max, defaultVal;
    CsoundParameterMappingPanel::layoutColumns (getLocalBounds().reduced (4, 2), handleBounds, name, min, max, defaultVal);

    channelNameEditor.setBounds (name);
    minEditor.setBounds (min);
    maxEditor.setBounds (max);
    defaultEditor.setBounds (defaultVal);
}

void CsoundParameterMappingPanel::ParamRow::mouseDown (const juce::MouseEvent& event)
{
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

    // Il nome prende TUTTO lo spazio che resta, fino al campo min (non una
    // larghezza fissa): cosi' la riga - e quindi l'intero pannello - scala
    // con la larghezza della finestra del plugin invece di lasciare spazio
    // vuoto o tagliare il nome.
    name = area;
}

CsoundParameterMappingPanel::CsoundParameterMappingPanel (CsoundAudioProcessor& processorToEdit)
{
    // Tema dedicato "cool" (vedi CsoundParameterPanelLookAndFeel) - si
    // applica a questo componente e, a cascata, a tutti i figli (righe,
    // header, viewport/scrollbar) che non impostano la propria LookAndFeel.
    setLookAndFeel (&lookAndFeel);

    // Un'etichetta per colonna, non un'unica stringa imbottita di spazi:
    // layoutColumns() le allinea esattamente come i campi sotto, in
    // resized() - qui si imposta solo testo/font/colore.
    for (auto* label : { &headerNameLabel, &headerMinLabel, &headerMaxLabel, &headerDefaultLabel })
    {
        label->setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        label->setColour (juce::Label::textColourId, kTextMuted);
        addAndMakeVisible (*label);
    }

    headerNameLabel.setText ("Csound channel (chnget/chnset)", juce::dontSendNotification);
    headerMinLabel.setText ("Min", juce::dontSendNotification);
    headerMaxLabel.setText ("Max", juce::dontSendNotification);
    headerDefaultLabel.setText ("Default", juce::dontSendNotification);
    headerMinLabel.setJustificationType (juce::Justification::centred);
    headerMaxLabel.setJustificationType (juce::Justification::centred);
    headerDefaultLabel.setJustificationType (juce::Justification::centred);

    for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
    {
        rows[(size_t) i] = std::make_unique<ParamRow> (processorToEdit, i);
        rowsContainer.addAndMakeVisible (*rows[(size_t) i]);
    }

    viewport.setViewedComponent (&rowsContainer, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    // Larghezza "naturale" del pannello coi campi a dimensione fissa
    // (handle + nome + min + max + curva + i margini tra loro) - usata come
    // default e come riferimento da PluginEditor per la larghezza del
    // pannello laterale (vedi parameterPanelWidth).
    setSize (preferredWidth, 500);
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

void CsoundParameterMappingPanel::paint (juce::Graphics& g)
{
    // Tema scuro dedicato (vedi il namespace anonimo in cima al file): il
    // testo nero di default su sfondo chiaro risultava illeggibile in
    // diversi stati dei campi - qui lo sfondo e' scuro e ogni componente ha
    // i propri colori impostati esplicitamente per restare leggibile a
    // prescindere dal tema generale (chiaro) del resto del plugin.
    g.fillAll (kPanelBg);

    // Sottile linea accentata (sfumata ai lati) sotto l'intestazione, al
    // posto di un piatto separatore grigio - stesso accento teal usato
    // ovunque nel pannello.
    if (headerBottomY > 0)
    {
        auto gradient = juce::ColourGradient::horizontal (
            kAccent.withAlpha (0.0f), 0.0f, kAccent.withAlpha (0.0f), (float) getWidth());
        gradient.addColour (0.5, kAccent.withAlpha (0.6f));
        g.setGradientFill (gradient);
        g.fillRect (0, headerBottomY, getWidth(), 1);
    }
}

void CsoundParameterMappingPanel::resized()
{
    auto area = getLocalBounds().reduced (8);

    auto headerArea = area.removeFromTop (headerHeight);
    headerBottomY = headerArea.getBottom() + 2;
    area.removeFromTop (4);

    viewport.setBounds (area);

    const int contentWidth = viewport.getMaximumVisibleWidth();
    rowsContainer.setSize (contentWidth, CsoundAudioProcessor::numChannelParams * rowHeight);

    for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
        rows[(size_t) i]->setBounds (0, i * rowHeight, contentWidth, rowHeight);

    // Stessa larghezza delle righe sotto (non quella piena di headerArea,
    // che includerebbe lo spazio della scrollbar del viewport) - cosi' le
    // etichette restano allineate ai campi anche quando la scrollbar e'
    // visibile.
    headerArea.setWidth (contentWidth);

    juce::Rectangle<int> handle, name, min, max, defaultVal;
    layoutColumns (headerArea, handle, name, min, max, defaultVal);
    juce::ignoreUnused (handle);

    headerNameLabel.setBounds (name);
    headerMinLabel.setBounds (min);
    headerMaxLabel.setBounds (max);
    headerDefaultLabel.setBounds (defaultVal);
}
