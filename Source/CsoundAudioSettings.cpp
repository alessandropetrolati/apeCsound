#include "CsoundAudioSettings.h"

namespace
{
    // Palette scura delle schede About/Guide (vedi kAbout* in PluginEditor.cpp).
    const juce::Colour kRowBg      { 0xff1c2730 };
    const juce::Colour kRowPressed { 0xff2a3a48 };
    const juce::Colour kText       { 0xffe8eef1 };
    const juce::Colour kTextMuted  { 0xff8a9aa5 };
    const juce::Colour kAccent     { 0xff4aa3b8 };
    const juce::Colour kSwitchOff  { 0xff3a4855 };

    constexpr int kHeaderHeight = 32;
    constexpr int kRowGap = 4;

   #if JUCE_IOS || JUCE_ANDROID
    constexpr bool kTouchSizes = true;
   #else
    constexpr bool kTouchSizes = false;
   #endif

    float titleFontSize() { return kTouchSizes ? 18.0f : 15.0f; }
    float valueFontSize() { return kTouchSizes ? 17.0f : 14.0f; }
}

//==============================================================================
// Row
CsoundAudioSettings::Row::Row (const juce::String& titleText, const juce::String& valueText, Accessory accessoryType)
    : juce::Button (titleText), title (titleText), value (valueText), accessory (accessoryType)
{
}

