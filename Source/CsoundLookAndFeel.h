#pragma once

#include <JuceHeader.h>

/**
    Tema chiaro e "cool" per l'editor del plugin: sfondo chiaro, pannelli
    bianchi, accento petrolio/teal sui controlli, console a contrasto scuro
    (stile terminale) per il codice/log. Applicato in CsoundAudioProcessorEditor
    via setLookAndFeel(&lookAndFeel) nel costruttore.
*/
class CsoundLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CsoundLookAndFeel();

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override;

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    /** Extra larghezza (icona + spaziatura) che un bottone con questo nome
        (Component::setName, es. "run"/"clear") occupa oltre al solo testo -
        usato per calcolare dimensioni proporzionate nel layout della toolbar. */
    static int getIconAllowance (const juce::String& buttonName);

    /** Tooltip: scheda bianca squadrata con bordo grigio sottile e testo
        nero 13.5 px, a capo automatico entro kTooltipMaxWidth. */
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override;

private:
    static constexpr int   kTooltipMaxWidth = 340;
    static constexpr float kTooltipFontSize = 13.5f;
    static constexpr int   kTooltipPadX = 10, kTooltipPadY = 7;

    juce::TextLayout layoutTooltipText (const juce::String& text, juce::Colour colour) const;
};
