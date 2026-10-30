#include "CsoundChannelsPanel.h"
#include "PluginProcessor.h"
#include "CsoundActionSheet.h"

namespace
{
    // Stessa palette di Graphs / consolle (CsoundGraphs.cpp).
    const juce::Colour kPanelBg   { 0xff10181f };
    const juce::Colour kRowBg     { 0xff1c2730 };
    const juce::Colour kText      { 0xffe8eef1 };
    const juce::Colour kTextMuted { 0xff8a9aa5 };
    const juce::Colour kAccent    { 0xff4aa3b8 };
    const juce::Colour kWarn      { 0xffe0b84a }; // orfani
    const juce::Colour kParam     { 0xff6fb7d6 };
    const juce::Colour kHost      { 0xff9a8fd6 };

    juce::Colour originColour (csoundChannels::Origin o)
    {
        using csoundChannels::Origin;
        switch (o)
        {
            case Origin::orphan:    return kWarn;
            case Origin::parameter: return kParam;
            case Origin::host:      return kHost;
            default:                return kTextMuted;
        }
    }
}

//==============================================================================
CsoundChannelsPanel::Row::Row()
{
    mapButton.setTooltip ("Create a parameter for this channel (Float, Int, Bool or Menu)");
    addChildComponent (mapButton);
}

void CsoundChannelsPanel::Row::resized()
{
    mapButton.setBounds (getLocalBounds().removeFromRight (buttonWidth()).reduced (4, 5));
}

void CsoundChannelsPanel::Row::paint (juce::Graphics& g)
{
    using namespace csoundChannels;
    auto b = getLocalBounds();
    g.setColour (kRowBg);
    g.fillRect (b.reduced (0, 1));
    g.setColour (originColour (info.origin));
    g.fillRect (b.getX(), b.getY() + 1, 4, b.getHeight() - 2);

    auto area = b.reduced (10, 0).withTrimmedRight (mapButton.isVisible() ? buttonWidth() : 0);

    // tipo + direzione: "k in/out". Per parametri e canali host la
    // direzione la decide l'host (sempre in/out): solo il tipo.
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain)));
    g.setColour (kTextMuted);
    juce::String typeText (typeName (info.type));
    if (info.origin != Origin::parameter && info.origin != Origin::host)
        typeText << " " << (info.input && info.output ? "in/out" : (info.input ? "in" : (info.output ? "out" : "-")));
    g.drawText (typeText, area.removeFromLeft (64), juce::Justification::centredLeft, false);

    // nome (o famiglia "Acc1..Acc16")
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.setColour (kText);
    const int nameW = juce::jlimit (90, 220, area.getWidth() / 3);
    const auto shownName = familyMembers.isEmpty() ? info.name
                                                   : familyMembers[0] + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")) + familyMembers[familyMembers.size() - 1];
    g.drawFittedText (shownName, area.removeFromLeft (nameW), juce::Justification::centredLeft, 1);

    // origine
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.setColour (originColour (info.origin));
    juce::String origin = originName (info.origin);
    if (info.origin == Origin::parameter) origin = "parameter (" + info.parameterKind + ")";
    if (isNeverReadParameter (info))
    {
        // slot sprecato: indicatore GUI (LED/meter) che il codice scrive e basta
        origin << ", never read by code";
        g.setColour (kWarn.withAlpha (0.85f));
    }
    const int originW = juce::jlimit (110, 240, area.getWidth() / 2);
    g.drawFittedText (origin, area.removeFromLeft (originW), juce::Justification::centredLeft, 1);

    // valore
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain)));
    g.setColour (kText);
    g.drawFittedText (value, area, juce::Justification::centredRight, 1);
}

//==============================================================================
CsoundChannelsPanel::CsoundChannelsPanel (CsoundAudioProcessor& p)
    : processor (p)
{
    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    setTitle ("Channels");
}

CsoundChannelsPanel::~CsoundChannelsPanel() = default;

void CsoundChannelsPanel::visibilityChanged()
{
    if (isVisible())
    {
        refreshList (true);
        startTimerHz (10);
    }
    else
    {
        stopTimer();
    }
}

void CsoundChannelsPanel::timerCallback()
{
    if (++tick % 5 == 0)
        refreshList (false);   // 2 Hz: canali nuovi, motore nuovo, parametri cambiati
    refreshValues();           // 10 Hz
}