void CsoundAudioSettings::Row::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const float alpha = isEnabled() ? 1.0f : 0.45f;
    const bool interactive = isEnabled() && accessory != Accessory::none;

    g.setColour ((interactive && (down || highlighted)) ? kRowPressed : kRowBg);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);

    auto area = getLocalBounds().reduced (14, 0);

    // Freccia "indietro" a sinistra
    if (accessory == Accessory::back)
    {
        auto c = area.removeFromLeft (14).toFloat().withSizeKeepingCentre (8.0f, 15.0f);
        juce::Path p;
        p.startNewSubPath (c.getRight(), c.getY());
        p.lineTo (c.getX(), c.getCentreY());
        p.lineTo (c.getRight(), c.getBottom());
        g.setColour (kAccent.withMultipliedAlpha (alpha));
        g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        area.removeFromLeft (10);
    }

    // Accessorio a destra
    if (accessory == Accessory::chevron)
    {
        auto c = area.removeFromRight (12).toFloat().withSizeKeepingCentre (8.0f, 14.0f);
        juce::Path p;
        p.startNewSubPath (c.getX(), c.getY());
        p.lineTo (c.getRight(), c.getCentreY());
        p.lineTo (c.getX(), c.getBottom());
        g.setColour (kTextMuted.withMultipliedAlpha (alpha));
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        area.removeFromRight (10);
    }
    else if (accessory == Accessory::check)
    {
        auto c = area.removeFromRight (20).toFloat().withSizeKeepingCentre (16.0f, 12.0f);

        if (ticked)
        {
            juce::Path p;
            p.startNewSubPath (c.getX(), c.getCentreY());
            p.lineTo (c.getX() + c.getWidth() * 0.38f, c.getBottom());
            p.lineTo (c.getRight(), c.getY());
            g.setColour (kAccent.withMultipliedAlpha (alpha));
            g.strokePath (p, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        area.removeFromRight (10);
    }
    else if (accessory == Accessory::toggleSwitch)
    {
        const float w = kTouchSizes ? 51.0f : 40.0f, h = kTouchSizes ? 31.0f : 24.0f;
        auto sw = area.removeFromRight ((int) w).toFloat().withSizeKeepingCentre (w, h);
        g.setColour ((ticked ? kAccent : kSwitchOff).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (sw, h * 0.5f);
        const float knob = h - 4.0f;
        const float kx = ticked ? sw.getRight() - knob - 2.0f : sw.getX() + 2.0f;
        g.setColour (juce::Colours::white.withMultipliedAlpha (alpha));
        g.fillEllipse (kx, sw.getCentreY() - knob * 0.5f, knob, knob);
        area.removeFromRight (12);
    }

    // Titolo e valore
    g.setFont (juce::Font (juce::FontOptions (titleFontSize())));
    g.setColour ((accessory == Accessory::back ? kAccent : kText).withMultipliedAlpha (alpha));

    if (value.isNotEmpty())
    {
        const int titleWidth = juce::jmin (area.getWidth() / 2, kTouchSizes ? 190 : 160);
        g.drawFittedText (title, area.removeFromLeft (titleWidth), juce::Justification::centredLeft, 1, 0.85f);
        g.setFont (juce::Font (juce::FontOptions (valueFontSize())));
        g.setColour (kTextMuted.withMultipliedAlpha (alpha));
        g.drawFittedText (value, area, juce::Justification::centredRight, 1, 0.8f);
    }
    else
    {
        g.drawFittedText (title, area, juce::Justification::centredLeft, 1, 0.85f);
    }
}

//==============================================================================
CsoundAudioSettings::CsoundAudioSettings (juce::AudioDeviceManager& dm, int maxInputChannels, int maxOutputChannels,
                                          juce::Value* muteInputValue)
    : deviceManager (dm),
      maxInputs (juce::jmax (0, maxInputChannels)),
      maxOutputs (juce::jmax (1, maxOutputChannels)),
      muteInput (muteInputValue)
{
    deviceManager.addChangeListener (this);
    rebuild();
    startTimer (1000);
}

CsoundAudioSettings::~CsoundAudioSettings()
{
    stopTimer();
    deviceManager.removeChangeListener (this);
}

int CsoundAudioSettings::rowHeight() const noexcept
{
    return kTouchSizes ? 56 : 40;
}

juce::String CsoundAudioSettings::formatRate (double rate)
{
    return juce::String (juce::roundToInt (rate)) + " Hz";
}

//==============================================================================
// Stato e aggiornamenti
juce::String CsoundAudioSettings::stateSignature() const
{
    // Come SLSettings::updateAudioMidiSettings: si ricostruisce solo se lo
    // stato e' davvero cambiato (dispositivi, canali, sr, buffer, MIDI).
    juce::StringArray s;
    s.add (deviceManager.getCurrentAudioDeviceType());

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        s.add (device->getName());
        s.add (device->getActiveOutputChannels().toString (2));
        s.add (device->getActiveInputChannels().toString (2));
        s.add (juce::String (device->getCurrentSampleRate()));
        s.add (juce::String (device->getCurrentBufferSizeSamples()));
    }

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        s.add (type->getDeviceNames (false).joinIntoString ("|"));
        s.add (type->getDeviceNames (true).joinIntoString ("|"));
    }

    for (const auto& info : juce::MidiInput::getAvailableDevices())
        s.add (info.identifier + (deviceManager.isMidiInputDeviceEnabled (info.identifier) ? "+" : "-"));

    if (muteInput != nullptr)
        s.add (muteInput->toString());

    return s.joinIntoString ("\n");
}

void CsoundAudioSettings::changeListenerCallback (juce::ChangeBroadcaster*)
{
    rebuild();
}

void CsoundAudioSettings::timerCallback()
{
    if (stateSignature() != lastSignature)
        rebuild();
}

void CsoundAudioSettings::setupChanged()
{
    if (onSetupChanged)
        onSetupChanged();

    rebuild();
}

void CsoundAudioSettings::showPage (Page newPage)
{
    page = newPage;
    rebuild();
}

