#pragma once

#include <JuceHeader.h>
#include "CsoundActionSheet.h"
#include <array>
#include <bitset>

/**
    Tastiera MIDI virtuale (menu > Show Keyboard, Cmd+5).

    Suona gli strumenti direttamente dalla UI: le note finiscono nel
    juce::MidiKeyboardState del processore (getVirtualKeyboardState), che
    processBlock fonde nel buffer MIDI del blocco (processNextMidiBuffer)
    prima di passarlo a Csound - esattamente come il MIDI dell'host. I
    device MIDI hardware non vengono toccati e queste note non escono verso
    l'host. Funziona anche al contrario: le note che arrivano dall'host o da
    un controller si illuminano sui tasti.

    Barra in alto: canale MIDI (1-16: con il routing di default di Csound,
    canale N = instr N, o massign), ottava -/+, velocity, All Notes Off.
*/
class CsoundVirtualKeyboardPanel final : public juce::Component,
                                         private juce::MidiKeyboardState::Listener
{
public:
    explicit CsoundVirtualKeyboardPanel (juce::MidiKeyboardState& s)
        : state (s), keys (s, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        const juce::Colour bg     { 0xff10181f };   // stesso fondo della consolle
        const juce::Colour accent { 0xff4aa3b8 };

        keys.setWantsKeyboardFocus (false);          // la tastiera del computer resta all'editor di codice
        // Niente frecce di scorrimento: con le frecce nascoste JUCE riporta
        // sempre la tastiera alla prima nota dell'intervallo, quindi le
        // ottave si cambiano spostando l'intervallo (setAvailableRange, vedi
        // layoutKeys) e non la "prima nota visibile".
        keys.setScrollButtonsVisible (false);
        keys.setOctaveForMiddleC (4);                // 60 = C4, come in Csound (cpsmidinn 60 = 261.6 Hz)
        keys.setColour (juce::MidiKeyboardComponent::whiteNoteColourId,              juce::Colour (0xffe9edf1));
        keys.setColour (juce::MidiKeyboardComponent::blackNoteColourId,              juce::Colour (0xff1a2128));
        keys.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,       juce::Colour (0xff8a9aa5));
        keys.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,    accent.withAlpha (0.25f));
        keys.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,         accent.withAlpha (0.85f));
        keys.setColour (juce::MidiKeyboardComponent::textLabelColourId,              juce::Colour (0xff53616c));
        keys.setColour (juce::MidiKeyboardComponent::shadowColourId,                 juce::Colours::transparentBlack);
        keys.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, bg);
        keys.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId,      accent);
        addAndMakeVisible (keys);

        // Canale MIDI: lo stesso foglio (CsoundActionSheet) di tutti gli
        // altri menu dell'app, su desktop e su iOS.
        setupButton (channelButton, {}, "MIDI channel of the notes (Csound default: channel N plays instr N, or use massign)");
        channelButton.onClick = [this] { showChannelMenu(); };
        setMidiChannel (1);

        setupButton (octDown, "Oct -", "One octave down");
        setupButton (octUp,   "Oct +", "One octave up");
        setupButton (panic,   "All Notes Off", "Release every note played from this keyboard");
        octDown.onClick = [this] { shiftOctave (-1); };
        octUp.onClick   = [this] { shiftOctave (+1); };
        panic.onClick   = [this]
        {
            releaseOwnNotes();
            for (int ch = 1; ch <= 16; ++ch)
                state.allNotesOff (ch);
        };

        rangeLabel.setJustificationType (juce::Justification::centred);
        rangeLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a9aa5));
        addAndMakeVisible (rangeLabel);

        velocityLabel.setText ("Vel", juce::dontSendNotification);
        velocityLabel.setJustificationType (juce::Justification::centredRight);
        velocityLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a9aa5));
        addAndMakeVisible (velocityLabel);

        velocity.setSliderStyle (juce::Slider::LinearHorizontal);
        velocity.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 20);
        velocity.setRange (1.0, 127.0, 1.0);
        velocity.setValue (100.0, juce::dontSendNotification);
        velocity.setColour (juce::Slider::trackColourId, accent);
        velocity.setColour (juce::Slider::thumbColourId, accent.brighter (0.3f));
        velocity.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffe8eef1));
        velocity.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        velocity.setTooltip ("Velocity of the notes (1-127)");
        velocity.onValueChange = [this] { updateVelocityMode(); };
        addAndMakeVisible (velocity);

        // Fixed Vel: acceso = ogni nota esce con la velocity di Vel; spento
        // (default) = la velocity dipende da dove si preme il tasto (in alto
        // piano, in basso forte), con Vel come massimo.
        touchButton.setButtonText ("Fixed Vel");
        touchButton.setClickingTogglesState (true);
        touchButton.setToggleState (false, juce::dontSendNotification);
        touchButton.setTooltip ("On: every note has the velocity set by Vel. Off: the velocity depends on where you press the key (top = soft, bottom = loud), up to Vel.");
        touchButton.setWantsKeyboardFocus (false);
        touchButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff1c2730));
        touchButton.setColour (juce::TextButton::buttonOnColourId, accent);
        touchButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffe8eef1));
        touchButton.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff0b1116));
        touchButton.onClick = [this] { updateVelocityMode(); };
        addAndMakeVisible (touchButton);

        // Ultima nota inviata (nota e velocity reali): si vede cosa riceve Csound.
        lastNoteLabel.setJustificationType (juce::Justification::centredLeft);
        lastNoteLabel.setColour (juce::Label::textColourId, accent);
        addAndMakeVisible (lastNoteLabel);

        updateVelocityMode();
        updateRangeLabel();
        state.addListener (this);
    }

    ~CsoundVirtualKeyboardPanel() override
    {
        state.removeListener (this);
        releaseOwnNotes();   // nessuna nota appesa se la finestra si chiude con un tasto premuto
    }

    void visibilityChanged() override
    {
        if (! isVisible())
            releaseOwnNotes();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff10181f));
        g.setColour (juce::Colour (0xff2a3540));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (1);

        auto bar = r.removeFromTop (barHeight).reduced (8, 3);
        channelButton.setBounds (bar.removeFromLeft (72));
        bar.removeFromLeft (8);
        octDown.setBounds (bar.removeFromLeft (52));
        rangeLabel.setBounds (bar.removeFromLeft (70));
        octUp.setBounds (bar.removeFromLeft (52));

        bar.removeFromLeft (8);

        panic.setBounds (bar.removeFromRight (juce::jmin (110, juce::jmax (0, bar.getWidth() / 4))));
        bar.removeFromRight (8);
        auto vel = bar.removeFromRight (juce::jmin (190, juce::jmax (0, bar.getWidth() - 8)));
        velocityLabel.setBounds (vel.removeFromLeft (30));
        velocity.setBounds (vel);
        bar.removeFromRight (6);
        touchButton.setBounds (bar.removeFromRight (juce::jmin (78, juce::jmax (0, bar.getWidth()))));
        bar.removeFromRight (6);
        lastNoteLabel.setBounds (bar);

        keys.setBounds (r.reduced (4, 0).withTrimmedBottom (4));
        layoutKeys();
    }

    static constexpr int barHeight = 28;
   #if JUCE_IOS || JUCE_ANDROID
    static constexpr int preferredHeight = 150;
   #else
    static constexpr int preferredHeight = 118;
   #endif

    // --- stato salvato nel layout della finestra (EDITOR_VIEW) ---
    int  getMidiChannel() const     { return midiChannel; }
    void setMidiChannel (int ch)
    {
        midiChannel = juce::jlimit (1, 16, ch);
        keys.setMidiChannel (midiChannel);
        channelButton.setButtonText ("Ch " + juce::String (midiChannel) + juce::String::fromUTF8 (" \xe2\x96\xbe"));   // "Ch 1 ▾"
    }
    int  getVelocity() const        { return (int) velocity.getValue(); }
    void setVelocity (int v)        { velocity.setValue (juce::jlimit (1, 127, v), juce::sendNotificationSync); }
    int  getLowestKey() const       { return baseNote; }
    void setLowestKey (int note)    { baseNote = juce::jlimit (0, maxBaseNote, (note / 12) * 12); layoutKeys(); }
    bool getTouchVelocity() const   { return ! touchButton.getToggleState(); }   // bottone "Fixed Vel" spento
    void setTouchVelocity (bool on) { touchButton.setToggleState (! on, juce::dontSendNotification); updateVelocityMode(); }

