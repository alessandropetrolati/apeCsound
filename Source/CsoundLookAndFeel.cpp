#include "CsoundLookAndFeel.h"

namespace
{
    const juce::Colour kBackground { 0xfff3f6f8 }; // sfondo generale, grigio-azzurro molto chiaro
    const juce::Colour kPanel      { 0xffffffff }; // pannelli/editor di testo
    const juce::Colour kAccent     { 0xff17a2b8 }; // teal "cool" - accento dei controlli
    const juce::Colour kAccentDark { 0xff11808f };
    const juce::Colour kText       { 0xff1f2933 };
    const juce::Colour kTextMuted  { 0xff6b7a85 };
    const juce::Colour kOutline    { 0xffd7dee3 };

    // Icone da path SVG di Material Design Icons (Apache 2.0), non disegnate
    // a mano: shape testate piuttosto che geometria approssimata. Il
    // bottone viene riconosciuto dal suo Component::setName("apply"/"clear").
    // juce::Drawable::parseSVGPath interpreta la stringa "d" dell'SVG; la
    // scala originale (viewBox 24x24) non conta perche' drawButtonText poi
    // chiama Path::scaleToFit sull'area dell'icona.

    // MDI "refresh": un arco quasi completo con una freccia in punta - non
    // un play, perche' il motore Csound e' sempre in esecuzione, il bottone
    // si limita a rimpiazzare il .csd corrente con quello appena modificato.
    // NON PIU' usata per Apply (vedi makeAudioEngineIconPath() sotto,
    // richiesta esplicita: "qualcosa che ha a che fare con l'engine
    // audio", un refresh generico andava bene per qualsiasi bottone) -
    // lasciata qui comunque: nessun altro bottone la usa oggi, ma e' una
    // forma pulita da riprendere in futuro.
    juce::Path makeReloadIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M17.65,6.35C16.2,4.9 14.21,4 12,4A8,8 0 0,0 4,12A8,8 0 0,0 12,20C15.73,20 "
            "18.84,17.45 19.73,14H17.65C16.83,16.33 14.61,18 12,18A6,6 0 0,1 6,12A6,6 0 0,1 "
            "12,6C13.66,6 15.14,6.69 16.22,7.78L13,11H20V4L17.65,6.35Z");
    }

    // Icona per Apply: un'onda audio stilizzata (3 gobbe, come una
    // waveform) invece del "refresh" generico sopra - richiesto
    // esplicitamente ("qualcosa che ha a che fare con l'engine audio").
    // NON una freccia "play"/triangolo: il motore Csound e' SEMPRE in
    // esecuzione (vedi il commento su makeReloadIconPath sopra), un'icona
    // di trasporto sarebbe fuorviante - Apply si limita a rimpiazzare il
    // .csd in esecuzione con quello appena modificato. Costruita come
    // un'area piena ottenuta ispessendo una linea ondulata (nessun path
    // SVG a memoria, stessa convenzione di makeEqualizerIconPath/
    // makeSaveIconPath sopra): drawButtonText riempie SEMPRE il Path
    // restituito (g.fillPath), una linea sola (non chiusa) non basta.
    juce::Path makeAudioEngineIconPath()
    {
        juce::Path wave;
        wave.startNewSubPath (1.0f, 12.0f);
        wave.cubicTo (4.0f, 12.0f, 4.0f, 3.0f, 7.5f, 3.0f);
        wave.cubicTo (11.0f, 3.0f, 11.0f, 21.0f, 14.5f, 21.0f);
        wave.cubicTo (18.0f, 21.0f, 18.0f, 3.0f, 21.5f, 3.0f);
        wave.cubicTo (22.5f, 3.0f, 23.0f, 7.0f, 23.0f, 12.0f);

        juce::Path filled;
        juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createStrokedPath (filled, wave);
        return filled;
    }

    // MDI "trash-can-outline".
    juce::Path makeTrashIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M9,3V4H4V6H5V19A2,2 0 0,0 7,21H17A2,2 0 0,0 19,19V6H20V4H15V3H9M7,6H17V19H7V6M9,"
            "8V17H11V8H9M13,8V17H15V8H13Z");
    }

    // MDI "tune": tre slider orizzontali con manopola - apre la finestra di
    // mapping dei parametri (rename canale + range/skew/increment per slot).
    juce::Path makeTuneIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M3,17V19H9V17H3M3,5V7H13V5H3M13,21V19H21V17H13V15H11V21H13M7,9V11H3V13H7V15H9V9H7M21,"
            "13V11H11V13H21M15,9H17V7H21V5H17V3H15V9Z");
    }

    // Icona "equalizer" per il bottone Generic Editor: 4 barre verticali ad
    // altezza diversa - deliberatamente MOLTO diversa dall'icona "tune"
    // (linee orizzontali con pallini, usata da Parameters) per non
    // confondere i due bottoni a colpo d'occhio. Costruita con geometria
    // esplicita (rettangoli arrotondati), non un path SVG memorizzato a
    // mano: piu' sicuro, nessun rischio di forma sbagliata. Viewbox 24x24,
    // stessa convenzione delle altre icone sopra.
    juce::Path makeEqualizerIconPath()
    {
        juce::Path p;
        constexpr float barWidth = 4.0f;
        constexpr float bottom   = 22.0f;
        const float xs[4]      = { 2.0f, 8.0f, 14.0f, 20.0f };
        const float heights[4] = { 10.0f, 18.0f, 7.0f, 14.0f };

        for (int i = 0; i < 4; ++i)
            p.addRectangle (xs[i], bottom - heights[i], barWidth, heights[i]);

        return p;
    }

    // Icone "freccia in/da un vassoio" per Save/Load Session (vedi
    // CsoundAudioProcessor::saveSessionToFile/loadSessionFromFile) - stessa
    // geometria esplicita delle altre sopra (nessun path SVG a memoria),
    // deliberatamente diverse da tutte le altre icone della toolbar:
    // freccia IN GIU' dentro un vassoio per Save (il codice "scende" su
    // disco), freccia IN SU' da un vassoio per Load (il codice "risale" da
    // disco nel plugin). Viewbox 24x24, stessa convenzione delle altre.
    juce::Path makeSaveIconPath()
    {
        juce::Path p;
        p.addRectangle (10.5f, 3.0f, 3.0f, 10.0f);              // asta della freccia
        p.addTriangle (7.0f, 13.0f, 17.0f, 13.0f, 12.0f, 19.0f); // punta verso il basso
        p.addRectangle (3.0f, 20.0f, 18.0f, 3.0f);               // vassoio in basso
        return p;
    }

    juce::Path makeLoadIconPath()
    {
        juce::Path p;
        p.addRectangle (10.5f, 10.0f, 3.0f, 10.0f);             // asta della freccia
        p.addTriangle (7.0f, 10.0f, 17.0f, 10.0f, 12.0f, 4.0f); // punta verso l'alto
        p.addRectangle (3.0f, 20.0f, 18.0f, 3.0f);               // vassoio in basso
        return p;
    }

    // MDI "console": un rettangolo (schermo) con un prompt ">" dentro -
    // bottone Console/Hide Console nella toolbar (vedi PluginEditor.h/.cpp),
    // analogo a "params" per la sidebar ma per mostrare/nascondere logConsole.
    juce::Path makeConsoleIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M20,19V7H4V19H20M20,3A2,2 0 0,1 22,5V19A2,2 0 0,1 20,21H4A2,2 0 0,1 2,19V5C2,3.89 "
            "2.9,3 4,3H20M13,17V15H18V17H13M9.58,13L5.57,9H8.4L11.7,12.3C12.09,12.69 12.09,13.33 "
            "11.7,13.72L8.42,17H5.59L9.58,13Z");
    }

    juce::Path getIconPathForButtonName (const juce::String& name)
    {
        if (name == "apply")
            return makeAudioEngineIconPath();

        if (name == "clear")
            return makeTrashIconPath();

        if (name == "params")
            return makeTuneIconPath();

        if (name == "console")
            return makeConsoleIconPath();

        if (name == "genericEditor")
            return makeEqualizerIconPath();

        if (name == "saveSession")
            return makeSaveIconPath();

        if (name == "loadSession")
            return makeLoadIconPath();

        return {};
    }
}