//==============================================================================
// Contenuto delle pagine
std::vector<CsoundAudioSettings::Entry> CsoundAudioSettings::buildEntries() const
{
    if (page == Page::main)
        return buildMainPage();

    std::vector<Entry> list;

    auto back = [this] (const juce::String& title)
    {
        Entry e;
        e.title = title;
        e.accessory = Row::Accessory::back;
        e.action = [this] { const_cast<CsoundAudioSettings*> (this)->showPage (Page::main); };
        return e;
    };

    auto choice = [] (const juce::String& title, bool ticked, std::function<void()> action, const juce::String& value = {})
    {
        Entry e;
        e.title = title;
        e.value = value;
        e.accessory = Row::Accessory::check;
        e.ticked = ticked;
        e.action = std::move (action);
        return e;
    };

    auto* self = const_cast<CsoundAudioSettings*> (this);
    auto* device = deviceManager.getCurrentAudioDevice();
    auto* type = deviceManager.getCurrentDeviceTypeObject();

    switch (page)
    {
        case Page::driver:
        {
            list.push_back (back ("Audio Driver"));

            for (auto* t : deviceManager.getAvailableDeviceTypes())
            {
                const auto name = t->getTypeName();
                list.push_back (choice (name, name == deviceManager.getCurrentAudioDeviceType(),
                                        [self, name] { self->selectDriver (name); }));
            }
            break;
        }

        case Page::outputDevice:
        case Page::inputDevice:
        {
            const bool isInput = (page == Page::inputDevice);
            list.push_back (back (isInput ? "Input Device" : "Output Device"));

            const auto setup = deviceManager.getAudioDeviceSetup();
            const auto current = isInput ? setup.inputDeviceName : setup.outputDeviceName;

            if (type != nullptr)
            {
                for (const auto& name : type->getDeviceNames (isInput))
                    list.push_back (choice (name, name == current,
                                            [self, name, isInput] { isInput ? self->selectInputDevice (name)
                                                                            : self->selectOutputDevice (name); }));
            }

            if (isInput)
                list.push_back (choice ("None", current.isEmpty(), [self] { self->selectInputDevice ({}); }));
            break;
        }

        case Page::outputChannels:
        case Page::inputChannels:
        {
            const bool isInput = (page == Page::inputChannels);
            list.push_back (back (isInput ? "Input Channels" : "Output Channels"));

            if (device != nullptr)
            {
                const auto active = isInput ? device->getActiveInputChannels() : device->getActiveOutputChannels();
                const auto names = channelPairNames (isInput);

                for (int pair = 0; pair < names.size(); ++pair)
                    list.push_back (choice (names[pair], active[pair * 2] || active[pair * 2 + 1],
                                            [self, isInput, pair] { self->toggleChannelPair (isInput, pair); }));
            }
            break;
        }

        case Page::sampleRate:
        {
            list.push_back (back ("Sample Rate"));

            if (device != nullptr)
                for (auto rate : device->getAvailableSampleRates())
                    list.push_back (choice (formatRate (rate), std::abs (rate - device->getCurrentSampleRate()) < 1.0,
                                            [self, rate] { self->selectSampleRate (rate); }));
            break;
        }

        case Page::bufferSize:
        {
            list.push_back (back ("Buffer Size"));

            if (device != nullptr)
            {
                const double sr = juce::jmax (1.0, device->getCurrentSampleRate());

                for (auto size : device->getAvailableBufferSizes())
                    list.push_back (choice (juce::String (size) + " samples", size == device->getCurrentBufferSizeSamples(),
                                            [self, size] { self->selectBufferSize (size); },
                                            juce::String (1000.0 * size / sr, 1) + " ms"));
            }
            break;
        }

        case Page::midiInputs:
        {
            list.push_back (back ("MIDI Inputs"));
            const auto devices = juce::MidiInput::getAvailableDevices();

            if (devices.isEmpty())
            {
                Entry e;
                e.title = "No MIDI devices connected";
                list.push_back (e);
            }

            for (const auto& info : devices)
            {
                const auto id = info.identifier;
                list.push_back (choice (info.name, deviceManager.isMidiInputDeviceEnabled (id),
                                        [self, id] { self->toggleMidiInput (id); }));
            }
            break;
        }

        case Page::main:
            break;
    }

    return list;
}