private:
    void setupButton (juce::TextButton& b, const juce::String& text, const juce::String& tip)
    {
        b.setButtonText (text);
        b.setTooltip (tip);
        b.setWantsKeyboardFocus (false);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c2730));
        b.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe8eef1));
        addAndMakeVisible (b);
    }

    void showChannelMenu()
    {
        std::vector<CsoundActionSheetItem> items;
        for (int ch = 1; ch <= 16; ++ch)
        {
            CsoundActionSheetItem item;
            item.id = ch;
            item.text = "Channel " + juce::String (ch);
            item.ticked = (ch == midiChannel);
            items.push_back (item);
        }

        juce::Component::SafePointer<CsoundVirtualKeyboardPanel> safeThis (this);
        CsoundActionSheet::show (channelButton, "MIDI Channel", std::move (items), [safeThis] (int result)
        {
            if (safeThis != nullptr && result >= 1 && result <= 16)
                safeThis->setMidiChannel (result);
        });
    }

    void shiftOctave (int delta)
    {
        baseNote = juce::jlimit (0, maxBaseNote, baseNote + delta * 12);
        layoutKeys();
    }

    // Le ottave che ci stanno nella larghezza (tasti suonabili col dito su
    // iOS), a partire da baseNote: l'intervallo della tastiera e' proprio
    // [baseNote, baseNote + ottave * 12], cosi' cambiare ottava funziona.
    void layoutKeys()
    {
        const float w = (float) keys.getWidth();
       #if JUCE_IOS || JUCE_ANDROID
        const float minKeyWidth = 30.0f;
       #else
        const float minKeyWidth = 18.0f;
       #endif
        int octaves = 1;
        if (w > 0.0f)
            octaves = juce::jlimit (1, 6, (int) ((w - minKeyWidth) / (minKeyWidth * 7.0f)));

        const int lo = baseNote;
        const int hi = juce::jmin (127, lo + octaves * 12);
        keys.setAvailableRange (lo, hi);

        int whiteKeys = 0;
        for (int n = lo; n <= hi; ++n)
            if (! juce::MidiMessage::isMidiNoteBlack (n))
                ++whiteKeys;
        if (w > 0.0f && whiteKeys > 0)
            keys.setKeyWidth (w / (float) whiteKeys);

        keys.setLowestVisibleKey (lo);
        updateRangeLabel();
    }

    void updateRangeLabel()
    {
        rangeLabel.setText (juce::MidiMessage::getMidiNoteName (baseNote, true, true, 4), juce::dontSendNotification);
        octDown.setEnabled (baseNote > 0);
        octUp.setEnabled (baseNote < maxBaseNote);
    }

    void updateVelocityMode()
    {
        keys.setVelocity ((float) velocity.getValue() / 127.0f, ! touchButton.getToggleState());   // Fixed Vel spento = per posizione
    }

    static constexpr int maxBaseNote = 108;   // C8: almeno un'ottava sopra
    int baseNote = 48;                        // C3

    // Note accese DA QUESTA tastiera: le chiamate dal message thread sono
    // nostre (tasti premuti), quelle dal thread audio sono il MIDI
    // dell'host (processNextMidiBuffer) e non si toccano.
    void handleNoteOn (juce::MidiKeyboardState*, int ch, int note, float noteVelocity) override
    {
        if (juce::MessageManager::existsAndIsCurrentThread() && ch >= 1 && ch <= 16)
        {
            ownNotes[(size_t) (ch - 1)].set ((size_t) (note & 127));
            lastNoteLabel.setText (juce::MidiMessage::getMidiNoteName (note, true, true, 4)
                                     + "  vel " + juce::String (juce::MidiMessage::floatValueToMidiByte (noteVelocity))
                                     + "  ch " + juce::String (ch), juce::dontSendNotification);
        }
    }
    void handleNoteOff (juce::MidiKeyboardState*, int ch, int note, float) override
    {
        if (juce::MessageManager::existsAndIsCurrentThread() && ch >= 1 && ch <= 16)
            ownNotes[(size_t) (ch - 1)].reset ((size_t) (note & 127));
    }
    void releaseOwnNotes()
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (ownNotes[(size_t) ch].test ((size_t) n))
                    state.noteOff (ch + 1, n, 0.0f);   // azzera anche il bit (handleNoteOff)
    }
    std::array<std::bitset<128>, 16> ownNotes;

    juce::MidiKeyboardState& state;
    juce::MidiKeyboardComponent keys;
    juce::TextButton channelButton;
    int midiChannel = 1;
    juce::TextButton octDown, octUp, panic, touchButton;
    juce::Label rangeLabel, velocityLabel, lastNoteLabel;
    juce::Slider velocity;
};