CsoundLookAndFeel::CsoundLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kBackground);

    setColour (juce::TextButton::buttonColourId,   kAccent);
    setColour (juce::TextButton::buttonOnColourId, kAccentDark);
    setColour (juce::TextButton::textColourOffId,  juce::Colours::white);
    setColour (juce::TextButton::textColourOnId,   juce::Colours::white);

    setColour (juce::TextEditor::backgroundColourId,      kPanel);
    setColour (juce::TextEditor::textColourId,            kText);
    setColour (juce::TextEditor::outlineColourId,         kOutline);
    setColour (juce::TextEditor::focusedOutlineColourId,  kAccent);
    setColour (juce::TextEditor::highlightColourId,       kAccent.withAlpha (0.25f));

    setColour (juce::CodeEditorComponent::backgroundColourId,       kPanel);
    setColour (juce::CodeEditorComponent::defaultTextColourId,      kText);
    setColour (juce::CodeEditorComponent::lineNumberBackgroundId,   juce::Colour (0xffeef2f5));
    setColour (juce::CodeEditorComponent::lineNumberTextId,         kTextMuted);
    setColour (juce::CodeEditorComponent::highlightColourId,        kAccent.withAlpha (0.2f));

    setColour (juce::ScrollBar::thumbColourId, kAccent.withAlpha (0.55f));
    setColour (juce::Label::textColourId,      kText);

    setColour (juce::ComboBox::backgroundColourId, kPanel);
    setColour (juce::ComboBox::outlineColourId,    kOutline);
}

void CsoundLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

    auto colour = backgroundColour;

    if (shouldDrawButtonAsDown)
        colour = colour.darker (0.25f);
    else if (shouldDrawButtonAsHighlighted)
        colour = colour.brighter (0.12f);

    g.setColour (colour);

    // Bottone "circolare" (proprieta' dinamica "circular"): STESSO colore e
    // STESSA risposta a hover/down di tutti gli altri bottoni sopra (stesso
    // "colour" calcolato identicamente) - cambia SOLO la forma (arrotondata
    // invece che squadrata), niente ombra o altro trattamento speciale che
    // lo farebbe sembrare "spento"/diverso dagli altri. Usato da
    // clearConsoleButton in PluginEditor, in overlap sopra l'angolo in alto
    // a destra della consolle invece che nella toolbar.
    const bool circular = (bool) button.getProperties().getWithDefault ("circular", false);

    if (circular)
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
    else
        g.fillRect (bounds); // angoli a 90 gradi per tutti gli ALTRI widget

    // Bordo rosso tutto intorno al bottone quando il chiamante lo segnala
    // con una proprieta' dinamica (Component::getProperties(), un semplice
    // NamedValueSet - non serve una nuova API di LookAndFeel solo per
    // questo): usato da applyButton in PluginEditor per indicare che il
    // codice nell'editor e' stato modificato e non coincide piu' con
    // quello applicato/caricato - vedi CsoundAudioProcessorEditor::
    // updateApplyButtonDirtyState().
    if (! circular && (bool) button.getProperties().getWithDefault ("pendingChanges", false))
    {
        g.setColour (juce::Colours::red);
        g.drawRect (bounds, 2.0f);
    }
}

juce::Font CsoundLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (15.0f, (float) buttonHeight * 0.5f), juce::Font::bold));
}

void CsoundLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    auto textColour = button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                                  : juce::TextButton::textColourOffId);
    g.setColour (textColour);

    auto icon = getIconPathForButtonName (button.getName());

    // Bottone circolare: SOLO icona, centrata su tutto il cerchio, niente
    // testo (il bottone e' troppo piccolo per ospitarne, e clearConsoleButton
    // in PluginEditor ha comunque testo vuoto) - vedi il commento identico
    // su drawButtonBackground sopra.
    if ((bool) button.getProperties().getWithDefault ("circular", false))
    {
        if (! icon.isEmpty())
        {
            auto bounds = button.getLocalBounds().toFloat();
            const float iconSize = bounds.getHeight() * 0.5f;
            auto iconArea = bounds.withSizeKeepingCentre (iconSize, iconSize);

            icon.scaleToFit (iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true);
            g.fillPath (icon);
        }

        return;
    }

    auto bounds = button.getLocalBounds().toFloat().reduced (10.0f, 0.0f);

    if (! icon.isEmpty())
    {
        const float iconSize = juce::jmin (bounds.getHeight() * 0.55f, 16.0f);
        auto iconArea = bounds.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize);

        icon.scaleToFit (iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true);
        g.fillPath (icon);

        bounds.removeFromLeft (7.0f); // spazio tra icona e testo
    }

    auto font = getTextButtonFont (button, button.getHeight());
    g.setFont (font);
    g.drawFittedText (button.getButtonText(), bounds.toNearestInt(), juce::Justification::centredLeft, 1);
}

int CsoundLookAndFeel::getIconAllowance (const juce::String& buttonName)
{
    return getIconPathForButtonName (buttonName).isEmpty() ? 0 : 23; // 16px icona + 7px spaziatura
}