std::vector<CsoundAudioSettings::Entry> CsoundAudioSettings::buildMainPage() const
{
    std::vector<Entry> list;
    auto* self = const_cast<CsoundAudioSettings*> (this);
    auto* device = deviceManager.getCurrentAudioDevice();
    auto* type = deviceManager.getCurrentDeviceTypeObject();

    auto header = [&list] (const juce::String& title)
    {
        Entry e;
        e.isHeader = true;
        e.title = title;
        list.push_back (e);
    };

    // Riga con freccia se c'e' davvero una scelta, altrimenti solo il valore.
    auto row = [&list, self] (const juce::String& title, const juce::String& value, bool hasChoice, Page target)
    {
        Entry e;
        e.title = title;
        e.value = value;
        e.accessory = hasChoice ? Row::Accessory::chevron : Row::Accessory::none;

        if (hasChoice)
            e.action = [self, target] { self->showPage (target); };

        list.push_back (e);
    };

    header ("Audio");

    row ("Audio Driver", deviceManager.getCurrentAudioDeviceType(),
         deviceManager.getAvailableDeviceTypes().size() > 1, Page::driver);

    if (device == nullptr || type == nullptr)
    {
        Entry e;
        e.title = "No audio device open";
        list.push_back (e);
    }
    else
    {
        const auto setup = deviceManager.getAudioDeviceSetup();
        const auto outputs = type->getDeviceNames (false);
        const auto inputs  = type->getDeviceNames (true);

        row ("Output Device", setup.outputDeviceName.isNotEmpty() ? setup.outputDeviceName : device->getName(),
             outputs.size() > 1, Page::outputDevice);

        if (type->hasSeparateInputsAndOutputs() && maxInputs > 0)
            row ("Input Device", setup.inputDeviceName.isNotEmpty() ? setup.inputDeviceName : juce::String ("None"),
                 ! inputs.isEmpty(), Page::inputDevice);

        if (device->getOutputChannelNames().size() > 2)
            row ("Output Channels", channelSummary (false), true, Page::outputChannels);

        if (maxInputs > 0 && device->getInputChannelNames().size() > 2)
            row ("Input Channels", channelSummary (true), true, Page::inputChannels);

        const double sr = device->getCurrentSampleRate();
        row ("Sample Rate", formatRate (sr), device->getAvailableSampleRates().size() > 1, Page::sampleRate);

        const int buffer = device->getCurrentBufferSizeSamples();
        row ("Buffer Size", juce::String (buffer) + " samples, " + juce::String (1000.0 * buffer / juce::jmax (1.0, sr), 1) + " ms",
             device->getAvailableBufferSizes().size() > 1, Page::bufferSize);

        if (muteInput != nullptr)
        {
            Entry e;
            e.title = "Mute audio input";
            e.accessory = Row::Accessory::toggleSwitch;
            e.ticked = (bool) muteInput->getValue();
            e.action = [self] { self->muteInput->setValue (! (bool) self->muteInput->getValue()); self->rebuild(); };
            list.push_back (e);
        }

        if (device->hasControlPanel())
        {
            Entry e;
            e.title = "Control Panel...";
            e.accessory = Row::Accessory::chevron;
            e.action = [self]
            {
                if (auto* d = self->deviceManager.getCurrentAudioDevice())
                    if (d->showControlPanel())
                        self->deviceManager.restartLastAudioDevice();
            };
            list.push_back (e);
        }
    }

    header ("MIDI");

    {
        juce::StringArray enabled;

        for (const auto& info : juce::MidiInput::getAvailableDevices())
            if (deviceManager.isMidiInputDeviceEnabled (info.identifier))
                enabled.add (info.name);

        const bool any = ! juce::MidiInput::getAvailableDevices().isEmpty();
        row ("MIDI Inputs", enabled.isEmpty() ? juce::String (any ? "None" : "No devices") : enabled.joinIntoString (", "),
             any, Page::midiInputs);
    }

    if (juce::BluetoothMidiDevicePairingDialogue::isAvailable())
    {
        Entry e;
        e.title = "Bluetooth MIDI...";
        e.accessory = Row::Accessory::chevron;
        e.action = [self] { self->openBluetoothPairing(); };
        list.push_back (e);
    }

    header ({});

    {
        Entry e;
        e.title = "Reset Audio/MIDI";
        e.value = "default devices";
        e.accessory = Row::Accessory::chevron;
        e.action = [self] { self->resetAudioMidi(); };
        list.push_back (e);
    }

    return list;
}

