#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "CsoundChannelDiagnostics.h"

class CsoundAudioProcessor;

/**
    Scheda "Channels" (menu principale > Show Channels, Cmd+4): tutti i
    canali del motore a runtime - nome, tipo (k/S/a/f), direzione (in = lo
    legge chnget, out = lo scrive chnset), valore corrente e origine (chi lo
    scrive). I canali LETTI DA NESSUNO SCRITTI stanno in testa, in giallo:
    sono quasi sempre il motivo per cui "non si muove" (widget non importato,
    slot finiti, nome sbagliato). "Map" crea il parametro nel primo slot
    libero (Float/Int/Bool/Choice).

    Occupa lo stesso posto del pannello Graphs (in basso, accanto alla
    consolle): i due si escludono a vicenda. Elenco rifatto 2 volte al
    secondo, valori 10 volte al secondo, solo mentre e' visibile.
*/
class CsoundChannelsPanel final : public juce::Component,
                                  private juce::Timer
{
public:
    explicit CsoundChannelsPanel (CsoundAudioProcessor& processor);
    ~CsoundChannelsPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    /** Messaggi per la consolle ("--- Mapped x as Float ---"). */
    std::function<void (const juce::String&)> onLog;

private:
    struct Row final : public juce::Component
    {
        Row();
        void paint (juce::Graphics& g) override;
        void resized() override;
        csoundChannels::Info info;
        juce::String value;
        juce::TextButton mapButton { "Map" };
        // Riga "famiglia" (Acc1..Acc16): membri da mappare tutti insieme.
        juce::StringArray familyMembers;
        int buttonWidth() const noexcept { return mapButton.getButtonText().length() > 6 ? 84 : (familyMembers.isEmpty() ? 52 : 72); }
        static constexpr int height = 30;
    };

    void timerCallback() override;
    void refreshList (bool force);
    void refreshValues();
    void showMapMenu (const juce::StringArray& channels, const juce::String& title);

    static constexpr int headerHeight = 22;

    CsoundAudioProcessor& processor;
    juce::Viewport viewport;
    juce::Component list;
    std::vector<std::unique_ptr<Row>> rows;
    csoundChannels::Summary summary;
    juce::String signature;
    std::uint64_t engine = 0;
    int tick = 0;
};
