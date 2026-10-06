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
    juce::Path makeReloadIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M17.65,6.35C16.2,4.9 14.21,4 12,4A8,8 0 0,0 4,12A8,8 0 0,0 12,20C15.73,20 "
            "18.84,17.45 19.73,14H17.65C16.83,16.33 14.61,18 12,18A6,6 0 0,1 6,12A6,6 0 0,1 "
            "12,6C13.66,6 15.14,6.69 16.22,7.78L13,11H20V4L17.65,6.35Z");
    }

    // MDI "trash-can-outline".
    juce::Path makeTrashIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M9,3V4H4V6H5V19A2,2 0 0,0 7,21H17A2,2 0 0,0 19,19V6H20V4H15V3H9M7,6H17V19H7V6M9,"
            "8V17H11V8H9M13,8V17H15V8H13Z");
    }

    // MDI "tune": tre slider orizzontali con manopola - apre la finestra di
    // mapping dei parametri (rename canale + range/curva per slot).
    juce::Path makeTuneIconPath()
    {
        return juce::Drawable::parseSVGPath (
            "M3,17V19H9V17H3M3,5V7H13V5H3M13,21V19H21V17H13V15H11V21H13M7,9V11H3V13H7V15H9V9H7M21,"
            "13V11H11V13H21M15,9H17V7H21V5H17V3H15V9Z");
    }

    juce::Path getIconPathForButtonName (const juce::String& name)
    {
        if (name == "apply")
            return makeReloadIconPath();

        if (name == "clear")
            return makeTrashIconPath();

        if (name == "params")
            return makeTuneIconPath();

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
    const float cornerSize = bounds.getHeight() * 0.3f;

    auto colour = backgroundColour;

    if (shouldDrawButtonAsDown)
        colour = colour.darker (0.25f);
    else if (shouldDrawButtonAsHighlighted)
        colour = colour.brighter (0.12f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, cornerSize);
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

    auto bounds = button.getLocalBounds().toFloat().reduced (10.0f, 0.0f);
    auto icon = getIconPathForButtonName (button.getName());

    if (! icon.isEmpty())
    {
        const float iconSize = juce::jmin (bounds.getHeight() * 0.55f, 16.0f);
        auto iconArea = bounds.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize);

        icon.scaleToFit (iconArea.getX(), iconArea.getY(), iconArea.getWidth(), iconArea.getHeight(), true);
        g.fillPath (icon);

        bounds.removeFromLeft (7.0f); // spazio tra icona e testo
    }

    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.drawFittedText (button.getButtonText(), bounds.toNearestInt(), juce::Justification::centredLeft, 1);
}

int CsoundLookAndFeel::getIconAllowance (const juce::String& buttonName)
{
    return getIconPathForButtonName (buttonName).isEmpty() ? 0 : 23; // 16px icona + 7px spaziatura
}