//==============================================================================
// Canali (coppie stereo, come SLSettings::getChannelNamesAtPairIndex/flipBit)
juce::StringArray CsoundAudioSettings::channelPairNames (bool isInput) const
{
    juce::StringArray result;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto names = isInput ? device->getInputChannelNames() : device->getOutputChannelNames();

        for (int i = 0; i < names.size(); i += 2)
            result.add (i + 1 < names.size() ? names[i] + " + " + names[i + 1] : names[i]);
    }

    return result;
}

juce::String CsoundAudioSettings::channelSummary (bool isInput) const
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
        return {};

    const auto active = isInput ? device->getActiveInputChannels() : device->getActiveOutputChannels();
    juce::StringArray list;

    for (int ch = active.findNextSetBit (0); ch >= 0; ch = active.findNextSetBit (ch + 1))
        list.add (juce::String (ch + 1));

    return list.isEmpty() ? juce::String ("None") : list.joinIntoString (", ");
}

void CsoundAudioSettings::toggleChannelPair (bool isInput, int pairIndex)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    auto& channels = isInput ? setup.inputChannels : setup.outputChannels;

    // Bit per coppia (come SLSettings): una coppia e' attiva se lo e' almeno
    // uno dei suoi due canali.
    juce::BigInteger pairs;

    for (int i = 0; i < 256; i += 2)
        pairs.setBit (i / 2, channels[i] || channels[i + 1]);

    const int maxChannels = isInput ? maxInputs : maxOutputs;
    const int maxPairs = juce::jmax (1, (maxChannels + 1) / 2);
    const int minPairs = isInput ? 0 : 1; // almeno un'uscita

    if (pairs[pairIndex])
    {
        if (pairs.countNumberOfSetBits() > minPairs)
            pairs.setBit (pairIndex, false);
    }
    else
    {
        if (pairs.countNumberOfSetBits() >= maxPairs)
        {
            // al massimo: si toglie la coppia piu' lontana per far posto a quella nuova
            const int first = pairs.findNextSetBit (0);
            pairs.clearBit (pairIndex > first ? first : pairs.getHighestBit());
        }

        pairs.setBit (pairIndex, true);
    }

    channels.clear();

    for (int i = 0; i < 256; ++i)
        channels.setBit (i, pairs[i / 2]);

    if (isInput)
        setup.useDefaultInputChannels = false;
    else
        setup.useDefaultOutputChannels = false;

    deviceManager.setAudioDeviceSetup (setup, true);
    setupChanged();
}

//==============================================================================
// Azioni
void CsoundAudioSettings::selectDriver (const juce::String& typeName)
{
    deviceManager.setCurrentAudioDeviceType (typeName, true);
    page = Page::main;
    setupChanged();
}

void CsoundAudioSettings::selectOutputDevice (const juce::String& name)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.outputDeviceName = name;

   #if JUCE_WINDOWS && JUCE_ASIO
    // ASIO: ingresso e uscita sono lo stesso dispositivo (come in SLSettings).
    if (deviceManager.getCurrentAudioDeviceType().equalsIgnoreCase ("ASIO"))
        setup.inputDeviceName = name;
   #endif

    deviceManager.setAudioDeviceSetup (setup, true);
    page = Page::main;
    setupChanged();
}

void CsoundAudioSettings::selectInputDevice (const juce::String& name)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = name;
    deviceManager.setAudioDeviceSetup (setup, true);
    page = Page::main;
    setupChanged();
}

