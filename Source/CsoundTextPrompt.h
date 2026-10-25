#pragma once

#include <JuceHeader.h>

/**
    Piccolo dialogo di testo nello stile dei menu di apeCsound
    (CsoundActionSheet): velo scuro sull'intera finestra del plugin e una
    scheda centrata con titolo, un campo di testo e due bottoni (Cancel /
    conferma). Usato dove serve un nome dall'utente, es. il nome di uno
    snapshot dei parametri.

    Non un juce::AlertWindow (disegnato da JUCE, non a tema) ne' un dialogo
    nativo (su iOS, dentro un'estensione AUv3, le finestre top-level non si
    possono aprire): e' un figlio della finestra del plugin, come i menu.
    Return conferma, Esc o un tocco sul velo annullano. La scheda sta nel
    terzo superiore, cosi' su iPad resta sopra la tastiera a schermo.

    Si gestisce da solo: show() lo crea e lo mostra, si rimuove alla
    chiusura. onResult riceve il testo confermato (mai vuoto) oppure una
    stringa vuota se annullato.
*/
class CsoundTextPrompt final : public juce::Component
{
public:
    static void show (juce::Component& host, const juce::String& title,
                      const juce::String& initialText, const juce::String& okLabel,
                      std::function<void (const juce::String&)> onResult)
    {
        auto* prompt = new CsoundTextPrompt (title, initialText, okLabel, std::move (onResult));
        host.addAndMakeVisible (prompt);
        prompt->setBounds (host.getLocalBounds());
        prompt->toFront (true);
        prompt->field.grabKeyboardFocus();
        prompt->field.selectAll();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.45f));

        const auto c = cardBounds().toFloat();
        g.setColour (kBg);
        g.fillRoundedRectangle (c, 12.0f);
        g.setColour (kBorder);
        g.drawRoundedRectangle (c.reduced (0.5f), 12.0f, 1.0f);

        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
        g.drawFittedText (title, cardBounds().removeFromTop (48).reduced (20, 0),
                          juce::Justification::centredLeft, 1);
    }

    void resized() override
    {
        auto c = cardBounds();
        c.removeFromTop (48);
        c = c.reduced (20, 0);
        field.setBounds (c.removeFromTop (38));
        c.removeFromTop (14);
        auto buttons = c.removeFromTop (38);
        okButton.setBounds (buttons.removeFromRight (110));
        buttons.removeFromRight (10);
        cancelButton.setBounds (buttons.removeFromRight (100));
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == this && ! cardBounds().contains (e.getPosition()))
            finish ({});
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { finish ({}); return true; }
        return false;
    }

private:
    CsoundTextPrompt (const juce::String& titleIn, const juce::String& initialText,
                      const juce::String& okLabel, std::function<void (const juce::String&)> onResultIn)
        : title (titleIn), onResult (std::move (onResultIn))
    {
        setWantsKeyboardFocus (true);
        setInterceptsMouseClicks (true, true);

        field.setMultiLine (false);
        field.setReturnKeyStartsNewLine (false);
        field.setFont (juce::Font (juce::FontOptions (16.0f)));
        field.setIndents (10, 0);
        field.setJustification (juce::Justification::centredLeft);
        field.setColour (juce::TextEditor::backgroundColourId, kFieldBg);
        field.setColour (juce::TextEditor::textColourId, kText);
        field.setColour (juce::TextEditor::outlineColourId, kBorder);
        field.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
        field.setColour (juce::TextEditor::highlightColourId, kAccent.withAlpha (0.4f));
        field.setColour (juce::CaretComponent::caretColourId, kAccent);
        field.setText (initialText, juce::dontSendNotification);
        field.onReturnKey = [this] { confirm(); };
        field.onEscapeKey = [this] { finish ({}); };
        field.onTextChange = [this] { okButton.setEnabled (field.getText().trim().isNotEmpty()); };
        addAndMakeVisible (field);

        auto styleButton = [] (juce::TextButton& b, juce::Colour bg, juce::Colour fg)
        {
            b.setColour (juce::TextButton::buttonColourId, bg);
            b.setColour (juce::TextButton::textColourOffId, fg);
            b.setColour (juce::TextButton::textColourOnId, fg);
        };

        cancelButton.setButtonText ("Cancel");
        styleButton (cancelButton, kFieldBg, kText);
        cancelButton.onClick = [this] { finish ({}); };
        addAndMakeVisible (cancelButton);

        okButton.setButtonText (okLabel);
        styleButton (okButton, kAccent, juce::Colour (0xff0b1116));
        okButton.setEnabled (initialText.trim().isNotEmpty());
        okButton.onClick = [this] { confirm(); };
        addAndMakeVisible (okButton);
    }

    juce::Rectangle<int> cardBounds() const
    {
        const int w = juce::jmin (380, getWidth() - 32);
        const int h = 48 + 38 + 14 + 38 + 20;
        // Terzo superiore (tastiera a schermo su iOS), ma non sopra la toolbar.
        const int y = juce::jlimit (16, juce::jmax (16, getHeight() - h - 16), getHeight() / 3 - h / 2);
        return { (getWidth() - w) / 2, y, w, h };
    }

    void confirm()
    {
        const auto text = field.getText().trim();
        if (text.isNotEmpty())
            finish (text);
    }

    void finish (const juce::String& result)
    {
        if (finished)
            return;

        finished = true;
        field.giveAwayKeyboardFocus();

        auto callback = std::move (onResult);

        // Rimosso e distrutto in modo asincrono: siamo dentro un evento di
        // un nostro figlio (bottone/campo).
        juce::Component::SafePointer<CsoundTextPrompt> safeThis (this);
        juce::MessageManager::callAsync ([safeThis, callback, result]
        {
            if (safeThis != nullptr)
            {
                if (auto* parent = safeThis->getParentComponent())
                    parent->removeChildComponent (safeThis.getComponent());
                delete safeThis.getComponent();
            }

            if (callback)
                callback (result);
        });
    }

    const juce::Colour kBg      { 0xff141d24 };
    const juce::Colour kBorder  { 0xff2a3540 };
    const juce::Colour kFieldBg { 0xff202a33 };
    const juce::Colour kText    { 0xffe8eef1 };
    const juce::Colour kAccent  { 0xff4aa3b8 };

    juce::String title;
    std::function<void (const juce::String&)> onResult;
    juce::TextEditor field;
    juce::TextButton cancelButton, okButton;
    bool finished = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundTextPrompt)
};