void CsoundChannelsPanel::refreshList (bool force)
{
    const auto newEngine = processor.getEngineIdentity();
    auto infos = processor.listAllChannels();

    juce::String sig;
    for (const auto& i : infos)
        sig << i.name << '|' << (int) i.type << '|' << (int) i.origin << '|' << i.parameterKind << '\n';

    if (! force && sig == signature && newEngine == engine)
        return;

    signature = sig;
    engine = newEngine;
    summary = csoundChannels::summarize (infos);

    rows.clear();

    // Famiglie (Acc1..Acc16): una riga sola, al posto del primo membro.
    const auto families = csoundChannels::findFamilies (infos);
    std::vector<int> familyOf ((size_t) infos.size(), -1);
    for (int f = 0; f < (int) families.size(); ++f)
        for (int idx : families[(size_t) f].channels)
            familyOf[(size_t) idx] = f;

    std::vector<bool> familyDone (families.size(), false);

    for (int i = 0; i < (int) infos.size(); ++i)
    {
        const auto& info = infos[(size_t) i];
        const int f = familyOf[(size_t) i];
        if (f >= 0 && familyDone[(size_t) f])
            continue;

        auto row = std::make_unique<Row>();
        row->info = info;

        if (f >= 0)
        {
            familyDone[(size_t) f] = true;
            for (int idx : families[(size_t) f].channels)
                row->familyMembers.add (infos[(size_t) idx].name);
            row->info.name = families[(size_t) f].base;
            row->mapButton.setButtonText ("Map " + juce::String (row->familyMembers.size()));
            row->mapButton.setTooltip ("Create one parameter for each channel of the family");
            const auto members = row->familyMembers;
            const auto title = "Map " + members[0] + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa6")) + members[members.size() - 1] + " as";
            row->mapButton.onClick = [this, members, title] { showMapMenu (members, title); };
        }
        else
        {
            const auto name = info.name;
            row->mapButton.onClick = [this, name] { showMapMenu (juce::StringArray (name), "Map \"" + name + "\" as"); };
        }

        if (csoundChannels::isNeverReadParameter (info))
        {
            // "Unmap": libera lo slot (tutta la famiglia, se e' una famiglia).
            juce::StringArray members = row->familyMembers.isEmpty() ? juce::StringArray (info.name) : row->familyMembers;
            row->mapButton.setButtonText (members.size() == 1 ? "Unmap" : "Unmap " + juce::String (members.size()));
            row->mapButton.setTooltip ("Remove the parameter: the code only writes this channel, the slot is wasted");
            row->mapButton.onClick = [this, members]
            {
                int n = 0;
                for (const auto& m : members)
                    if (processor.unmapChannelParameter (m)) ++n;
                if (onLog)
                    onLog ("--- Unmapped " + (members.size() == 1 ? "\"" + members[0] + "\"" : juce::String (n) + " parameter(s)") + " ---");
                refreshList (true);
            };
            row->mapButton.setVisible (true);
        }
        else
        {
            row->mapButton.setVisible (csoundChannels::canMap (info));
        }
        row->setTitle (row->info.name + ", " + csoundChannels::originName (info.origin));
        list.addAndMakeVisible (*row);
        rows.push_back (std::move (row));
    }

    resized();
    repaint();
    refreshValues();
}

void CsoundChannelsPanel::refreshValues()
{
    for (auto& row : rows)
    {
        juce::String v;
        if (row->familyMembers.isEmpty())
        {
            v = processor.readChannelValueText (row->info);
        }
        else
        {
            // famiglia: i valori dei membri, in fila ("1 0 0 1 ...")
            for (const auto& name : row->familyMembers)
            {
                auto member = row->info; member.name = name;
                v << (v.isEmpty() ? "" : " ") << processor.readChannelValueText (member);
            }
        }
        if (v != row->value)
        {
            row->value = v;
            row->repaint();
        }
    }
}

void CsoundChannelsPanel::showMapMenu (const juce::StringArray& channels, const juce::String& title)
{
    std::vector<CsoundActionSheetItem> items;
    const char* const names[] = { "Slider (Float)", "Slider (Int)", "Button", "Menu" };
    const CsoundActionSheetIcon icons[] = { CsoundActionSheetIcon::sliderFloat, CsoundActionSheetIcon::sliderInt,
                                            CsoundActionSheetIcon::toggleSwitch, CsoundActionSheetIcon::comboMenu };
    for (int k = 0; k < 4; ++k)
    {
        CsoundActionSheetItem item;
        item.id = k + 1;
        item.text = names[k];
        item.icon = icons[k];
        items.push_back (item);
    }

    juce::Component::SafePointer<CsoundChannelsPanel> safeThis (this);
    CsoundActionSheet::show (*this, title, std::move (items), [safeThis, channels] (int result)
    {
        if (safeThis == nullptr || result <= 0)
            return;
        const juce::String kindName = result == 1 ? "Float" : result == 2 ? "Int" : result == 3 ? "Bool" : "Menu";
        juce::StringArray failed;
        int mapped = 0;
        for (const auto& channel : channels)
        {
            if (safeThis->processor.mapChannelToParameter (channel, result - 1)) ++mapped;
            else failed.add (channel);
        }
        if (safeThis->onLog)
        {
            if (mapped > 0)
                safeThis->onLog ("--- Mapped " + (channels.size() == 1 ? "\"" + channels[0] + "\"" : juce::String (mapped) + " channel(s)") + " as " + kindName + " parameter" + (mapped == 1 ? "" : "s") + " ---");
            if (! failed.isEmpty())
                safeThis->onLog ("--- Could not map " + failed.joinIntoString (", ") + ": no free " + kindName + " slot, or the name is already used ---");
        }
        safeThis->refreshList (true);
    });
}

void CsoundChannelsPanel::paint (juce::Graphics& g)
{
    g.fillAll (kPanelBg);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (10, 0);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    juce::String text;
    if (summary.total > 0)
    {
        text << summary.total << " channels: " << summary.parameters << " parameters, " << summary.host << " host, "
             << summary.written << " written, " << summary.readWritten << " read & written";
        if (summary.orphans > 0)
            text << ", " << summary.orphans << " read but never written";
    }
    g.setColour (summary.orphans > 0 ? kWarn : kTextMuted);
    g.drawFittedText (text, header, juce::Justification::centredLeft, 1);

    if (rows.empty())
    {
        g.setColour (kTextMuted);
        g.setFont (juce::Font (juce::FontOptions (12.5f)));
        g.drawFittedText ("No channels: the engine is not running, or the code uses no chnget / chnset.",
                          getLocalBounds().withTrimmedTop (headerHeight).reduced (12), juce::Justification::centred, 3);
    }
}

void CsoundChannelsPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop (headerHeight);
    viewport.setBounds (area);

    const int w = juce::jmax (100, area.getWidth() - (viewport.isVerticalScrollBarShown() ? viewport.getScrollBarThickness() : 0));
    int y = 2;
    for (auto& row : rows)
    {
        row->setBounds (4, y, w - 8, Row::height);
        y += Row::height;
    }
    list.setSize (w, y + 2);
}