void CsoundAudioSettings::selectSampleRate (double rate)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.sampleRate = rate;
    deviceManager.setAudioDeviceSetup (setup, true);
    page = Page::main;
    setupChanged();
}

void CsoundAudioSettings::selectBufferSize (int samples)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.bufferSize = samples;
    deviceManager.setAudioDeviceSetup (setup, true);
    page = Page::main;
    setupChanged();
}

void CsoundAudioSettings::toggleMidiInput (const juce::String& identifier)
{
    deviceManager.setMidiInputDeviceEnabled (identifier, ! deviceManager.isMidiInputDeviceEnabled (identifier));
    setupChanged(); // resta nella pagina: selezione multipla
}

void CsoundAudioSettings::openBluetoothPairing()
{
    // Come SLSettings: permesso Bluetooth (Android/iOS) e poi il pannello di sistema.
    juce::Timer::callAfterDelay (110, []
    {
        if (! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::bluetoothMidi))
            juce::RuntimePermissions::request (juce::RuntimePermissions::bluetoothMidi, nullptr);

        if (juce::RuntimePermissions::isGranted (juce::RuntimePermissions::bluetoothMidi))
            juce::BluetoothMidiDevicePairingDialogue::open();
    });
}

void CsoundAudioSettings::resetAudioMidi()
{
    deviceManager.initialiseWithDefaultDevices (maxInputs, maxOutputs);
    page = Page::main;
    setupChanged();
}

//==============================================================================
// Componenti e layout
void CsoundAudioSettings::rebuild()
{
    lastSignature = stateSignature();
    entries = buildEntries();

    rows.clear();

    for (const auto& e : entries)
    {
        if (e.isHeader)
        {
            rows.push_back (nullptr);
            continue;
        }

        auto row = std::make_unique<Row> (e.title, e.value, e.accessory);
        row->ticked = e.ticked;
        row->setEnabled (e.enabled);

        if (e.action != nullptr)
        {
            // Asincrono: l'azione puo' ricostruire le righe, compresa questa.
            auto action = e.action;
            juce::Component::SafePointer<CsoundAudioSettings> safeThis (this);
            row->onClick = [safeThis, action]
            {
                juce::MessageManager::callAsync ([safeThis, action]
                {
                    if (safeThis != nullptr)
                        action();
                });
            };
        }
        else
        {
            row->setInterceptsMouseClicks (false, false);
        }

        addAndMakeVisible (*row);
        rows.push_back (std::move (row));
    }

    resized();
    repaint();

    if (onLayoutChanged)
        onLayoutChanged();
}

int CsoundAudioSettings::getIdealHeight (int) const
{
    int y = 0;

    for (const auto& e : entries)
        y += e.isHeader ? kHeaderHeight : rowHeight() + kRowGap;

    return y + 8;
}

void CsoundAudioSettings::resized()
{
    int y = 0;

    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].isHeader)
        {
            y += kHeaderHeight;
            continue;
        }

        if (rows[i] != nullptr)
            rows[i]->setBounds (0, y, getWidth(), rowHeight());

        y += rowHeight() + kRowGap;
    }
}

void CsoundAudioSettings::paint (juce::Graphics& g)
{
    // Intestazioni di sezione ("AUDIO", "MIDI")
    int y = 0;
    g.setFont (juce::Font (juce::FontOptions (kTouchSizes ? 13.0f : 12.0f, juce::Font::bold)));
    g.setColour (kTextMuted);

    for (const auto& e : entries)
    {
        if (e.isHeader)
        {
            if (e.title.isNotEmpty())
                g.drawText (e.title.toUpperCase(), 6, y, getWidth() - 12, kHeaderHeight - 6,
                            juce::Justification::bottomLeft, true);

            y += kHeaderHeight;
        }
        else
        {
            y += rowHeight() + kRowGap;
        }
    }
}
